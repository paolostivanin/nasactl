#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/components/nasactl/nasa_base.h"
#include "esphome/components/nasactl/nasa_controller.h"

namespace nasactl {

class NasactlSwitch : public esphome::switch_::Switch, public NasaBase {
 public:
  NasactlSwitch(const std::string &label, uint16_t message, ControllerMode mode,
                NasaDevice *device)
      : NasaBase(label, message, mode, device) {}

  void set_parent(NasaController *parent) { parent_ = parent; }

  void on_receive(long value) override {
    bool state = (value != 0);
    publish_state(state);
  }

 protected:
  void write_state(bool state) override {
    publish_state(state);
    long raw = state ? 1 : 0;
    parent_->write(get_address(), get_message(), raw);
  }

 private:
  NasaController *parent_{nullptr};
};

}  // namespace nasactl
