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
  Peak = 0x5A,
  PowerDivider = 0x5B,
  OnOffController = 0x60,
  WiFiKit = 0x62,
  CentralController = 0x65,
  DMS = 0x6A,
  JigTester = 0x80,
  BroadcastSelfLayer = 0xB0,
  BroadcastControlLayer = 0xB1,
  BroadcastSetLayer = 0xB2,
  BroadcastControlAndSetLayer = 0xB3,
  BroadcastModuleLayer = 0xB4,
  BroadcastCSM = 0xB7,
  BroadcastLocalLayer = 0xB8,
  BroadcastCSML = 0xBF,
};

struct Address {
  AddressClass klass{AddressClass::Indoor};
  uint8_t channel{0};
  uint8_t address{0};

  static Address parse(const std::string &str) {
    Address addr;
    if (str.length() >= 8) {
      unsigned long v1 = strtoul(str.c_str(), nullptr, 16);
      unsigned long v2 = strtoul(str.c_str() + 3, nullptr, 16);
      unsigned long v3 = strtoul(str.c_str() + 6, nullptr, 16);
      addr.klass = static_cast<AddressClass>(v1 & 0xFF);
      addr.channel = v2 & 0xFF;
      addr.address = v3 & 0xFF;
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
    case AddressClass::MCU: return "MCU";
    case AddressClass::RMC: return "RMC";
    case AddressClass::WiredRemote: return "Wired Remote";
    case AddressClass::WiFiKit: return "WiFi Kit";
    case AddressClass::CentralController: return "Central Controller";
    case AddressClass::DMS: return "DMS";
    case AddressClass::JigTester: return "Jig Tester";
    default: return "Unknown";
  }
}

}  // namespace nasactl
