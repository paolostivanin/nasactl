#pragma once

#include <deque>
#include <functional>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

#ifdef USE_ESP32
#include "esphome/core/hal.h"
#endif

#include "nasa_packet.h"
#include "nasa_queue.h"

namespace nasactl {

struct OutgoingPacket {
  uint8_t id;
  std::vector<uint8_t> data;
  uint32_t next_retry{0};
  uint32_t timeout{0};
  uint8_t retry_count{0};
};

class NasaClient : public esphome::Component, public esphome::uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return esphome::setup_priority::DATA; }

  // Configuration
  void set_flow_control_pin(esphome::InternalGPIOPin *pin) { flow_control_pin_ = pin; }
  void set_silence_interval(uint32_t ms) { silence_interval_ = ms; }
  void set_retry_interval(uint32_t ms) { retry_interval_ = ms; }
  void set_min_retries(uint8_t n) { min_retries_ = n; }
  void set_send_timeout(uint32_t ms) { send_timeout_ = ms; }

  // Receive callback — controller registers to receive decoded packets
  void set_on_packet(std::function<void(const Packet &)> callback) {
    on_packet_ = callback;
  }

  // Send methods
  void send_read(const std::vector<uint16_t> &message_numbers);
  void send_write(const Address &dest, uint16_t message_number, long value);

  // Queue a raw packet with retry (id > 0) or immediate (id = 0)
  void queue_packet(uint8_t id, std::vector<uint8_t> &&data);

  // Acknowledge a packet (remove from retry queue)
  void ack_packet(uint8_t id);

 private:
  void read_data_();
  void write_data_();
  void before_write_();
  void after_write_();

  esphome::InternalGPIOPin *flow_control_pin_{nullptr};
  uint32_t silence_interval_{100};
  uint32_t retry_interval_{500};
  uint8_t min_retries_{1};
  uint32_t send_timeout_{4000};

  static const size_t MAX_SEND_QUEUE_SIZE = 64;

  std::vector<uint8_t> rx_buffer_;
  std::deque<OutgoingPacket> send_queue_;
  uint32_t last_rx_time_{0};

  std::function<void(const Packet &)> on_packet_;

  // Batch dispatcher for read requests: queue 100, batch 10, delay 200ms
  BatchDispatcher<uint16_t> read_dispatcher_{100, 10, 200};
};

}  // namespace nasactl
