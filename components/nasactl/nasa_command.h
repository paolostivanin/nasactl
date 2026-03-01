#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nasactl {

enum class PacketType : uint8_t {
  StandBy = 0,
  Normal = 1,
  Gathering = 2,
  Install = 3,
  Download = 4,
};

enum class DataType : uint8_t {
  Undefined = 0,
  Read = 1,
  Write = 2,
  Request = 3,
  Notification = 4,
  Response = 5,
  Ack = 6,
  Nack = 7,
};

struct Command {
  bool packet_information{false};
  uint8_t protocol_version{2};
  uint8_t retry_count{0};
  PacketType packet_type{PacketType::Normal};
  DataType data_type{DataType::Undefined};
  uint8_t packet_number{0};

  static uint8_t next_packet_number() {
    static uint8_t counter = 0;
    return counter++;
  }

  void decode(const std::vector<uint8_t> &data, uint32_t &offset) {
    uint8_t b0 = data[offset];
    uint8_t b1 = data[offset + 1];
    packet_number = data[offset + 2];

    packet_information = (b0 >> 7) & 0x01;
    protocol_version = (b0 >> 4) & 0x03;
    retry_count = (b0 >> 2) & 0x03;
    packet_type = static_cast<PacketType>((b1 >> 4) & 0x0F);
    data_type = static_cast<DataType>(b1 & 0x0F);

    offset += 3;
  }

  std::vector<uint8_t> encode() const {
    uint8_t b0 = ((packet_information ? 1 : 0) << 7) |
                 ((protocol_version & 0x03) << 4) |
                 ((retry_count & 0x03) << 2);
    uint8_t b1 = ((static_cast<uint8_t>(packet_type) & 0x0F) << 4) |
                 (static_cast<uint8_t>(data_type) & 0x0F);
    return {b0, b1, packet_number};
  }

  std::string to_string() const {
    char buf[64];
    snprintf(buf, sizeof(buf), "pkt=%d dt=%d num=%d",
             static_cast<int>(packet_type), static_cast<int>(data_type), packet_number);
    return std::string(buf);
  }
};

}  // namespace nasactl
