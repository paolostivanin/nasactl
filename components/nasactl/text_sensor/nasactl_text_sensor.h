#pragma once

#include <map>
#include <string>

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/nasactl/nasa_base.h"

namespace nasactl {

class NasactlTextSensor : public esphome::text_sensor::TextSensor, public NasaBase {
 public:
  NasactlTextSensor(const std::string &label, uint16_t message, ControllerMode mode,
                    NasaDevice *device)
      : NasaBase(label, message, mode, device) {}

  void add_mapping(long key, const std::string &text) {
    mapping_[key] = text;
  }

  void on_receive(long value) override {
    if (value == last_value_)
      return;  // Skip duplicate updates

    last_value_ = value;

    if (!mapping_.empty()) {
      auto it = mapping_.find(value);
      if (it != mapping_.end()) {
        publish_state(it->second);
      } else {
        publish_state("Unknown (" + std::to_string(value) + ")");
      }
    } else {
      publish_state(std::to_string(value));
    }
  }

 private:
  std::map<long, std::string> mapping_;
  long last_value_{-999999};
};

}  // namespace nasactl
