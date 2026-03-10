#pragma once

#include "esphome/components/climate/climate.h"
#include "esphome/core/component.h"
#include "esphome/components/nasactl/nasa_base.h"
#include "esphome/components/nasactl/nasa_controller.h"
#include "esphome/components/nasactl/nasa_device.h"

namespace nasactl {

// AC modes: Auto=0, Cool=1, Dry=2, Fan=3, Heat=4 (Samsung NASA)
// Fan speeds: Auto=0, Low=1, Mid=2, High=3, Turbo=4 (Samsung NASA)

class NasactlClimate : public esphome::climate::Climate, public esphome::Component {
 public:
  NasactlClimate() = default;

  void setup() override;
  esphome::climate::ClimateTraits traits() override;

  void set_controller(NasaController *controller) { controller_ = controller; }
  void set_device(NasaDevice *device) { device_ = device; }

  // Called by controller when NASA messages arrive
  void update_power(bool on);
  void update_mode(long value);
  void update_target_temp(float temp);
  void update_current_temp(float temp);
  void update_fan_mode(long value);

 protected:
  void control(const esphome::climate::ClimateCall &call) override;

 private:
  NasaController *controller_{nullptr};
  NasaDevice *device_{nullptr};
  esphome::climate::ClimateMode last_active_mode_{esphome::climate::CLIMATE_MODE_HEAT_COOL};
};

// Helper entities that route NASA messages to the climate entity
class ClimateMessageRouter : public NasaBase {
 public:
  static const uint8_t FIELD_POWER = 0;
  static const uint8_t FIELD_MODE = 1;
  static const uint8_t FIELD_TARGET_TEMP = 2;
  static const uint8_t FIELD_CURRENT_TEMP = 3;
  static const uint8_t FIELD_FAN_MODE = 4;

  ClimateMessageRouter(const std::string &label, uint16_t message, ControllerMode mode,
                       NasaDevice *device, NasactlClimate *climate, uint8_t field)
      : NasaBase(label, message, mode, device), climate_(climate), field_(field) {}

  void on_receive(long value) override;

 private:
  NasactlClimate *climate_;
  uint8_t field_;
};

}  // namespace nasactl
