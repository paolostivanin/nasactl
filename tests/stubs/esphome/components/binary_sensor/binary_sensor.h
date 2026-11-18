#pragma once
#include "esphome/components/switch/switch.h"
namespace esphome::binary_sensor {
class BinarySensor {
 public:
  bool state{};
  std::vector<bool> publications;
  void publish_state(bool value) { state = value; publications.push_back(value); }
};
}
