#include "nasa_controller.h"
#include "esphome/core/log.h"

#include <algorithm>

using esphome::millis;

namespace nasactl {

static const char *const TAG = "nasactl.controller";

void NasaController::setup() {
  // Register receive callback with client
  client_->set_on_packet([this](const Packet &packet) {
    this->on_packet_(packet);
  });

  client_->set_on_write_result([this](const Packet &packet, WriteOutcome outcome) {
    on_write_result_(packet, outcome);
  });

  // Collect all FSV codes from registered components (deduped via set)
  std::set<uint16_t> fsv_seen;
  for (auto &pair : components_) {
    for (auto *comp : pair.second) {
      if (comp->is_fsv()) {
        fsv_seen.insert(comp->get_message());
      }
    }
  }
  fsv_codes_.assign(fsv_seen.begin(), fsv_seen.end());

  fsv_setup_time_ = millis();

  if (!fsv_codes_.empty()) {
    ESP_LOGI(TAG, "Registered %zu FSV codes for auto-polling", fsv_codes_.size());
  }

  // Also do an initial read of ALL registered message codes (not just FSV)
  std::vector<uint16_t> all_codes;
  for (auto &pair : components_) {
    all_codes.push_back(pair.first);
  }
  if (!all_codes.empty()) {
    ESP_LOGI(TAG, "Queuing initial read for %zu message codes", all_codes.size());
    read(all_codes);
  }
}

void NasaController::loop() {
  readback_poll_();
}

void NasaController::update() {
  // This is called periodically by PollingComponent
  fsv_poll_();
}

void NasaController::register_device(NasaDevice *device) {
  devices_[device->get_address()] = device;
  addresses_.insert(device->get_address());
  ESP_LOGD(TAG, "Registered device at %s (%s)",
           device->get_address().c_str(),
           address_class_to_string(static_cast<AddressClass>(device->get_address_class())));
}

void NasaController::register_component(NasaBase *component) {
  uint16_t msg = component->get_message();
  components_[msg].push_back(component);
  ESP_LOGV(TAG, "Registered component '%s' for message 0x%04X",
           component->get_label().c_str(), msg);
}

void NasaController::write(const std::string &address, uint16_t message_number, long value) {
  MessageSet message(message_number);
  message.value = value;
  write(address, {message});
}

bool NasaController::is_fsv_(const std::string &address, uint16_t code) const {
  auto it = components_.find(code);
  if (it == components_.end()) return false;
  for (auto *component : it->second) {
    if (component->get_address() == address && component->is_fsv()) return true;
  }
  return false;
}

void NasaController::write(const std::string &address, const std::vector<MessageSet> &messages) {
  auto it = devices_.find(address);
  if (it == devices_.end()) {
    ESP_LOGW(TAG, "Write to unknown device %s", address.c_str());
    return;
  }
  // FSV writes retain Write semantics, even when normal control uses Request.
  // Split mixed categories rather than sending one packet with ambiguous semantics.
  std::vector<MessageSet> control, fsv;
  for (const auto &message : messages) {
    (is_fsv_(address, message.message_number) ? fsv : control).push_back(message);
  }
  const auto &dest = it->second->get_parsed_address();
  if (!control.empty()) client_->send_write(dest, control, it->second->get_control_data_type());
  if (!fsv.empty()) client_->send_write(dest, fsv, DataType::Write);
}

bool NasaController::read(const std::string &address, const std::vector<uint16_t> &message_numbers,
                          uint32_t queue_lifetime) {
  auto it = devices_.find(address);
  if (it == devices_.end()) return false;
  const Address dest = it->second->get_targeted_reads() ?
                       it->second->get_parsed_address() : Address::broadcast();
  return client_->send_read(dest, message_numbers, queue_lifetime);
}

void NasaController::on_write_result_(const Packet &packet, WriteOutcome outcome) {
  const std::string address = packet.destination.to_string();
  auto device = devices_.find(address);
  if (device == devices_.end()) return;
  std::set<uint16_t> codes;
  for (const auto &message : packet.messages) codes.insert(message.message_number);
  if (device->second->has_climate()) {
    codes.insert({0x4000, 0x4001, 0x4006, 0x4201});
  }
  uint32_t now = millis();
  for (auto code : codes) {
    ReadbackKey key{address, code};
    if (readbacks_.find(key) == readbacks_.end() && readbacks_.size() >= MAX_READBACK_CODES) {
      client_->record_readback_drop();
      ESP_LOGW(TAG, "Readback capacity reached: %s code=0x%04X", address.c_str(), code);
      continue;
    }
    // A later result supersedes that code's previous readback window.
    readbacks_[key] = {now, now + 3000, 0, false};
  }
  ESP_LOGD(TAG, "Scheduled readback for %s after write outcome=%u", address.c_str(),
           static_cast<unsigned>(outcome));
}

void NasaController::readback_poll_() {
  uint32_t now = millis();
  std::map<std::string, std::vector<uint16_t>> due;
  for (auto it = readbacks_.begin(); it != readbacks_.end();) {
    auto &readback = it->second;
    if (now - readback.started >= READBACK_LIFETIME) {
      if (!readback.observed) {
        ESP_LOGW(TAG, "No fresh report after write: %s code=0x%04X attempts=%u",
                 it->first.first.c_str(), it->first.second, readback.attempt);
      }
      it = readbacks_.erase(it);
      continue;
    }
    // Once the second window starts, skip any first attempt that never fit the queue.
    if (readback.attempt == 0 && now - readback.started >= 15000) {
      readback.attempt = 1;
      readback.retry_at = readback.started + 15000;
    }
    if (readback.attempt < 2 && static_cast<int32_t>(now - readback.retry_at) >= 0) {
      due[it->first.first].push_back(it->first.second);
    }
    ++it;
  }
  for (const auto &device : due) {
    // Keep packets small and destination-specific; admission of each batch is atomic.
    for (size_t offset = 0; offset < device.second.size(); offset += 10) {
      auto end = std::min(device.second.size(), offset + 10);
      std::vector<uint16_t> codes(device.second.begin() + offset, device.second.begin() + end);
      uint32_t lifetime = 5000;
      for (auto code : codes) {
        const auto &r = readbacks_.at({device.first, code});
        lifetime = std::min(lifetime, READBACK_LIFETIME - (now - r.started));
      }
      bool accepted = read(device.first, codes, lifetime);
      for (auto code : codes) {
        auto &r = readbacks_.at({device.first, code});
        if (accepted) {
          r.attempt++;
          r.retry_at = r.started + 15000;
        } else {
          r.retry_at = now + 200;
        }
      }
    }
  }
}

void NasaController::read(const std::vector<uint16_t> &message_numbers) {
  client_->send_read(message_numbers);
}

void NasaController::read(uint16_t message_number) {
  std::vector<uint16_t> msgs = {message_number};
  read(msgs);
}

void NasaController::on_packet_(const Packet &packet) {
  // ACK payloads and other controllers' commands are not device state reports.
  if (packet.command.data_type != DataType::Response &&
      packet.command.data_type != DataType::Notification) return;
  std::string src = packet.source.to_string();

  // Only process packets from known devices (or discover new ones)
  if (addresses_.find(src) == addresses_.end()) {
    // Track discovered but unconfigured addresses
    return;
  }

  for (const auto &msg : packet.messages) {
    if (debug_log_messages_) {
      ESP_LOGD(TAG, "[%s] msg 0x%04X = %ld", src.c_str(), msg.message_number, msg.value);
    }
    auto readback = readbacks_.find({src, msg.message_number});
    if (readback != readbacks_.end()) readback->second.observed = true;
    route_message_(src, msg);
  }
}

void NasaController::route_message_(const std::string &source_address, const MessageSet &msg) {
  auto it = components_.find(msg.message_number);
  if (it == components_.end()) {
    if (debug_log_undefined_) {
      ESP_LOGD(TAG, "Unhandled message 0x%04X = %ld from %s",
               msg.message_number, msg.value, source_address.c_str());
    }
    return;
  }

  for (auto *comp : it->second) {
    if (comp->get_address() == source_address) {
      comp->on_receive(msg.value);
    }
  }
}

void NasaController::fsv_poll_() {
  if (fsv_codes_.empty())
    return;

  uint32_t now = millis();

  // Wait for startup delay (uses subtraction for millis() wraparound safety)
  if (!fsv_initial_poll_done_ && (now - fsv_setup_time_) < fsv_startup_delay_)
    return;

  // Start initial poll
  if (!fsv_initial_poll_done_ && !fsv_polling_in_progress_) {
    ESP_LOGI(TAG, "Starting FSV initial poll of %zu codes", fsv_codes_.size());
    fsv_polling_in_progress_ = true;
    fsv_batch_index_ = 0;
    fsv_last_batch_time_ = now;
    fsv_send_batch_();
    return;
  }

  // Continue sending batches during active polling
  if (fsv_polling_in_progress_) {
    if (fsv_batch_index_ >= fsv_codes_.size()) {
      // All batches sent
      fsv_polling_in_progress_ = false;
      fsv_initial_poll_done_ = true;
      fsv_last_poll_time_ = now;
      ESP_LOGI(TAG, "FSV poll complete");
      return;
    }

    if (now - fsv_last_batch_time_ >= fsv_batch_delay_) {
      fsv_send_batch_();
      fsv_last_batch_time_ = now;
    }
    return;
  }

  // Periodic re-poll
  if (fsv_interval_ > 0 && (now - fsv_last_poll_time_) >= fsv_interval_) {
    ESP_LOGD(TAG, "Starting periodic FSV re-poll");
    fsv_polling_in_progress_ = true;
    fsv_batch_index_ = 0;
    fsv_last_batch_time_ = now;
    fsv_send_batch_();
  }
}

void NasaController::fsv_send_batch_() {
  std::vector<uint16_t> batch;

  for (size_t i = 0; i < fsv_batch_size_ && fsv_batch_index_ < fsv_codes_.size();
       i++, fsv_batch_index_++) {
    batch.push_back(fsv_codes_[fsv_batch_index_]);
  }

  if (!batch.empty()) {
    ESP_LOGD(TAG, "Sending FSV batch: %zu codes (offset %zu/%zu)",
             batch.size(), fsv_batch_index_, fsv_codes_.size());
    read(batch);
  }
}

}  // namespace nasactl
