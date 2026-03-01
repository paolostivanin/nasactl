#pragma once

#include <cstdint>
#include <string>
#include "nasa_device.h"

namespace nasactl {

// Controller mode — determines message category
enum class ControllerMode : uint8_t {
  Status = 0,   // Read-only status
  Control = 1,  // Read/write control
  FSV = 2,      // Field Setting Value (polled)
};

// Base class for all NASA entities (sensors, numbers, selects, etc.)
class NasaBase {
 public:
  NasaBase() = default;
  NasaBase(const std::string &label, uint16_t message, ControllerMode mode, NasaDevice *device)
      : label_(label), message_(message), mode_(mode), device_(device) {}

  virtual ~NasaBase() = default;
  virtual void on_receive(long value) = 0;

  const std::string &get_label() const { return label_; }
  uint16_t get_message() const { return message_; }
  ControllerMode get_mode() const { return mode_; }
  NasaDevice *get_device() const { return device_; }
  const std::string &get_address() const { return device_->get_address(); }

  bool is_fsv() const { return mode_ == ControllerMode::FSV; }

 protected:
  std::string label_;
  uint16_t message_{0};
  ControllerMode mode_{ControllerMode::Status};
  NasaDevice *device_{nullptr};
};

// Base class for writable entities (numbers, selects, switches)
class NasaWrite : public virtual NasaBase {
 public:
  virtual void write_raw(long value) = 0;
};

}  // namespace nasactl
