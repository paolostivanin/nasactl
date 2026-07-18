#pragma once

#include <string>
#include <vector>

#include "esphome/components/select/select.h"
#include "esphome/components/nasactl/nasa_base.h"
#include "esphome/components/nasactl/nasa_controller.h"

namespace nasactl {

class NasactlSelect : public esphome::select::Select, public NasaBase {
 public:
  NasactlSelect(const std::string &label, uint16_t message, ControllerMode mode,
                NasaDevice *device)
      : NasaBase(label, message, mode, device) {}

  void set_parent(NasaController *parent) { parent_ = parent; }
  void set_offset(int offset) { offset_ = offset; }

  void on_receive(long value) override {
    int index = static_cast<int>(value) - offset_;
    const auto &options = this->traits.get_options();
    if (index >= 0 && index < static_cast<int>(options.size())) {
      publish_state(std::string(options[index]));
    }
  }

 protected:
  void control(const std::string &value) override {
    publish_state(value);

    const auto &options = this->traits.get_options();
    for (size_t i = 0; i < options.size(); i++) {
      if (std::string(options[i]) == value) {
        long raw = static_cast<long>(i) + offset_;
        parent_->write(get_address(), get_message(), raw);
        return;
      }
    }
  }

 private:
  NasaController *parent_{nullptr};
  int offset_{0};  // Offset between option index and NASA value
};

}  // namespace nasactl
