#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nasactl {

enum class AddressClass : uint8_t {
  Outdoor = 0x10,
  HTU = 0x11,
  Indoor = 0x20,
  ERV = 0x30,
  Diffuser = 0x35,
  MCU = 0x38,
  RMC = 0x40,
  WiredRemote = 0x50,
  PIM = 0x58,
  SIM = 0x59,
  WiFiKit = 0x62,
  DMS = 0x65,
  JigTester = 0xB0,
  BroadcastSelfLayer = 0xB1,
  BroadcastControlLayer = 0xB2,
  BroadcastSetLayer = 0xB3,
  BroadcastCSLayer = 0xB4,
  BroadcastControlCSLayer = 0xB5,
  BroadcastModuleLayer = 0xB8,
  BroadcastLocalLayer = 0xBF,
};

struct Address {
  AddressClass klass{AddressClass::Indoor};
  uint8_t channel{0};
  uint8_t address{0};

  static Address parse(const std::string &str) {
    Address addr;
    if (str.length() >= 8) {
      try {
        addr.klass = static_cast<AddressClass>(std::stoi(str.substr(0, 2), nullptr, 16));
        addr.channel = std::stoi(str.substr(3, 2), nullptr, 16);
        addr.address = std::stoi(str.substr(6, 2), nullptr, 16);
      } catch (...) {
        // Return default address on parse failure
      }
    }
    return addr;
  }

  static Address my_address() {
    return {AddressClass::JigTester, 0xFF, 0x00};
  }

  static Address broadcast() {
    return {AddressClass::BroadcastSetLayer, 0x00, 0x20};
  }

  bool decode(const std::vector<uint8_t> &data, uint32_t &offset) {
    if (offset + 3 > data.size())
      return false;
    klass = static_cast<AddressClass>(data[offset]);
    channel = data[offset + 1];
    address = data[offset + 2];
    offset += 3;
    return true;
  }

  std::vector<uint8_t> encode() const {
    return {static_cast<uint8_t>(klass), channel, address};
  }

  std::string to_string() const {
    char buf[9];
    snprintf(buf, sizeof(buf), "%02x.%02x.%02x",
             static_cast<uint8_t>(klass), channel, address);
    return std::string(buf);
  }

  bool operator==(const Address &other) const {
    return klass == other.klass && channel == other.channel && address == other.address;
  }

  bool operator!=(const Address &other) const { return !(*this == other); }
};

inline const char *address_class_to_string(AddressClass klass) {
  switch (klass) {
    case AddressClass::Outdoor: return "Outdoor";
    case AddressClass::HTU: return "HTU";
    case AddressClass::Indoor: return "Indoor";
    case AddressClass::ERV: return "ERV";
    case AddressClass::WiFiKit: return "WiFi Kit";
    case AddressClass::JigTester: return "Jig Tester";
    default: return "Unknown";
  }
}

}  // namespace nasactl
