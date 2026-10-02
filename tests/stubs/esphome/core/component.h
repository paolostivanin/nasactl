#pragma once
#include "hal.h"
namespace esphome {
namespace setup_priority { constexpr float DATA = 600; }
class Component {
 public:
  virtual ~Component() = default;
  virtual void setup() {}
  virtual void loop() {}
  virtual float get_setup_priority() const { return setup_priority::DATA; }
};
class PollingComponent : public Component {
 public:
  virtual void update() {}
};
}
