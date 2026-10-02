#pragma once
#include <string>
#include <vector>
namespace esphome::text_sensor {
class TextSensor {
 public:
  std::string state;
  std::vector<std::string> publications;
  void publish_state(const std::string &value) { state = value; publications.push_back(value); }
};
}
