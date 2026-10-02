#include "nasa_client.h"
#include "esphome/core/log.h"

#include <algorithm>

using esphome::millis;
using esphome::delayMicroseconds;

namespace nasactl {

static const char *const TAG = "nasactl.client";

void NasaClient::setup() {
  if (flow_control_pin_ != nullptr) {
    flow_control_pin_->setup();
    flow_control_pin_->digital_write(false);  // RX mode
  }

  rx_buffer_.reserve(256);
  if (tx_timeouts_sensor_) tx_timeouts_sensor_->publish_state(0);
  if (tx_nacks_sensor_) tx_nacks_sensor_->publish_state(0);
  if (tx_queue_drops_sensor_) tx_queue_drops_sensor_->publish_state(0);

  // Register batch dispatcher callback for read requests
  read_dispatcher_.set_callback([this](std::vector<uint16_t> &msg_numbers) {
    send_read(Address::broadcast(), msg_numbers);
  });
}

void NasaClient::loop() {
  uint32_t now = millis();
  read_data_();
  write_data_();
  read_dispatcher_.update(now);
}

void NasaClient::read_data_() {
  while (available()) {
    uint8_t byte;
    read_byte(&byte);
    last_rx_time_ = millis();

    if (rx_buffer_.empty() && byte != PACKET_START) {
      continue;  // Wait for start marker
    }

    rx_buffer_.push_back(byte);

    // Check if we have enough data to read size
    if (rx_buffer_.size() >= 3) {
      uint16_t expected_size = (static_cast<uint16_t>(rx_buffer_[1]) << 8) | rx_buffer_[2];
      uint32_t expected_frame_len = expected_size + 2;  // start + size bytes + ... + end

      if (rx_buffer_.size() >= expected_frame_len) {
        // Try to decode
        Packet pkt;
        auto result = pkt.decode(rx_buffer_);

        if (result == DecodeResult::Ok) {
          // Capture all decoded packets, including foreign and empty ACKs.
          if (debug_log_packets_) {
            std::vector<uint8_t> frame(rx_buffer_.begin(), rx_buffer_.begin() + expected_frame_len);
            log_packet_("RX", pkt, frame);
          }
          ack_packet(pkt);

          // Forward to controller
          if (on_packet_) {
            on_packet_(pkt);
          }

          // Remove only the consumed bytes, preserving any trailing data
          // from the next packet that may have already arrived.
          rx_buffer_.erase(rx_buffer_.begin(),
                           rx_buffer_.begin() + expected_frame_len);
        } else {
          if (debug_log_packets_) log_packet_("RX invalid", pkt, rx_buffer_);
          ESP_LOGW(TAG, "Packet decode failed (result=%d), discarding %zu bytes",
                   static_cast<int>(result), rx_buffer_.size());
          rx_buffer_.clear();
        }
      }
    }

    // Prevent buffer overflow
    if (rx_buffer_.size() > PACKET_MAX_SIZE) {
      ESP_LOGW(TAG, "RX buffer overflow, clearing");
      rx_buffer_.clear();
    }
  }
}

void NasaClient::write_data_() {
  if (send_queue_.empty())
    return;

  uint32_t now = millis();

  auto &front = send_queue_.front();
  // Expiry does not depend on bus silence; incoming traffic must not hide a timeout.
  if (front.queue_lifetime > 0 && now - front.queued_at >= front.queue_lifetime) {
    ESP_LOGW(TAG, "Queued read expired: %s", front.packet.to_string().c_str());
    tx_timeouts_++;
    if (tx_timeouts_sensor_) tx_timeouts_sensor_->publish_state(tx_timeouts_);
    send_queue_.pop_front();
    return;
  }
  if (front.wait_for_ack && front.send_count > 0 && front.send_count >= min_retries_ &&
      now - front.first_send >= send_timeout_) {
    finish_write_(WriteOutcome::TimedOut);
    return;
  }

  if (now - last_rx_time_ < silence_interval_)
    return;
  if (front.send_count > 0 && static_cast<int32_t>(now - front.next_retry) < 0)
    return;

  if (debug_log_packets_) log_packet_("TX", front.packet, front.data);
  before_write_();
  write_array(front.data.data(), front.data.size());
  flush();
  after_write_();

  if (!front.wait_for_ack) {
    send_queue_.pop_front();
    return;
  }
  if (front.send_count == 0) front.first_send = now;
  front.send_count++;
  front.next_retry = now + retry_interval_;
}

void NasaClient::before_write_() {
  if (flow_control_pin_ != nullptr) {
    flow_control_pin_->digital_write(true);  // TX mode
    delayMicroseconds(50);
  }
}

void NasaClient::after_write_() {
  if (flow_control_pin_ != nullptr) {
    delayMicroseconds(50);
    flow_control_pin_->digital_write(false);  // RX mode
  }
}

bool NasaClient::send_read(const std::vector<uint16_t> &message_numbers) {
  bool accepted = true;
  for (auto msg_num : message_numbers) {
    if (!read_dispatcher_.push(msg_num)) {
      record_queue_drop_();
      ESP_LOGW(TAG, "Read dispatcher full: code=0x%04X", msg_num);
      accepted = false;
    }
  }
  return accepted;
}

bool NasaClient::send_read(const Address &dest, const std::vector<uint16_t> &message_numbers,
                           uint32_t queue_lifetime) {
  if (message_numbers.empty()) return true;
  return queue_packet(Packet::create_read(dest, message_numbers), false, queue_lifetime);
}

bool NasaClient::send_write(const Address &dest, uint16_t message_number, long value) {
  MessageSet message(message_number);
  message.value = value;
  return send_write(dest, {message});
}

bool NasaClient::send_write(const Address &dest, const std::vector<MessageSet> &messages,
                            DataType data_type) {
  if (messages.empty()) return true;
  auto packet = Packet::create_write(dest, messages);
  packet.command.data_type = data_type;
  return queue_packet(std::move(packet), true);
}

bool NasaClient::queue_packet(Packet packet, bool wait_for_ack, uint32_t queue_lifetime) {
  packet.command.packet_information = packet_information_;
  // Numbers wrap at 255. Never assign a number already held by a queued packet.
  auto in_use = [this](uint8_t number) {
    return std::any_of(send_queue_.begin(), send_queue_.end(), [number](const OutgoingPacket &p) {
      return p.packet.command.packet_number == number;
    });
  };
  while (packet.command.packet_number == 0 || in_use(packet.command.packet_number)) {
    packet.command.packet_number = Command::next_packet_number();
  }
  OutgoingPacket out;
  out.packet = std::move(packet);
  out.data = out.packet.encode(standard_command_header_);
  out.wait_for_ack = wait_for_ack;
  out.queued_at = millis();
  out.queue_lifetime = queue_lifetime;
  return queue_outgoing_(std::move(out));
}

bool NasaClient::queue_outgoing_(OutgoingPacket &&out) {
  if (send_queue_.size() >= MAX_SEND_QUEUE_SIZE || out.packet.messages.size() > 255 ||
      out.data.size() > PACKET_MAX_SIZE + 2) {
    record_queue_drop_();
    ESP_LOGW(TAG, "Send queue rejected packet: %s", out.packet.to_string().c_str());
    if (out.wait_for_ack) notify_write_result_(out, WriteOutcome::QueueRejected);
    return false;
  }
  send_queue_.push_back(std::move(out));
  ESP_LOGD(TAG, "Queued %s", send_queue_.back().packet.to_string().c_str());
  return true;
}

bool NasaClient::queue_packet(uint8_t id, std::vector<uint8_t> &&data) {
  Packet packet;
  if (packet.decode(data) != DecodeResult::Ok) {
    record_queue_drop_();
    ESP_LOGW(TAG, "Cannot queue invalid raw packet");
    return false;
  }
  OutgoingPacket out;
  out.packet = std::move(packet);
  out.data = std::move(data);  // Preserve the caller's raw header and CRC exactly.
  out.wait_for_ack = id != 0;
  out.queued_at = millis();
  bool duplicate = std::any_of(send_queue_.begin(), send_queue_.end(), [&out](const OutgoingPacket &p) {
    return p.packet.command.packet_number == out.packet.command.packet_number;
  });
  if ((id != 0 && id != out.packet.command.packet_number) || duplicate) {
    record_queue_drop_();
    if (out.wait_for_ack) notify_write_result_(out, WriteOutcome::QueueRejected);
    return false;
  }
  return queue_outgoing_(std::move(out));
}

void NasaClient::ack_packet(const Packet &packet) {
  if (send_queue_.empty() ||
      (packet.command.data_type != DataType::Ack && packet.command.data_type != DataType::Nack))
    return;
  const auto &front = send_queue_.front();
  if (!front.wait_for_ack || front.send_count == 0 ||
      packet.destination != Address::my_address() || packet.source != front.packet.destination ||
      packet.command.packet_number != front.packet.command.packet_number)
    return;
  finish_write_(packet.command.data_type == DataType::Ack ?
                WriteOutcome::Acknowledged : WriteOutcome::Rejected);
}

void NasaClient::finish_write_(WriteOutcome outcome) {
  // Remove first: a result callback may enqueue new work.
  auto out = std::move(send_queue_.front());
  send_queue_.pop_front();
  if (outcome == WriteOutcome::TimedOut) {
    tx_timeouts_++;
    if (tx_timeouts_sensor_) tx_timeouts_sensor_->publish_state(tx_timeouts_);
  } else if (outcome == WriteOutcome::Rejected) {
    tx_nacks_++;
    if (tx_nacks_sensor_) tx_nacks_sensor_->publish_state(tx_nacks_);
  }
  notify_write_result_(out, outcome);
}

void NasaClient::notify_write_result_(const OutgoingPacket &out, WriteOutcome outcome) {
  const char *reason = "ACK";
  switch (outcome) {
    case WriteOutcome::Acknowledged: break;
    case WriteOutcome::Rejected: reason = "NACK"; break;
    case WriteOutcome::TimedOut: reason = "timeout"; break;
    case WriteOutcome::QueueRejected: reason = "queue rejection"; break;
  }
  if (outcome != WriteOutcome::Acknowledged) {
    ESP_LOGW(TAG, "Write %s: %s sends=%u", reason, out.packet.to_string().c_str(), out.send_count);
  } else {
    ESP_LOGD(TAG, "Write ACK: %s sends=%u", out.packet.to_string().c_str(), out.send_count);
  }
  if (on_write_result_) on_write_result_(out.packet, outcome);
}

void NasaClient::record_queue_drop_() {
  tx_queue_drops_++;
  if (tx_queue_drops_sensor_) tx_queue_drops_sensor_->publish_state(tx_queue_drops_);
}

void NasaClient::log_packet_(const char *direction, const Packet &packet,
                            const std::vector<uint8_t> &data) const {
  Command wire_command;
  uint32_t command_offset = 9;
  wire_command.decode(data, command_offset);
  ESP_LOGD(TAG, "%s %s info=%d version=%u retry=%u", direction, packet.to_string().c_str(),
           wire_command.packet_information, wire_command.protocol_version, wire_command.retry_count);
  // Bound individual log lines; preserve every byte for capture fixtures.
  for (size_t offset = 0; offset < data.size(); offset += 32) {
    char hex[32 * 3 + 1];
    size_t count = std::min<size_t>(32, data.size() - offset);
    for (size_t i = 0; i < count; i++) snprintf(hex + i * 3, 4, "%02X ", data[offset + i]);
    ESP_LOGD(TAG, "%s bytes[%zu]: %s", direction, offset, hex);
  }
}

}  // namespace nasactl
