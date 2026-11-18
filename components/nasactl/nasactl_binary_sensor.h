#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/nasactl/nasa_base.h"

namespace nasactl {

class NasactlBinarySensor : public esphome::binary_sensor::BinarySensor, public NasaBase {
 public:
  NasactlBinarySensor(const std::string &label, uint16_t message, ControllerMode mode,
                      NasaDevice *device)
      : NasaBase(label, message, mode, device) {}

  void on_receive(long value) override {
    if (value == 0xFF) return;  // "Not available": unsupported codes answer 0xFF.
    publish_state(value != 0);
  }
};

}  // namespace nasactl
