#pragma once

#include <deque>
#include <functional>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"

#include "esphome/core/hal.h"

#include "nasa_packet.h"
#include "nasa_queue.h"

namespace nasactl {

enum class WriteOutcome : uint8_t { Acknowledged, Rejected, TimedOut, QueueRejected };

struct OutgoingPacket {
  Packet packet;
  std::vector<uint8_t> data;
  bool wait_for_ack{true};
  uint32_t queued_at{0};
  uint32_t queue_lifetime{0};
  uint32_t next_retry{0};
  uint32_t first_send{0};
  uint16_t send_count{0};
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
  void set_debug_log_packets(bool v) { debug_log_packets_ = v; }
  void set_packet_information(bool v) { packet_information_ = v; }
  void set_standard_command_header(bool v) { standard_command_header_ = v; }
  void set_tx_timeouts_sensor(esphome::sensor::Sensor *s) { tx_timeouts_sensor_ = s; }
  void set_tx_nacks_sensor(esphome::sensor::Sensor *s) { tx_nacks_sensor_ = s; }
  void set_tx_queue_drops_sensor(esphome::sensor::Sensor *s) { tx_queue_drops_sensor_ = s; }
  uint32_t get_tx_timeouts() const { return tx_timeouts_; }
  uint32_t get_tx_nacks() const { return tx_nacks_; }
  uint32_t get_tx_queue_drops() const { return tx_queue_drops_; }
  void record_readback_drop() { record_queue_drop_(); }

  // Receive callback — controller registers to receive decoded packets
  void set_on_packet(std::function<void(const Packet &)> callback) {
    on_packet_ = callback;
  }
  void set_on_write_result(std::function<void(const Packet &, WriteOutcome)> callback) {
    on_write_result_ = callback;
  }

  // Send methods
  bool send_read(const std::vector<uint16_t> &message_numbers);
  bool send_read(const Address &dest, const std::vector<uint16_t> &message_numbers,
                 uint32_t queue_lifetime = 0);
  bool send_write(const Address &dest, uint16_t message_number, long value);
  bool send_write(const Address &dest, const std::vector<MessageSet> &messages,
                  DataType data_type = DataType::Write);

  // Retry policy is independent of the on-wire packet number.
  bool queue_packet(Packet packet, bool wait_for_ack, uint32_t queue_lifetime = 0);
  // Compatibility for callers of the original raw-packet interface.
  bool queue_packet(uint8_t id, std::vector<uint8_t> &&data);

  void ack_packet(const Packet &packet);

 private:
  void read_data_();
  void write_data_();
  void before_write_();
  void after_write_();
  void finish_write_(WriteOutcome outcome);
  void notify_write_result_(const OutgoingPacket &packet, WriteOutcome outcome);
  void record_queue_drop_();
  bool queue_outgoing_(OutgoingPacket &&packet);
  void log_packet_(const char *direction, const Packet &packet,
                   const std::vector<uint8_t> &data) const;

  esphome::InternalGPIOPin *flow_control_pin_{nullptr};
  uint32_t silence_interval_{100};
  uint32_t retry_interval_{500};
  uint8_t min_retries_{1};
  uint32_t send_timeout_{4000};
  bool debug_log_packets_{false};
  bool packet_information_{false};
  bool standard_command_header_{false};

  static const size_t MAX_SEND_QUEUE_SIZE = 64;

  std::vector<uint8_t> rx_buffer_;
  std::deque<OutgoingPacket> send_queue_;
  uint32_t last_rx_time_{0};

  std::function<void(const Packet &)> on_packet_;
  std::function<void(const Packet &, WriteOutcome)> on_write_result_;
  uint32_t tx_timeouts_{0};
  uint32_t tx_nacks_{0};
  uint32_t tx_queue_drops_{0};
  esphome::sensor::Sensor *tx_timeouts_sensor_{nullptr};
  esphome::sensor::Sensor *tx_nacks_sensor_{nullptr};
  esphome::sensor::Sensor *tx_queue_drops_sensor_{nullptr};

  // Batch dispatcher for read requests: queue 100, batch 10, delay 200ms
  BatchDispatcher<uint16_t> read_dispatcher_{100, 10, 200};
};

}  // namespace nasactl
