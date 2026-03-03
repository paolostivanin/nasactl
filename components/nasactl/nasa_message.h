#pragma once

#include <cstdint>
#include <vector>

namespace nasactl {

enum class MessageSetType : uint8_t {
  Enum = 0,          // 1 byte value
  Variable = 1,      // 2 bytes (int16)
  LongVariable = 2,  // 4 bytes (int32)
  Structure = 3,     // Variable length
};

struct MessageSet {
  uint16_t message_number{0};
  MessageSetType type{MessageSetType::Enum};
  long value{0};

  MessageSet() = default;
  explicit MessageSet(uint16_t msg_num) : message_number(msg_num) {
    // Type is derived from bits 9-10 of message number
    type = static_cast<MessageSetType>((msg_num >> 9) & 0x03);
  }

  bool decode(const std::vector<uint8_t> &data, uint32_t &offset) {
    if (offset + 2 > data.size())
      return false;
    message_number = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
    type = static_cast<MessageSetType>((message_number >> 9) & 0x03);
    offset += 2;

    switch (type) {
      case MessageSetType::Enum:
        if (offset + 1 > data.size())
          return false;
        value = data[offset];
        offset += 1;
        break;
      case MessageSetType::Variable:
        if (offset + 2 > data.size())
          return false;
        value = (static_cast<int16_t>(data[offset]) << 8) | data[offset + 1];
        offset += 2;
        break;
      case MessageSetType::LongVariable:
        if (offset + 4 > data.size())
          return false;
        value = (static_cast<long>(data[offset]) << 24) |
                (static_cast<long>(data[offset + 1]) << 16) |
                (static_cast<long>(data[offset + 2]) << 8) |
                static_cast<long>(data[offset + 3]);
        offset += 4;
        break;
      case MessageSetType::Structure: {
        if (offset + 1 > data.size())
          return false;
        uint8_t len = data[offset];
        offset += 1;
        if (offset + len > data.size())
          return false;
        // For structures, store the first 4 bytes as value for simple access
        value = 0;
        for (uint8_t i = 0; i < len && i < 4; i++) {
          value = (value << 8) | data[offset + i];
        }
        offset += len;
        break;
      }
    }
    return true;
  }

  void encode(std::vector<uint8_t> &data) const {
    data.push_back(static_cast<uint8_t>(message_number >> 8));
    data.push_back(static_cast<uint8_t>(message_number & 0xFF));

    switch (type) {
      case MessageSetType::Enum:
        data.push_back(static_cast<uint8_t>(value & 0xFF));
        break;
      case MessageSetType::Variable:
        data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        data.push_back(static_cast<uint8_t>(value & 0xFF));
        break;
      case MessageSetType::LongVariable:
        data.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
        data.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        data.push_back(static_cast<uint8_t>(value & 0xFF));
        break;
      default:
        break;
    }
  }

  uint8_t payload_size() const {
    switch (type) {
      case MessageSetType::Enum: return 1;
      case MessageSetType::Variable: return 2;
      case MessageSetType::LongVariable: return 4;
      default: return 0;
    }
  }
};

}  // namespace nasactl
