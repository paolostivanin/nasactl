#pragma once
#include "esphome/components/sensor/sensor.h"
namespace esphome::number {
class Number : public sensor::Sensor {
 protected:
  virtual void control(float) = 0;
};
}
