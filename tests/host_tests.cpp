#include <algorithm>
#include <cassert>
#include <iostream>
#include <random>
#include "nasa_client.h"
#include "nasa_controller.h"
#include "nasactl_climate.h"
#include "nasactl_number.h"
#include "nasactl_sensor.h"
#include "nasactl_select.h"
#include "nasactl_switch.h"
#include "nasactl_binary_sensor.h"
#include "nasactl_text_sensor.h"
#include "nasa_crc.h"
#include "esphome/core/log.h"

using namespace nasactl;
using namespace esphome::climate;
using esphome::test::now;

static MessageSet message(uint16_t code, long value) {
  MessageSet result(code);
  result.value = value;
  return result;
}
static Packet decode(const std::vector<uint8_t> &frame) {
  Packet packet;
  assert(packet.decode(frame) == DecodeResult::Ok);
  return packet;
}
static Packet reply(const Packet &sent, DataType type = DataType::Ack) {
  Packet packet;
  packet.source = sent.destination;
  packet.destination = sent.source;
  packet.command.data_type = type;
  packet.command.packet_number = sent.command.packet_number;
  return packet;
}
static void inject(NasaClient &client, const Packet &packet) {
  auto frame = packet.encode();
  client.rx.insert(client.rx.end(), frame.begin(), frame.end());
  client.loop();
}
static bool logged(const std::string &text) {
  return std::any_of(esphome::test::logs.begin(), esphome::test::logs.end(),
                     [&](const auto &line) { return line.find(text) != std::string::npos; });
}
struct Rig {
  NasaClient client;
  NasaController controller{&client};
  NasaDevice device{"20.00.02", 0x20};
  Rig() {
    now = 1000;
    esphome::test::logs.clear();
    client.setup();
    controller.register_device(&device);
    controller.setup();
  }
};
struct TestClimate : NasactlClimate { using NasactlClimate::control; };
struct Entity : NasaBase {
  long state = -1;
  Entity(uint16_t code, NasaDevice *device, ControllerMode mode = ControllerMode::Control)
      : NasaBase("test", code, mode, device) {}
  void on_receive(long value) override { state = value; }
};
struct TestNumber : NasactlNumber { using NasactlNumber::NasactlNumber; using NasactlNumber::control; };
struct TestSelect : NasactlSelect { using NasactlSelect::NasactlSelect; using NasactlSelect::control; };
struct TestSwitch : NasactlSwitch { using NasactlSwitch::NasactlSwitch; using NasactlSwitch::write_state; };

static void update_crc(std::vector<uint8_t> &frame) {
  const size_t offset = frame.size() - 3;
  auto crc = crc16(frame, 3, offset - 3);
  frame[offset] = crc >> 8;
  frame[offset + 1] = crc & 0xff;
}

static void malformed_and_random_packets() {
  auto frame = Packet::create_write(Address::parse("20.00.02"), 0x4201, 230).encode();
  for (size_t length = 0; length < frame.size(); length++) {
    Packet packet;
    assert(packet.decode({frame.begin(), frame.begin() + length}) != DecodeResult::Ok);
  }
  // A CRC-valid frame may still be structurally invalid.
  auto short_payload = frame;
  short_payload[13] = 0x44;  // Claim a 4-byte value where only two bytes fit.
  update_crc(short_payload);
  Packet invalid;
  assert(invalid.decode(short_payload) == DecodeResult::InvalidSize);
  auto wrong_count = frame;
  wrong_count[12] = 0;
  update_crc(wrong_count);
  assert(invalid.decode(wrong_count) == DecodeResult::InvalidSize);
  for (unsigned size = 0; size < 14; size++) {
    auto bad_size = frame;
    bad_size[1] = 0;
    bad_size[2] = size;
    assert(invalid.decode(bad_size) != DecodeResult::Ok);
  }
  std::mt19937 random(0x4e415341);
  for (unsigned iteration = 0; iteration < 20000; iteration++) {
    std::vector<uint8_t> bytes(random() % 1800);
    for (auto &byte : bytes) byte = random();
    if (bytes.size() > 2 && iteration % 2 == 0) {
      bytes[0] = PACKET_START;
      unsigned size = random() % (bytes.size() + 1);
      bytes[1] = size >> 8;
      bytes[2] = size;
    }
    Packet packet;
    packet.decode(bytes);  // Sanitizers check arbitrary malformed frames.
    if (iteration % 10 == 0) {
      bytes = frame;
      bytes[12 + random() % (bytes.size() - 15)] = random();
      update_crc(bytes);
      packet.decode(bytes);  // Reach message decoding with a valid CRC.
    }
  }
}

static void protocol_fixtures() {
  // Independent fixture: upstream NASA layout, CRC-16/XMODEM AC57.
  const std::vector<uint8_t> fixture{
      0x32, 0x00, 0x18, 0x80, 0xff, 0x00, 0x20, 0x00, 0x02,
      0xc0, 0x13, 0x42, 0x03, 0x40, 0x00, 0x01, 0x40, 0x01,
      0x03, 0x42, 0x01, 0x00, 0xf0, 0xac, 0x57, 0x34};
  auto packet = decode(fixture);
  assert(packet.command.protocol_version == 2 && packet.command.packet_information);
  assert(packet.command.data_type == DataType::Request && packet.messages.size() == 3);
  assert(packet.messages[2].value == 240);
  auto outgoing = Packet::create_write(Address::parse("20.00.02"),
                                       {message(0x4000, 1), message(0x4001, 3), message(0x4201, 240)});
  outgoing.command.packet_information = true;
  outgoing.command.packet_number = 0x42;
  outgoing.command.data_type = DataType::Request;
  assert(outgoing.encode() == fixture);
  Command command;
  uint32_t offset = 0;
  assert(command.decode({0xd8, 0x12, 0xff}, offset));
  assert(command.protocol_version == 2 && command.retry_count == 3);
  assert(command.encode() == std::vector<uint8_t>({0xd8, 0x12, 0xff}));
  auto corrupt = fixture;
  corrupt[15] ^= 1;
  Packet invalid;
  assert(invalid.decode(corrupt) == DecodeResult::InvalidCRC);
  // Long and signed-variable payloads coexist without changing field widths.
  auto mixed = Packet::create_write(Address::parse("20.00.00"),
                                    {message(0x4006, 4), message(0x4201, -10), message(0x4400, 123456)});
  auto decoded = decode(mixed.encode());
  assert(decoded.messages[1].value == 65526 && decoded.messages[2].value == 123456);
}

static void batches() {
  BatchDispatcher<uint16_t> dispatcher(20, 10, 200);
  std::vector<std::vector<uint16_t>> batches;
  dispatcher.set_callback([&](auto &batch) { batches.push_back(batch); });
  dispatcher.push(std::vector<uint16_t>{1, 2, 3, 4});
  dispatcher.update(0);
  dispatcher.update(199);
  assert(batches.empty());
  dispatcher.update(200);
  dispatcher.update(1000);
  assert(batches.size() == 1 && batches[0].size() == 4);
  for (uint16_t i = 0; i < 11; i++) assert(dispatcher.push(i));
  dispatcher.update(1000);
  dispatcher.update(1001);
  dispatcher.update(1200);
  assert(batches.size() == 3 && batches[1].size() == 10 && batches[2].size() == 1);
  BatchDispatcher<int> limited(1, 10, 200);
  assert(limited.push(1) && !limited.push(2));
  BatchDispatcher<int> wrap(10, 10, 200);
  unsigned calls = 0;
  wrap.set_callback([&](auto &) { calls++; });
  wrap.push(1);
  wrap.update(UINT32_MAX - 50);
  wrap.push(2);
  wrap.update(49);
  wrap.update(149);
  assert(calls == 2);
}

static void acknowledgements() {
  NasaClient client;
  now = 1000;
  client.setup();
  std::vector<WriteOutcome> outcomes;
  client.set_on_write_result([&](const auto &, auto outcome) { outcomes.push_back(outcome); });
  auto first = Packet::create_write(Address::parse("20.00.02"), 0x4000, 1);
  auto second = Packet::create_write(first.destination, 0x4001, 3);
  client.queue_packet(first, true);
  client.queue_packet(second, true);
  client.ack_packet(reply(first));  // Never sent.
  assert(outcomes.empty());
  client.loop();
  assert(client.tx.size() == 1);
  auto foreign = reply(first);
  foreign.source = Address::parse("20.00.03");
  client.ack_packet(foreign);
  foreign = reply(first);
  foreign.destination = Address::parse("80.ff.01");
  client.ack_packet(foreign);
  client.ack_packet(reply(second));  // Queued but not in flight.
  auto wrong_id = reply(first);
  wrong_id.command.packet_number = first.command.packet_number + 1;
  client.ack_packet(wrong_id);
  assert(outcomes.empty());
  inject(client, reply(first));
  assert(outcomes == std::vector<WriteOutcome>{WriteOutcome::Acknowledged});
  client.ack_packet(reply(first));  // Duplicate cannot acknowledge the next write.
  now += 100;
  client.loop();
  assert(client.tx.size() == 2);
  auto nack = reply(second, DataType::Nack);
  inject(client, nack);
  client.ack_packet(nack);
  assert(outcomes.size() == 2 && outcomes.back() == WriteOutcome::Rejected);
  assert(client.get_tx_nacks() == 1);
  assert(logged("0x4001=3") && logged("20.00.02") && logged("sends=1"));
}

static void timeouts_and_wrap() {
  NasaClient client;
  now = UINT32_MAX - 999;
  client.setup();
  unsigned completions = 0;
  client.set_on_write_result([&](const auto &, auto outcome) {
    assert(outcome == WriteOutcome::TimedOut);
    completions++;
  });
  client.send_write(Address::parse("20.00.02"), 0x4001, 3);
  client.loop();
  now += 500;
  client.loop();
  assert(client.tx.size() == 2);
  now += 3500;
  // RX traffic must not prevent expiry even inside the silence interval.
  client.rx.push_back(0);
  client.loop();
  client.loop();
  assert(completions == 1 && client.get_tx_timeouts() == 1);
  assert(logged("0x4001=3") && logged("sends=2"));
  for (unsigned i = 0; i < 600; i++) assert(Command::next_packet_number() != 0);
}

static void reads_and_diagnostics() {
  NasaClient client;
  esphome::sensor::Sensor timeouts, nacks, drops;
  client.set_tx_timeouts_sensor(&timeouts);
  client.set_tx_nacks_sensor(&nacks);
  client.set_tx_queue_drops_sensor(&drops);
  client.set_debug_log_packets(true);
  now = 0;
  client.setup();
  assert(timeouts.publications == std::vector<float>{0});
  assert(nacks.publications == std::vector<float>{0});
  assert(drops.publications == std::vector<float>{0});
  client.send_read({0x4000, 0x4001, 0x4006, 0x4201});
  client.loop();
  now = 200;
  client.loop();
  client.loop();
  assert(client.tx.size() == 1);
  auto read = decode(client.tx[0]);
  assert(read.command.packet_number != 0 && read.command.data_type == DataType::Read);
  client.send_write(Address::parse("20.00.02"), 0x4000, 1);
  client.loop();
  assert(client.tx.size() == 2);  // Read did not wait for a reply.
  assert(logged("TX bytes[0]"));
  auto foreign = reply(decode(client.tx[1]));
  foreign.source = Address::parse("20.00.09");
  inject(client, foreign);
  assert(logged("RX 20.00.09") && logged("info="));
  assert(client.get_tx_nacks() == 0);

  NasaClient queue;
  now = 1000;
  queue.setup();
  unsigned rejected = 0;
  queue.set_on_write_result([&](const auto &, auto outcome) {
    if (outcome == WriteOutcome::QueueRejected) rejected++;
  });
  for (unsigned i = 0; i < 64; i++) assert(queue.send_write(read.destination, 0x4000, 1));
  assert(!queue.send_write(read.destination, 0x4001, 3));
  assert(rejected == 1 && queue.get_tx_queue_drops() == 1);
  assert(!queue.send_read(Address::parse("20.00.03"), {0x4000}));
  assert(rejected == 1 && queue.get_tx_queue_drops() == 2);  // No write callback for reads.
  NasaClient read_queue;
  read_queue.setup();
  std::vector<uint16_t> codes(101, 0x4000);
  assert(!read_queue.send_read(codes) && read_queue.get_tx_queue_drops() == 1);

  NasaClient expired;
  expired.setup();
  expired.send_read(read.destination, {0x4000}, 5000);
  now += 5000;
  expired.loop();
  assert(expired.tx.empty() && expired.get_tx_timeouts() == 1);
}

static void reconciliation(WriteOutcome outcome) {
  Rig rig;
  Entity entity(0x4269, &rig.device, ControllerMode::FSV);
  rig.controller.register_component(&entity);
  if (outcome == WriteOutcome::QueueRejected) {
    for (unsigned i = 0; i < 64; i++) rig.client.send_read(Address::broadcast(), {0x4000});
  }
  rig.controller.write("20.00.02", 0x4269, 10);
  if (outcome == WriteOutcome::QueueRejected) {
    for (unsigned i = 0; i < 64; i++) rig.client.loop();
  } else {
    rig.client.loop();
    auto sent = decode(rig.client.tx.back());
    if (outcome == WriteOutcome::TimedOut) {
      now += 4000;
      rig.client.loop();
    } else {
      inject(rig.client, reply(sent, outcome == WriteOutcome::Acknowledged ? DataType::Ack : DataType::Nack));
    }
  }
  const uint32_t completed = now;
  rig.client.tx.clear();
  now = completed + 2999;
  rig.controller.loop();
  rig.client.loop();
  assert(rig.client.tx.empty());
  now++;
  rig.controller.loop();
  rig.client.loop();
  assert(rig.client.tx.size() == 1);
  auto read = decode(rig.client.tx.back());
  assert(read.messages.size() == 1 && read.messages[0].message_number == 0x4269);
  assert(read.destination == Address::broadcast());
  // Only a genuine device report corrects the optimistic value.
  auto report = reply(read, DataType::Ack);
  report.source = rig.device.get_parsed_address();
  report.messages = {message(0x4269, 9)};
  inject(rig.client, report);
  assert(entity.state == -1);
  report.command.data_type = DataType::Response;
  inject(rig.client, report);
  assert(entity.state == 9);
  now = completed + 15000;
  rig.controller.loop();
  rig.client.loop();
  assert(rig.client.tx.size() == 2);
  now = completed + 20000;
  rig.controller.loop();
  assert(!logged("No fresh report after write: 20.00.02 code=0x4269"));
}

static void readback_coalescing_and_pressure() {
  Rig rig;
  rig.device.set_targeted_reads(true);
  rig.device.set_has_climate(true);
  NasaDevice other("20.00.03", 0x20);
  other.set_targeted_reads(true);
  rig.controller.register_device(&other);
  rig.controller.write("20.00.02", 0x4000, 1);
  rig.client.loop();
  inject(rig.client, reply(decode(rig.client.tx.back())));
  now += 100;
  rig.controller.write("20.00.02", 0x4001, 3);
  rig.client.loop();
  inject(rig.client, reply(decode(rig.client.tx.back())));
  const auto completed = now;
  now += 100;
  rig.controller.write("20.00.03", 0x4000, 1);
  rig.client.loop();
  inject(rig.client, reply(decode(rig.client.tx.back())));
  rig.client.tx.clear();
  now = completed + 3100;
  rig.controller.loop();
  rig.client.loop();
  rig.client.loop();
  assert(rig.client.tx.size() == 2);
  assert(decode(rig.client.tx[0]).destination == rig.device.get_parsed_address());
  assert(decode(rig.client.tx[0]).messages.size() == 4);  // One coalesced climate read.
  assert(decode(rig.client.tx[1]).destination == other.get_parsed_address());
  now = completed + 20100;
  rig.controller.loop();
  assert(logged("No fresh report after write"));

  Rig pressure;
  pressure.controller.write("20.00.02", 0x4269, 10);
  pressure.client.loop();
  inject(pressure.client, reply(decode(pressure.client.tx.back())));
  for (unsigned i = 0; i < 64; i++) pressure.client.send_read(Address::broadcast(), {0x4000});
  now += 3000;
  pressure.controller.loop();
  assert(pressure.client.get_tx_queue_drops() == 1);
  pressure.controller.loop();  // No recursive or per-loop retry storm.
  assert(pressure.client.get_tx_queue_drops() == 1);
  for (unsigned i = 0; i < 64; i++) pressure.client.loop();
  pressure.client.tx.clear();
  now += 200;
  pressure.controller.loop();
  pressure.client.loop();
  assert(pressure.client.tx.size() == 1);
}

static std::vector<Packet> climate_packets(const ClimateCall &call, bool batched) {
  Rig rig;
  rig.device.set_has_climate(true);
  TestClimate climate;
  climate.set_controller(&rig.controller);
  climate.set_device(&rig.device);
  climate.set_batch_writes(batched);
  climate.control(call);
  assert(climate.publications == 1);  // Optimistic state before any UART send.
  assert(rig.client.tx.empty());
  std::vector<Packet> packets;
  for (unsigned i = 0; i < 5; i++) {
    auto count = rig.client.tx.size();
    rig.client.loop();
    if (rig.client.tx.size() == count) break;
    auto packet = decode(rig.client.tx.back());
    packets.push_back(packet);
    rig.client.ack_packet(reply(packet));
  }
  return packets;
}
static void climate_calls() {
  ClimateCall call;
  call.mode = CLIMATE_MODE_FAN_ONLY;
  call.fan = CLIMATE_FAN_HIGH;
  call.temperature = 24;
  auto packets = climate_packets(call, true);
  assert(packets.size() == 1 && packets[0].messages.size() == 4);
  assert(packets[0].messages[0].message_number == 0x4000 && packets[0].messages[0].value == 1);
  assert(packets[0].messages[1].value == 3);
  assert(climate_packets(call, false).size() == 4);
  call = {};
  call.fan = CLIMATE_FAN_HIGH;
  packets = climate_packets(call, true);
  assert(packets.size() == 1 && packets[0].messages.size() == 1);
  assert(packets[0].messages[0].message_number == 0x4006);
  call = {};
  call.temperature = 23;
  packets = climate_packets(call, true);
  assert(packets[0].messages.size() == 1 && packets[0].messages[0].message_number == 0x4201);
  assert(packets[0].messages[0].value == 230);
  call.custom_fan = "Turbo";
  call.fan = CLIMATE_FAN_LOW;
  call.mode = CLIMATE_MODE_OFF;
  packets = climate_packets(call, true);
  assert(packets[0].messages.size() == 3 && packets[0].messages[0].value == 0);
  assert(packets[0].messages.back().message_number == 0x4006 && packets[0].messages.back().value == 4);
  call = {};
  call.mode = CLIMATE_MODE_OFF;
  assert(climate_packets(call, true)[0].messages.size() == 1);

  Rig rig;
  rig.device.set_control_data_type(true);
  rig.client.set_packet_information(true);
  Entity fsv(0x4269, &rig.device, ControllerMode::FSV);
  rig.controller.register_component(&fsv);
  rig.controller.write("20.00.02", {message(0x4000, 1), message(0x4269, 10)});
  rig.client.loop();
  auto control = decode(rig.client.tx.back());
  assert(control.command.data_type == DataType::Request && control.command.packet_information);
  rig.client.ack_packet(reply(control));
  rig.client.loop();
  assert(decode(rig.client.tx.back()).command.data_type == DataType::Write);
}

static void hydro_entity_regressions() {
  Rig rig;
  NasaDevice hydro("20.00.00", 0x20);
  rig.controller.register_device(&hydro);
  TestSwitch heating("heating", 0x4000, ControllerMode::Control, &hydro);
  heating.set_parent(&rig.controller);
  heating.write_state(true);
  assert(heating.state && rig.client.tx.empty());
  rig.client.loop();
  auto sent = decode(rig.client.tx.back());
  assert(sent.destination == hydro.get_parsed_address());
  assert(sent.messages[0].message_number == 0x4000 && sent.messages[0].value == 1);
  rig.client.ack_packet(reply(sent));
  heating.on_receive(0);
  assert(!heating.state);

  TestNumber dhw("DHW", 0x4235, ControllerMode::Control, &hydro);
  dhw.set_parent(&rig.controller);
  dhw.set_divisor(10);
  dhw.set_signed(true);
  dhw.control(49.5f);
  assert(dhw.state == 49.5f);
  rig.client.loop();
  sent = decode(rig.client.tx.back());
  assert(sent.destination == hydro.get_parsed_address() && sent.messages[0].value == 495);
  rig.client.ack_packet(reply(sent));
  dhw.on_receive(485);
  assert(dhw.state == 48.5f);
  dhw.on_receive(65511);
  assert(dhw.state == -2.5f);

  TestNumber delay("delay", 0x4267, ControllerMode::FSV, &hydro);
  delay.set_parent(&rig.controller);
  delay.set_multiplier(10);
  delay.control(180);
  rig.client.loop();
  sent = decode(rig.client.tx.back());
  assert(sent.messages[0].value == 18);
  rig.client.ack_packet(reply(sent));
  delay.on_receive(18);
  assert(delay.state == 180);

  TestSelect select("select", 0x4066, ControllerMode::Control, &hydro);
  select.set_parent(&rig.controller);
  select.set_offset(1);
  select.traits.options = {"Economy", "Standard", "Power"};
  select.control("Standard");
  rig.client.loop();
  sent = decode(rig.client.tx.back());
  assert(sent.messages[0].value == 2);
  rig.client.ack_packet(reply(sent));
  select.on_receive(3);
  assert(select.state == "Power");
  select.on_receive(0);
  select.on_receive(4);
  assert(select.state == "Power");

  NasactlSensor temperature("temperature", 0x4237, ControllerMode::Status, &hydro);
  temperature.set_signed(true);
  temperature.set_divisor(10);
  temperature.on_receive(65511);
  assert(temperature.state == -2.5f);
  NasactlBinarySensor binary("binary", 0x4000, ControllerMode::Status, &hydro);
  binary.on_receive(1);
  assert(binary.state);
  binary.on_receive(0);
  assert(!binary.state);
  NasactlTextSensor text("text", 0x4001, ControllerMode::Status, &hydro);
  text.add_mapping(3, "Fan");
  text.on_receive(3);
  text.on_receive(3);
  assert(text.state == "Fan" && text.publications.size() == 1);
  text.on_receive(9);
  assert(text.state == "Unknown (9)");
}

static void startup_polling_and_uart_faults() {
  NasaClient client;
  NasaController controller(&client);
  NasaDevice device("20.00.AA", 0x20);
  assert(device.get_address() == "20.00.aa");
  Entity entity(0x4269, &device, ControllerMode::FSV);
  now = 0;
  client.setup();
  controller.register_device(&device);
  controller.register_component(&entity);
  controller.set_fsv_interval(1000);
  controller.setup();
  now = 200;
  client.loop();
  client.loop();
  assert(client.tx.size() == 1 && decode(client.tx.back()).command.data_type == DataType::Read);
  now = 4999;
  controller.update();
  client.loop();
  assert(client.tx.size() == 1);
  now = 5000;
  controller.update();
  client.loop();
  client.loop();
  assert(client.tx.size() == 2);
  controller.update();  // Poll completion.
  now += 1000;
  controller.update();
  client.loop();
  client.loop();
  assert(client.tx.size() == 3);  // Periodic poll still runs.

  auto response = reply(decode(client.tx.back()), DataType::Response);
  response.source = device.get_parsed_address();
  response.messages = {message(0x4269, 10)};
  auto frame = response.encode();
  // Noise, CRC failure, then a byte-fragmented valid response.
  client.rx.insert(client.rx.end(), {0x00, 0x55, 0xff});
  auto corrupt = frame;
  corrupt[corrupt.size() - 2] ^= 1;
  client.rx.insert(client.rx.end(), corrupt.begin(), corrupt.end());
  client.loop();
  assert(entity.state == -1);
  for (auto byte : frame) {
    client.rx.push_back(byte);
    client.loop();
  }
  assert(entity.state == 10);
  response.messages[0].value = 11;
  auto first = response.encode();
  response.messages[0].value = 12;
  auto second = response.encode();
  client.rx.insert(client.rx.end(), first.begin(), first.end());
  client.rx.insert(client.rx.end(), second.begin(), second.end());
  client.loop();
  assert(entity.state == 12);  // Consecutive frames are processed in order.
}

static void raw_and_wire_compatibility() {
  NasaClient client;
  now = 1000;
  client.setup();
  client.send_write(Address::parse("20.00.00"), 0x4000, 1);
  client.loop();
  assert(client.tx.back()[9] == 0x20 && client.tx.back()[10] == 0x12);
  client.ack_packet(reply(decode(client.tx.back())));
  client.set_standard_command_header(true);
  client.set_packet_information(true);
  client.send_write(Address::parse("20.00.00"), 0x4000, 1);
  client.loop();
  assert(client.tx.back()[9] == 0xc0);
  client.ack_packet(reply(decode(client.tx.back())));
  auto packet = Packet::create_write(Address::parse("20.00.00"), 0x4000, 0);
  packet.command.packet_information = false;
  auto raw = packet.encode(false);
  auto expected = raw;
  assert(client.queue_packet(packet.command.packet_number, std::move(raw)));
  client.loop();
  assert(client.tx.back() == expected);  // Raw API never rewrites header or CRC.
  assert(!client.queue_packet(packet.command.packet_number, std::vector<uint8_t>(expected)));
  client.ack_packet(reply(decode(expected)));

  // Queue-number collision after wrap is resolved before transmission.
  auto a = Packet::create_write(Address::parse("20.00.00"), 0x4000, 1);
  auto b = Packet::create_write(a.destination, 0x4001, 3);
  a.command.packet_number = b.command.packet_number = 0x42;
  client.queue_packet(a, true);
  client.queue_packet(b, true);
  client.loop();
  auto sent_a = decode(client.tx.back());
  client.ack_packet(reply(sent_a));
  client.loop();
  auto sent_b = decode(client.tx.back());
  assert(sent_a.command.packet_number != sent_b.command.packet_number);
}

int main() {
  protocol_fixtures();
  malformed_and_random_packets();
  batches();
  acknowledgements();
  timeouts_and_wrap();
  reads_and_diagnostics();
  for (auto outcome : {WriteOutcome::Acknowledged, WriteOutcome::Rejected,
                       WriteOutcome::TimedOut, WriteOutcome::QueueRejected}) reconciliation(outcome);
  readback_coalescing_and_pressure();
  climate_calls();
  hydro_entity_regressions();
  startup_polling_and_uart_faults();
  raw_and_wire_compatibility();
  std::cout << "All NASA transport, readback and climate host tests passed\n";
}
