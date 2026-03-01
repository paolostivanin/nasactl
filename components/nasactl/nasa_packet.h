#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "nasa_address.h"
#include "nasa_command.h"
#include "nasa_message.h"

namespace nasactl {

static const uint8_t PACKET_START = 0x32;
static const uint8_t PACKET_END = 0x34;
static const uint16_t PACKET_MAX_SIZE = 1500;

enum class DecodeResult {
  Ok,
  TooShort,
  InvalidStart,
  InvalidEnd,
  InvalidCRC,
  InvalidSize,
};

struct Packet {
  Address source;
  Address destination;
  Command command;
  std::vector<MessageSet> messages;

  static Packet create_read(const Address &dest, const std::vector<uint16_t> &message_numbers);
  static Packet create_write(const Address &dest, uint16_t message_number, long value);

  DecodeResult decode(const std::vector<uint8_t> &data);
  std::vector<uint8_t> encode() const;
  std::string to_string() const;
};

}  // namespace nasactl
