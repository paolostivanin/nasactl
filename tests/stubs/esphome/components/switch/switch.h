#pragma once
#include <vector>
namespace esphome::switch_ {
class Switch {
 public:
  bool state{};
  std::vector<bool> publications;
  void publish_state(bool value) { state = value; publications.push_back(value); }
 protected:
  virtual void write_state(bool) = 0;
};
}
