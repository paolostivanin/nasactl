#include "nasa_packet.h"
#include "nasa_crc.h"

namespace nasactl {

Packet Packet::create_read(const Address &dest, const std::vector<uint16_t> &message_numbers) {
  Packet pkt;
  pkt.source = Address::my_address();
  pkt.destination = dest;
  pkt.command.packet_type = PacketType::Normal;
  pkt.command.data_type = DataType::Read;
  pkt.command.packet_number = Command::next_packet_number();

  for (auto msg_num : message_numbers) {
    MessageSet ms(msg_num);
    ms.value = 0;
    pkt.messages.push_back(ms);
  }
  return pkt;
}

Packet Packet::create_write(const Address &dest, uint16_t message_number, long value) {
  Packet pkt;
  pkt.source = Address::my_address();
  pkt.destination = dest;
  pkt.command.packet_type = PacketType::Normal;
  pkt.command.data_type = DataType::Write;
  pkt.command.packet_number = Command::next_packet_number();

  MessageSet ms(message_number);
  ms.value = value;
  pkt.messages.push_back(ms);
  return pkt;
}

DecodeResult Packet::decode(const std::vector<uint8_t> &data) {
  if (data.size() < 14)
    return DecodeResult::TooShort;

  if (data[0] != PACKET_START)
    return DecodeResult::InvalidStart;

  uint16_t size = (static_cast<uint16_t>(data[1]) << 8) | data[2];
  if (size > PACKET_MAX_SIZE || size + 2 > data.size())
    return DecodeResult::InvalidSize;

  // Total frame = start(1) + size(2) + payload(size-2) + crc(2) + end(1)
  // data length should be: size + 3 (start + size_hi + size_lo + ... + end)
  uint32_t frame_end = size + 1;  // index of end marker
  if (frame_end >= data.size())
    return DecodeResult::InvalidSize;

  if (data[frame_end] != PACKET_END)
    return DecodeResult::InvalidEnd;

  // Verify CRC (covers bytes from index 1 to frame_end-2, CRC is at frame_end-2..frame_end-1)
  uint16_t crc_offset = frame_end - 2;
  uint16_t received_crc = (static_cast<uint16_t>(data[crc_offset]) << 8) | data[crc_offset + 1];
  uint16_t computed_crc = crc16(data, 1, crc_offset - 1);
  if (received_crc != computed_crc)
    return DecodeResult::InvalidCRC;

  // Parse addresses and command
  uint32_t offset = 3;  // skip start + size
  source.decode(data, offset);       // 3 bytes
  destination.decode(data, offset);  // 3 bytes
  command.decode(data, offset);      // 3 bytes

  // Parse messages
  uint8_t msg_count = data[offset];
  offset++;

  messages.clear();
  for (uint8_t i = 0; i < msg_count && offset < crc_offset; i++) {
    MessageSet ms;
    ms.decode(data, offset);
    messages.push_back(ms);
  }

  return DecodeResult::Ok;
}

std::vector<uint8_t> Packet::encode() const {
  std::vector<uint8_t> payload;

  // Source address (3 bytes)
  auto src = source.encode();
  payload.insert(payload.end(), src.begin(), src.end());

  // Destination address (3 bytes)
  auto dst = destination.encode();
  payload.insert(payload.end(), dst.begin(), dst.end());

  // Command (3 bytes)
  auto cmd = command.encode();
  payload.insert(payload.end(), cmd.begin(), cmd.end());

  // Message count
  payload.push_back(static_cast<uint8_t>(messages.size()));

  // Messages
  for (const auto &msg : messages) {
    msg.encode(payload);
  }

  // Build frame: start + size + payload + crc + end
  std::vector<uint8_t> frame;
  frame.push_back(PACKET_START);

  // Size = 2 (size field itself) + payload + 2 (crc)
  uint16_t size = 2 + payload.size() + 2;
  frame.push_back(static_cast<uint8_t>(size >> 8));
  frame.push_back(static_cast<uint8_t>(size & 0xFF));

  frame.insert(frame.end(), payload.begin(), payload.end());

  // CRC over everything from index 1 (size fields + payload)
  uint16_t crc = crc16(frame, 1, frame.size() - 1);
  frame.push_back(static_cast<uint8_t>(crc >> 8));
  frame.push_back(static_cast<uint8_t>(crc & 0xFF));

  frame.push_back(PACKET_END);

  return frame;
}

std::string Packet::to_string() const {
  char buf[128];
  snprintf(buf, sizeof(buf), "%s -> %s [%s] msgs=%zu",
           source.to_string().c_str(),
           destination.to_string().c_str(),
           command.to_string().c_str(),
           messages.size());
  return std::string(buf);
}

}  // namespace nasactl
