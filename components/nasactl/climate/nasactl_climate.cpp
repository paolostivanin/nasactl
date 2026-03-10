#include "climate/nasactl_climate.h"
#include "esphome/core/log.h"

namespace nasactl {

static const char *const TAG = "nasactl.climate";

// Samsung NASA mode values
static const long NASA_MODE_AUTO = 0;
static const long NASA_MODE_COOL = 1;
static const long NASA_MODE_DRY = 2;
static const long NASA_MODE_FAN = 3;
static const long NASA_MODE_HEAT = 4;

// Samsung NASA fan mode values
static const long NASA_FAN_AUTO = 0;
static const long NASA_FAN_LOW = 1;
static const long NASA_FAN_MID = 2;
static const long NASA_FAN_HIGH = 3;
static const long NASA_FAN_TURBO = 4;

static const char *const CUSTOM_FAN_TURBO = "Turbo";

void NasactlClimate::setup() {
  // Start with unknown state
  this->mode = esphome::climate::CLIMATE_MODE_OFF;
  this->target_temperature = 22.0f;
  this->current_temperature = NAN;

  // Register message routers with controller
  if (controller_ && device_) {
    auto make_router = [&](const char *name, uint16_t code, ControllerMode cm, uint8_t field) {
      auto *r = new ClimateMessageRouter(name, code, cm, device_, this, field);
      controller_->register_component(r);
    };
    make_router("power",        0x4000, ControllerMode::Control, ClimateMessageRouter::FIELD_POWER);
    make_router("mode",         0x4001, ControllerMode::Control, ClimateMessageRouter::FIELD_MODE);
    make_router("target_temp",  0x4201, ControllerMode::Control, ClimateMessageRouter::FIELD_TARGET_TEMP);
    make_router("current_temp", 0x4204, ControllerMode::Status,  ClimateMessageRouter::FIELD_CURRENT_TEMP);
    make_router("fan_mode",     0x4006, ControllerMode::Control, ClimateMessageRouter::FIELD_FAN_MODE);
  }
}

esphome::climate::ClimateTraits NasactlClimate::traits() {
  auto traits = esphome::climate::ClimateTraits();

  traits.add_feature_flags(esphome::climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  traits.set_visual_min_temperature(16.0f);
  traits.set_visual_max_temperature(30.0f);
  traits.set_visual_temperature_step(0.5f);

  // Modes: off + cool, heat, dry, fan_only, auto
  traits.set_supported_modes({
      esphome::climate::CLIMATE_MODE_OFF,
      esphome::climate::CLIMATE_MODE_COOL,
      esphome::climate::CLIMATE_MODE_HEAT,
      esphome::climate::CLIMATE_MODE_DRY,
      esphome::climate::CLIMATE_MODE_FAN_ONLY,
      esphome::climate::CLIMATE_MODE_HEAT_COOL,  // = Auto
  });

  // Fan speeds: auto, low, medium, high + custom "Turbo"
  traits.set_supported_fan_modes({
      esphome::climate::CLIMATE_FAN_AUTO,
      esphome::climate::CLIMATE_FAN_LOW,
      esphome::climate::CLIMATE_FAN_MEDIUM,
      esphome::climate::CLIMATE_FAN_HIGH,
  });
  traits.set_supported_custom_fan_modes({CUSTOM_FAN_TURBO});

  return traits;
}

void NasactlClimate::control(const esphome::climate::ClimateCall &call) {
  if (!controller_ || !device_)
    return;

  const std::string &addr = device_->get_address();

  if (call.get_mode().has_value()) {
    auto mode = *call.get_mode();

    if (mode == esphome::climate::CLIMATE_MODE_OFF) {
      controller_->write(addr, 0x4000, 0);  // Power off
    } else {
      // Power on first
      controller_->write(addr, 0x4000, 1);

      long nasa_mode;
      switch (mode) {
        case esphome::climate::CLIMATE_MODE_COOL: nasa_mode = NASA_MODE_COOL; break;
        case esphome::climate::CLIMATE_MODE_HEAT: nasa_mode = NASA_MODE_HEAT; break;
        case esphome::climate::CLIMATE_MODE_DRY: nasa_mode = NASA_MODE_DRY; break;
        case esphome::climate::CLIMATE_MODE_FAN_ONLY: nasa_mode = NASA_MODE_FAN; break;
        case esphome::climate::CLIMATE_MODE_HEAT_COOL: nasa_mode = NASA_MODE_AUTO; break;
        default: nasa_mode = NASA_MODE_AUTO; break;
      }
      controller_->write(addr, 0x4001, nasa_mode);
      last_active_mode_ = mode;
    }
    this->mode = mode;
  }

  if (call.get_target_temperature().has_value()) {
    float temp = *call.get_target_temperature();
    long raw = static_cast<long>(temp * 10.0f);
    controller_->write(addr, 0x4201, raw);
    this->target_temperature = temp;
  }

  if (call.get_fan_mode().has_value()) {
    auto fan = *call.get_fan_mode();
    long nasa_fan;
    switch (fan) {
      case esphome::climate::CLIMATE_FAN_AUTO: nasa_fan = NASA_FAN_AUTO; break;
      case esphome::climate::CLIMATE_FAN_LOW: nasa_fan = NASA_FAN_LOW; break;
      case esphome::climate::CLIMATE_FAN_MEDIUM: nasa_fan = NASA_FAN_MID; break;
      case esphome::climate::CLIMATE_FAN_HIGH: nasa_fan = NASA_FAN_HIGH; break;
      default: nasa_fan = NASA_FAN_AUTO; break;
    }
    controller_->write(addr, 0x4006, nasa_fan);
    this->set_fan_mode_(fan);
    this->clear_custom_fan_mode_();
  }

  if (call.has_custom_fan_mode()) {
    if (call.get_custom_fan_mode() == CUSTOM_FAN_TURBO) {
      controller_->write(addr, 0x4006, NASA_FAN_TURBO);
      this->clear_custom_fan_mode_();
      this->set_custom_fan_mode_(CUSTOM_FAN_TURBO);
    }
  }

  this->publish_state();
}

void NasactlClimate::update_power(bool on) {
  if (on) {
    if (this->mode == esphome::climate::CLIMATE_MODE_OFF) {
      this->mode = last_active_mode_;
    }
  } else {
    if (this->mode != esphome::climate::CLIMATE_MODE_OFF) {
      last_active_mode_ = this->mode;
    }
    this->mode = esphome::climate::CLIMATE_MODE_OFF;
  }
  this->publish_state();
}

void NasactlClimate::update_mode(long value) {
  switch (value) {
    case NASA_MODE_COOL:
      this->mode = esphome::climate::CLIMATE_MODE_COOL;
      break;
    case NASA_MODE_DRY:
      this->mode = esphome::climate::CLIMATE_MODE_DRY;
      break;
    case NASA_MODE_FAN:
      this->mode = esphome::climate::CLIMATE_MODE_FAN_ONLY;
      break;
    case NASA_MODE_HEAT:
      this->mode = esphome::climate::CLIMATE_MODE_HEAT;
      break;
    case NASA_MODE_AUTO:
      this->mode = esphome::climate::CLIMATE_MODE_HEAT_COOL;
      break;
    default:
      return;
  }
  last_active_mode_ = this->mode;
  this->publish_state();
}

void NasactlClimate::update_target_temp(float temp) {
  this->target_temperature = temp;
  this->publish_state();
}

void NasactlClimate::update_current_temp(float temp) {
  this->current_temperature = temp;
  this->publish_state();
}

void NasactlClimate::update_fan_mode(long value) {
  if (value == NASA_FAN_TURBO) {
    this->clear_custom_fan_mode_();
    this->set_custom_fan_mode_(CUSTOM_FAN_TURBO);
  } else {
    esphome::climate::ClimateFanMode fm;
    switch (value) {
      case NASA_FAN_AUTO: fm = esphome::climate::CLIMATE_FAN_AUTO; break;
      case NASA_FAN_LOW: fm = esphome::climate::CLIMATE_FAN_LOW; break;
      case NASA_FAN_MID: fm = esphome::climate::CLIMATE_FAN_MEDIUM; break;
      case NASA_FAN_HIGH: fm = esphome::climate::CLIMATE_FAN_HIGH; break;
      default: fm = esphome::climate::CLIMATE_FAN_AUTO; break;
    }
    this->set_fan_mode_(fm);
    this->clear_custom_fan_mode_();
  }
  this->publish_state();
}

// ClimateMessageRouter implementation
void ClimateMessageRouter::on_receive(long value) {
  switch (field_) {
    case FIELD_POWER:
      climate_->update_power(value != 0);
      break;
    case FIELD_MODE:
      climate_->update_mode(value);
      break;
    case FIELD_TARGET_TEMP:
      climate_->update_target_temp(static_cast<float>(static_cast<int16_t>(value)) * 0.1f);
      break;
    case FIELD_CURRENT_TEMP:
      climate_->update_current_temp(static_cast<float>(static_cast<int16_t>(value)) * 0.1f);
      break;
    case FIELD_FAN_MODE:
      climate_->update_fan_mode(value);
      break;
  }
}

}  // namespace nasactl
