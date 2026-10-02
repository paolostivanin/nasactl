#pragma once
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
namespace esphome::climate {
enum ClimateMode { CLIMATE_MODE_OFF, CLIMATE_MODE_COOL, CLIMATE_MODE_HEAT,
                   CLIMATE_MODE_DRY, CLIMATE_MODE_FAN_ONLY, CLIMATE_MODE_HEAT_COOL };
enum ClimateFanMode { CLIMATE_FAN_AUTO, CLIMATE_FAN_LOW, CLIMATE_FAN_MEDIUM, CLIMATE_FAN_HIGH };
constexpr uint32_t CLIMATE_SUPPORTS_CURRENT_TEMPERATURE = 1;
class ClimateTraits {
 public:
  void add_feature_flags(uint32_t) {}
  void set_visual_min_temperature(float) {}
  void set_visual_max_temperature(float) {}
  void set_visual_temperature_step(float) {}
  void set_supported_modes(std::initializer_list<ClimateMode>) {}
  void set_supported_fan_modes(std::initializer_list<ClimateFanMode>) {}
};
class ClimateCall {
 public:
  std::optional<ClimateMode> mode;
  std::optional<ClimateFanMode> fan;
  std::optional<float> temperature;
  std::string custom_fan;
  const auto &get_mode() const { return mode; }
  const auto &get_fan_mode() const { return fan; }
  const auto &get_target_temperature() const { return temperature; }
  bool has_custom_fan_mode() const { return !custom_fan.empty(); }
  const auto &get_custom_fan_mode() const { return custom_fan; }
};
class Climate {
 public:
  virtual ~Climate() = default;
  virtual ClimateTraits traits() = 0;
  ClimateMode mode = CLIMATE_MODE_OFF;
  float target_temperature = NAN;
  float current_temperature = NAN;
  std::optional<ClimateFanMode> fan_mode;
  std::string custom_fan_mode;
  unsigned publications = 0;
  void publish_state() { publications++; }
  void set_supported_custom_fan_modes(std::initializer_list<const char *>) {}
 protected:
  virtual void control(const ClimateCall &) = 0;
  void set_fan_mode_(ClimateFanMode value) { fan_mode = value; }
  void clear_custom_fan_mode_() { custom_fan_mode.clear(); }
  void set_custom_fan_mode_(const char *value) { custom_fan_mode = value; }
};
}
