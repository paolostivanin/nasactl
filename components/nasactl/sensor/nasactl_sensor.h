#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/nasactl/nasa_base.h"

namespace nasactl {

class NasactlSensor : public esphome::sensor::Sensor, public NasaBase {
 public:
  NasactlSensor(const std::string &label, uint16_t message, ControllerMode mode,
                NasaDevice *device)
      : NasaBase(label, message, mode, device) {}

  void set_divisor(float d) { divisor_ = d; }
  void set_multiplier(float m) { multiplier_ = m; }
  void set_signed(bool s) { signed_ = s; }

  void on_receive(long value) override {
    publish_state(nasa_raw_to_float(value, get_message(), signed_, divisor_, multiplier_));
  }

 private:
  float divisor_{1.0f};
  float multiplier_{1.0f};
  bool signed_{false};
};

}  // namespace nasactl
