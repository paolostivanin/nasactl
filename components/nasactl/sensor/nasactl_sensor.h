#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/nasactl/nasa_base.h"
#include "esphome/components/nasactl/nasa_controller.h"
#include "esphome/components/nasactl/nasa_message.h"

namespace nasactl {

class NasactlSensor : public esphome::sensor::Sensor, public NasaBase {
 public:
  NasactlSensor(const std::string &label, uint16_t message, ControllerMode mode,
                NasaDevice *device)
      : NasaBase(label, message, mode, device) {}

  void set_parent(NasaController *parent) { parent_ = parent; }
  void set_divisor(float d) { divisor_ = d; }
  void set_multiplier(float m) { multiplier_ = m; }
  void set_signed(bool s) { signed_ = s; }

  void on_receive(long value) override {
    float result;
    if (signed_) {
      auto type = static_cast<MessageSetType>((get_message() >> 9) & 0x03);
      if (type == MessageSetType::LongVariable) {
        result = static_cast<float>(static_cast<int32_t>(value));
      } else {
        result = static_cast<float>(static_cast<int16_t>(value));
      }
    } else {
      result = static_cast<float>(value);
    }
    if (divisor_ != 1.0f) result /= divisor_;
    if (multiplier_ != 1.0f) result *= multiplier_;
    publish_state(result);
  }

 private:
  NasaController *parent_{nullptr};
  float divisor_{1.0f};
  float multiplier_{1.0f};
  bool signed_{false};
};

}  // namespace nasactl
