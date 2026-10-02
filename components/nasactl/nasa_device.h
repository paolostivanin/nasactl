#pragma once

#include <string>
#include "nasa_address.h"
#include "nasa_command.h"

namespace nasactl {

class NasaDevice {
 public:
  NasaDevice(const std::string &address, uint8_t address_class)
      : address_(Address::parse(address).to_string()), address_class_(address_class),
        parsed_address_(Address::parse(address)) {}

  const std::string &get_address() const { return address_; }
  uint8_t get_address_class() const { return address_class_; }
  const Address &get_parsed_address() const { return parsed_address_; }
  void set_control_data_type(bool request) { control_data_type_ = request ? DataType::Request : DataType::Write; }
  DataType get_control_data_type() const { return control_data_type_; }
  void set_targeted_reads(bool v) { targeted_reads_ = v; }
  bool get_targeted_reads() const { return targeted_reads_; }
  void set_has_climate(bool v) { has_climate_ = v; }
  bool has_climate() const { return has_climate_; }

 protected:
  std::string address_;
  uint8_t address_class_;
  Address parsed_address_;
  DataType control_data_type_{DataType::Write};
  bool targeted_reads_{false};
  bool has_climate_{false};
};

}  // namespace nasactl
