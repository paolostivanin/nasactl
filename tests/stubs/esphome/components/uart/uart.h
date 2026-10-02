#pragma once
#include <cstdint>
#include <deque>
#include <vector>
namespace esphome::uart {
class UARTDevice {
 public:
  std::deque<uint8_t> rx;
  std::vector<std::vector<uint8_t>> tx;
  size_t available() const { return rx.size(); }
  bool read_byte(uint8_t *byte) { *byte = rx.front(); rx.pop_front(); return true; }
  void write_array(const uint8_t *data, size_t size) { tx.emplace_back(data, data + size); }
  void flush() {}
};
}
