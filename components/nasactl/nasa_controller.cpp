#include "nasa_controller.h"
#include "esphome/core/log.h"

using esphome::millis;

namespace nasactl {

static const char *const TAG = "nasactl.controller";

void NasaController::setup() {
  // Register receive callback with client
  client_->set_on_packet([this](const Packet &packet) {
    this->on_packet_(packet);
  });

  // Collect all FSV codes from registered components
  for (auto &pair : components_) {
    for (auto *comp : pair.second) {
      if (comp->is_fsv()) {
        // Only add unique codes
        bool found = false;
        for (auto code : fsv_codes_) {
          if (code == comp->get_message()) {
            found = true;
            break;
          }
        }
        if (!found) {
          fsv_codes_.push_back(comp->get_message());
        }
      }
    }
  }

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
  auto it = devices_.find(address);
  if (it == devices_.end()) {
    ESP_LOGW(TAG, "Write to unknown device %s", address.c_str());
    return;
  }

  Address dest = it->second->get_parsed_address();
  ESP_LOGD(TAG, "Write 0x%04X = %ld to %s", message_number, value, address.c_str());
  client_->send_write(dest, message_number, value);
}

void NasaController::read(const std::vector<uint16_t> &message_numbers) {
  client_->send_read(message_numbers);
}

void NasaController::read(uint16_t message_number) {
  std::vector<uint16_t> msgs = {message_number};
  read(msgs);
}

void NasaController::on_packet_(const Packet &packet) {
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
