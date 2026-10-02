#pragma once
#include <cstdint>
namespace esphome {
namespace test { inline uint32_t now = 0; }
inline uint32_t millis() { return test::now; }
inline void delayMicroseconds(uint32_t) {}
class InternalGPIOPin {
 public:
  void setup() {}
  void digital_write(bool) {}
};
}
