#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/nasactl/nasa_base.h"
#include "esphome/components/nasactl/nasa_controller.h"

namespace nasactl {

class NasactlBinarySensor : public esphome::binary_sensor::BinarySensor, public NasaBase {
 public:
  NasactlBinarySensor(const std::string &label, uint16_t message, ControllerMode mode,
                      NasaDevice *device)
      : NasaBase(label, message, mode, device) {}

  void set_parent(NasaController *parent) { parent_ = parent; }

  void on_receive(long value) override {
    publish_state(value != 0);
  }

 private:
  NasaController *parent_{nullptr};
};

}  // namespace nasactl
