#pragma once
#include <string>
#include <vector>
namespace esphome::select {
class Select {
 public:
  struct Traits {
    std::vector<std::string> options;
    const auto &get_options() const { return options; }
  } traits;
  std::string state;
  std::vector<std::string> publications;
  void publish_state(const std::string &value) { state = value; publications.push_back(value); }
 protected:
  virtual void control(const std::string &) = 0;
};
}
