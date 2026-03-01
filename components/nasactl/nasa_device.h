#pragma once

#include <string>
#include "nasa_address.h"

namespace nasactl {

class NasaDevice {
 public:
  NasaDevice(const std::string &address, uint8_t address_class)
      : address_(address), address_class_(address_class),
        parsed_address_(Address::parse(address)) {}

  const std::string &get_address() const { return address_; }
  uint8_t get_address_class() const { return address_class_; }
  const Address &get_parsed_address() const { return parsed_address_; }

 protected:
  std::string address_;
  uint8_t address_class_;
  Address parsed_address_;
};

}  // namespace nasactl
