#pragma once
#include <vector>
namespace esphome::sensor {
class Sensor {
 public:
  float state{};
  std::vector<float> publications;
  void publish_state(float value) { state = value; publications.push_back(value); }
};
}
