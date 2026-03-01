#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "esphome/core/component.h"
#include "nasa_base.h"
#include "nasa_client.h"
#include "nasa_device.h"

namespace nasactl {

class NasaController : public esphome::PollingComponent {
 public:
  explicit NasaController(NasaClient *client) : client_(client) {}

  void setup() override;
  void update() override;
  float get_setup_priority() const override { return esphome::setup_priority::DATA - 1; }

  // Device management
  void register_device(NasaDevice *device);

  // Component registration — maps message number to entity
  void register_component(NasaBase *component);

  // Write a value to a device
  void write(const std::string &address, uint16_t message_number, long value);

  // Request a read for specific message numbers
  void read(const std::vector<uint16_t> &message_numbers);
  void read(uint16_t message_number);

  // Debug flags
  void set_debug_log_messages(bool v) { debug_log_messages_ = v; }
  void set_debug_log_undefined(bool v) { debug_log_undefined_ = v; }

  // FSV polling configuration
  void set_fsv_startup_delay(uint32_t ms) { fsv_startup_delay_ = ms; }
  void set_fsv_batch_size(uint32_t n) { fsv_batch_size_ = n; }
  void set_fsv_batch_delay(uint32_t ms) { fsv_batch_delay_ = ms; }
  void set_fsv_interval(uint32_t ms) { fsv_interval_ = ms; }

 private:
  void on_packet_(const Packet &packet);
  void route_message_(const std::string &source_address, const MessageSet &msg);
  void fsv_poll_();
  void fsv_send_batch_();

  NasaClient *client_;

  // address -> device
  std::map<std::string, NasaDevice *> devices_;

  // message_number -> [components]
  std::map<uint16_t, std::vector<NasaBase *>> components_;

  // All registered addresses
  std::set<std::string> addresses_;

  // Debug
  bool debug_log_messages_{false};
  bool debug_log_undefined_{false};

  // FSV polling state
  uint32_t fsv_startup_delay_{5000};
  uint32_t fsv_batch_size_{10};
  uint32_t fsv_batch_delay_{200};
  uint32_t fsv_interval_{0};  // 0 = startup only

  bool fsv_initial_poll_done_{false};
  uint32_t fsv_last_poll_time_{0};
  std::vector<uint16_t> fsv_codes_;           // All FSV message codes to poll
  size_t fsv_batch_index_{0};                 // Current position in fsv_codes_
  uint32_t fsv_last_batch_time_{0};           // For inter-batch delay
  bool fsv_polling_in_progress_{false};
};

}  // namespace nasactl
