#pragma once

#include "esphome/components/number/number.h"
#include "esphome/components/nasactl/nasa_base.h"
#include "esphome/components/nasactl/nasa_controller.h"

#include <cmath>

namespace nasactl {

class NasactlNumber : public esphome::number::Number, public NasaBase {
 public:
  NasactlNumber(const std::string &label, uint16_t message, ControllerMode mode,
                NasaDevice *device)
      : NasaBase(label, message, mode, device) {}

  void set_parent(NasaController *parent) { parent_ = parent; }
  void set_divisor(float d) { divisor_ = d; }
  void set_multiplier(float m) { multiplier_ = m; }
  void set_signed(bool s) { signed_ = s; }

  void on_receive(long value) override {
    publish_state(nasa_raw_to_float(value, get_message(), signed_, divisor_, multiplier_));
  }

 protected:
  void control(float value) override {
    publish_state(value);

    float raw = value;
    if (divisor_ != 1.0f) raw *= divisor_;
    if (multiplier_ != 1.0f) raw /= multiplier_;

    parent_->write(get_address(), get_message(), std::lroundf(raw));
  }

 private:
  NasaController *parent_{nullptr};
  float divisor_{1.0f};
  float multiplier_{1.0f};
  bool signed_{false};
};

}  // namespace nasactl
