#include "nasa_client.h"
#include "esphome/core/log.h"

using esphome::millis;
using esphome::delayMicroseconds;

namespace nasactl {

static const char *const TAG = "nasactl.client";

void NasaClient::setup() {
  if (flow_control_pin_ != nullptr) {
    flow_control_pin_->setup();
    flow_control_pin_->digital_write(false);  // RX mode
  }

  // Register batch dispatcher callback for read requests
  read_dispatcher_.set_callback([this](std::vector<uint16_t> &msg_numbers) {
    auto pkt = Packet::create_read(Address::broadcast(), msg_numbers);
    auto encoded = pkt.encode();
    ESP_LOGD(TAG, "Sending batched read for %zu messages", msg_numbers.size());
    queue_packet(pkt.command.packet_number, std::move(encoded));
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
          // Handle ACKs — remove from retry queue
          if (pkt.command.data_type == DataType::Ack) {
            ack_packet(pkt.command.packet_number);
          }

          // Forward to controller
          if (on_packet_) {
            on_packet_(pkt);
          }
        } else {
          ESP_LOGW(TAG, "Packet decode failed (result=%d), discarding %zu bytes",
                   static_cast<int>(result), rx_buffer_.size());
        }

        rx_buffer_.clear();
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

  // Enforce silence interval after last RX
  if (now - last_rx_time_ < silence_interval_)
    return;

  auto &front = send_queue_.front();

  // Check timeout
  if (front.timeout > 0 && now > front.timeout) {
    if (front.retry_count >= min_retries_) {
      ESP_LOGW(TAG, "Packet id=%d timed out after %d retries", front.id, front.retry_count);
      send_queue_.pop_front();
      return;
    }
  }

  // Check retry interval
  if (front.next_retry > 0 && now < front.next_retry)
    return;

  // Transmit
  before_write_();
  write_array(front.data.data(), front.data.size());
  flush();
  after_write_();

  // If id == 0, fire-and-forget
  if (front.id == 0) {
    send_queue_.pop_front();
    return;
  }

  // Schedule retry
  front.retry_count++;
  front.next_retry = now + retry_interval_;
  if (front.timeout == 0) {
    front.timeout = now + send_timeout_;
  }
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

void NasaClient::send_read(const Address &dest, const std::vector<uint16_t> &message_numbers) {
  for (auto msg_num : message_numbers) {
    read_dispatcher_.push(msg_num);
  }
}

void NasaClient::send_write(const Address &dest, uint16_t message_number, long value) {
  auto pkt = Packet::create_write(dest, message_number, value);
  auto encoded = pkt.encode();
  queue_packet(pkt.command.packet_number, std::move(encoded));
}

void NasaClient::queue_packet(uint8_t id, std::vector<uint8_t> &&data) {
  OutgoingPacket out;
  out.id = id;
  out.data = std::move(data);
  send_queue_.push_back(std::move(out));
}

void NasaClient::ack_packet(uint8_t id) {
  for (auto it = send_queue_.begin(); it != send_queue_.end(); ++it) {
    if (it->id == id) {
      send_queue_.erase(it);
      return;
    }
  }
}

}  // namespace nasactl
