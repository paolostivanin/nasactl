#pragma once

#include <cstdint>
#include <vector>

namespace nasactl {

// CRC-CCITT (polynomial 0x1021) used by Samsung NASA protocol
inline uint16_t crc16(const std::vector<uint8_t> &data, int start, int length) {
  uint16_t crc = 0;
  for (int i = start; i < start + length; i++) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (int j = 0; j < 8; j++) {
      if (crc & 0x8000)
        crc = (crc << 1) ^ 0x1021;
      else
        crc <<= 1;
    }
  }
  return crc & 0xFFFF;
}

}  // namespace nasactl
