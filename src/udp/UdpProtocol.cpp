#include "cpp_server/udp/UdpProtocol.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "cpp_server/core/PacketLog.h"
#include "cpp_server/core/Utf.h"

namespace cpp_server::udp {

namespace {

constexpr std::array<std::uint8_t, 8> kDecodePermutation{3, 4, 1, 5, 7, 6, 0, 2};
constexpr std::array<std::uint8_t, 8> kEncodePermutation{6, 2, 7, 0, 1, 3, 5, 4};
constexpr std::array<std::uint8_t, 13> kXorTable{
    0x3f, 0x26, 0x42, 0xfd, 0x8a, 0x72, 0x34, 0x74, 0xc8, 0xbf, 0xcd, 0xa4, 0x94};

std::uint8_t DecodeHeaderLength(std::uint8_t value) {
    return static_cast<std::uint8_t>(
        ((kDecodePermutation[(value >> 2U) & 7U] * 4U) | (value & 3U)) * 8U |
        kDecodePermutation[value >> 5U]);
}

std::uint8_t DecodeOpcode(std::uint8_t encoded_opcode, std::uint8_t payload_len) {
    const auto value = static_cast<std::uint8_t>(kXorTable[payload_len % kXorTable.size()] ^ encoded_opcode);
    return static_cast<std::uint8_t>(
        ((kDecodePermutation[(value & 0x1cU) >> 2U] << 5U) | kDecodePermutation[value >> 5U]) & 0x3fU |
        ((value & 3U) << 3U));
}

std::uint8_t EncodeHeaderLength(std::uint8_t payload_len) {
    return static_cast<std::uint8_t>(
        ((kEncodePermutation[payload_len & 7U] * 8U) | kEncodePermutation[payload_len >> 5U]) * 4U |
        ((payload_len >> 3U) & 3U));
}

std::uint8_t FindEncodedOpcode(std::uint8_t opcode, std::uint8_t payload_len) {
    for (std::uint16_t candidate = 0; candidate <= 0xffU; ++candidate) {
        if (DecodeOpcode(static_cast<std::uint8_t>(candidate), payload_len) == opcode) {
            return static_cast<std::uint8_t>(candidate);
        }
    }
    throw std::runtime_error("unable to encode UDP inner opcode 0x" + core::OpcodeHex(opcode));
}

}  // namespace

std::uint16_t ReadU16Le(std::span<const std::uint8_t> bytes) {
    return static_cast<std::uint16_t>(bytes[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[1]) << 8U);
}

std::uint32_t ReadU32Le(std::span<const std::uint8_t> bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

void AppendU16Le(core::ByteVector& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
}

void AppendU32Le(core::ByteVector& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xffU));
}

void AppendF32Le(core::ByteVector& bytes, float value) {
    std::uint32_t raw{};
    static_assert(sizeof(raw) == sizeof(value));
    std::memcpy(&raw, &value, sizeof(raw));
    AppendU32Le(bytes, raw);
}

float ReadF32Le(std::span<const std::uint8_t> bytes) {
    const auto raw = ReadU32Le(bytes);
    float value{};
    static_assert(sizeof(raw) == sizeof(value));
    std::memcpy(&value, &raw, sizeof(value));
    return value;
}

void AppendUtf16Z(core::ByteVector& bytes, std::string_view text) {
    const auto encoded = core::EncodeUtf16Le(text, true);
    bytes.insert(bytes.end(), encoded.begin(), encoded.end());
}

std::string Ipv4FromPackedLe(std::uint32_t packed_ipv4) {
    return std::to_string(packed_ipv4 & 0xffU) + "." +
           std::to_string((packed_ipv4 >> 8U) & 0xffU) + "." +
           std::to_string((packed_ipv4 >> 16U) & 0xffU) + "." +
           std::to_string((packed_ipv4 >> 24U) & 0xffU);
}

std::string HexU32(std::uint32_t value) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(8) << value;
    return stream.str();
}

std::string HexU16(std::uint16_t value) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(4) << value;
    return stream.str();
}

std::string HexU8(std::uint8_t value) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned int>(value);
    return stream.str();
}

std::string FormatF32(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3) << value;
    return stream.str();
}

std::vector<std::string> DecodeUtf16ZStrings(
    std::span<const std::uint8_t> payload,
    std::size_t offset,
    std::size_t maximum_count) {
    std::vector<std::string> strings;
    while (offset + 1U < payload.size() && strings.size() < maximum_count) {
        const auto start = offset;
        while (offset + 1U < payload.size() && (payload[offset] != 0 || payload[offset + 1U] != 0)) {
            offset += 2U;
        }

        if (offset == start) {
            strings.push_back({});
        } else {
            strings.push_back(core::DecodeUtf16Le(payload.subspan(start, offset - start)));
        }

        if (offset + 1U >= payload.size()) {
            break;
        }
        offset += 2U;
    }
    return strings;
}

std::string EndpointKey(const sockaddr_in& address) {
    char sender_ip[INET_ADDRSTRLEN]{};
    if (inet_ntop(AF_INET, &address.sin_addr, sender_ip, sizeof(sender_ip)) == nullptr) {
        std::snprintf(sender_ip, sizeof(sender_ip), "unknown");
    }
    return std::string(sender_ip) + ":" + std::to_string(ntohs(address.sin_port));
}

std::uint8_t TransportCheckByte(std::span<const std::uint8_t> bytes) {
    std::uint8_t sum = 0;
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        if (index == 2) {
            continue;
        }
        sum = static_cast<std::uint8_t>(sum + bytes[index]);
    }
    return static_cast<std::uint8_t>(static_cast<std::uint8_t>(bytes.size()) - sum + 0x78U);
}

bool TransportChecksumOk(std::span<const std::uint8_t> bytes) {
    return bytes.size() >= 3 && bytes[2] == TransportCheckByte(bytes);
}

core::ByteVector BuildProbeReply(std::uint16_t incoming_word) {
    const auto reply_word = static_cast<std::uint16_t>((incoming_word & 0xfffeU) | 0x0006U);
    core::ByteVector reply{static_cast<std::uint8_t>(reply_word & 0xffU),
                           static_cast<std::uint8_t>((reply_word >> 8U) & 0xffU), 0};
    reply[2] = TransportCheckByte(reply);
    return reply;
}

core::ByteVector BuildAckList(std::uint16_t sequence_base, std::uint8_t ack_kind) {
    const auto reply_word = static_cast<std::uint16_t>((1U << 3U) | (ack_kind & 7U));
    core::ByteVector reply{static_cast<std::uint8_t>(reply_word & 0xffU),
                           static_cast<std::uint8_t>((reply_word >> 8U) & 0xffU),
                           0,
                           static_cast<std::uint8_t>(sequence_base & 0xffU),
                           static_cast<std::uint8_t>((sequence_base >> 8U) & 0xffU)};
    reply[2] = TransportCheckByte(reply);
    return reply;
}

core::ByteVector EncodeInnerPacket(std::uint8_t opcode, std::span<const std::uint8_t> payload) {
    if (payload.size() > 0xffU) {
        throw std::runtime_error("UDP inner payload is too long");
    }

    const auto payload_len = static_cast<std::uint8_t>(payload.size());
    core::ByteVector encoded(payload.size() + 3U, 0);
    encoded[0] = EncodeHeaderLength(payload_len);
    encoded[1] = FindEncodedOpcode(opcode, payload_len);

    std::uint8_t payload_check = 0;
    std::uint16_t xor_index = static_cast<std::uint16_t>(encoded[1] + 7U);
    for (std::size_t index = 0; index < payload.size(); ++index) {
        const auto decoded = payload[index];
        payload_check = static_cast<std::uint8_t>(
            payload_check + ((index & 1U) == 0U ? static_cast<std::uint8_t>(-decoded) : decoded));
        encoded[index + 3U] = static_cast<std::uint8_t>(
            kXorTable[xor_index % kXorTable.size()] ^ decoded);
        ++xor_index;
    }

    const auto check_base = static_cast<std::uint8_t>(kXorTable[payload_len % kXorTable.size()] ^ encoded[1]);
    encoded[2] = static_cast<std::uint8_t>(payload_check ^ check_base ^ payload_len);
    return encoded;
}

core::ByteVector BuildReliableFrame(std::uint16_t sequence_base, std::span<const std::uint8_t> inner_packet) {
    const auto transport_word = static_cast<std::uint16_t>((sequence_base & 0xfff8U) | 2U);
    core::ByteVector frame;
    frame.reserve(inner_packet.size() + 3U);
    AppendU16Le(frame, transport_word);
    frame.push_back(0);
    frame.insert(frame.end(), inner_packet.begin(), inner_packet.end());
    frame[2] = TransportCheckByte(frame);
    return frame;
}

core::ByteVector BuildSequencedFrame(std::uint16_t sequence_base, std::span<const std::uint8_t> inner_packet) {
    const auto transport_word = static_cast<std::uint16_t>((sequence_base & 0xfff8U) | 1U);
    core::ByteVector frame;
    frame.reserve(inner_packet.size() + 3U);
    AppendU16Le(frame, transport_word);
    frame.push_back(0);
    frame.insert(frame.end(), inner_packet.begin(), inner_packet.end());
    frame[2] = TransportCheckByte(frame);
    return frame;
}

core::ByteVector BuildUnsequencedFrame(std::span<const std::uint8_t> inner_packet) {
    core::ByteVector frame;
    frame.reserve(inner_packet.size() + 3U);
    AppendU16Le(frame, 0);
    frame.push_back(0);
    frame.insert(frame.end(), inner_packet.begin(), inner_packet.end());
    frame[2] = TransportCheckByte(frame);
    return frame;
}

std::optional<DecodedPacket> DecodeInnerPacket(std::span<const std::uint8_t> encoded) {
    if (encoded.size() < 3) {
        return std::nullopt;
    }

    const auto payload_len = DecodeHeaderLength(encoded[0]);
    if (encoded.size() != static_cast<std::size_t>(payload_len) + 3U) {
        return std::nullopt;
    }

    const auto check_base = static_cast<std::uint8_t>(kXorTable[payload_len % kXorTable.size()] ^ encoded[1]);
    DecodedPacket packet;
    packet.opcode = DecodeOpcode(encoded[1], payload_len);
    packet.payload.resize(payload_len);

    std::uint8_t payload_check = 0;
    std::uint16_t xor_index = static_cast<std::uint16_t>(encoded[1] + 7U);
    for (std::size_t index = 0; index < payload_len; ++index) {
        const auto decoded = static_cast<std::uint8_t>(
            kXorTable[xor_index % kXorTable.size()] ^ encoded[index + 3U]);
        packet.payload[index] = decoded;
        payload_check = static_cast<std::uint8_t>(
            payload_check + ((index & 1U) == 0U ? static_cast<std::uint8_t>(-decoded) : decoded));
        ++xor_index;
    }

    packet.checksum_ok = static_cast<std::uint8_t>(encoded[2] ^ payload_check ^ check_base) == payload_len;
    return packet;
}

std::vector<DecodedPacket> DecodeInnerStream(std::span<const std::uint8_t> encoded_stream) {
    std::vector<DecodedPacket> packets;
    std::size_t offset = 0;
    while (offset < encoded_stream.size()) {
        if (encoded_stream.size() - offset < 3U) {
            break;
        }
        const auto payload_len = DecodeHeaderLength(encoded_stream[offset]);
        const auto packet_len = static_cast<std::size_t>(payload_len) + 3U;
        if (packet_len > encoded_stream.size() - offset) {
            break;
        }
        if (const auto packet = DecodeInnerPacket(encoded_stream.subspan(offset, packet_len))) {
            packets.push_back(*packet);
        } else {
            break;
        }
        offset += packet_len;
    }
    return packets;
}

std::string DecodeUtf16Tail(std::span<const std::uint8_t> payload, std::size_t offset) {
    if (payload.size() <= offset || ((payload.size() - offset) % 2U) != 0U) {
        return {};
    }

    auto tail = payload.subspan(offset);
    if (tail.size() >= 2U && tail[tail.size() - 2U] == 0 && tail[tail.size() - 1U] == 0) {
        tail = tail.first(tail.size() - 2U);
    }
    if (tail.empty()) {
        return {};
    }
    return core::DecodeUtf16Le(tail);
}

std::string DescribeInnerPayload(std::uint8_t opcode, std::span<const std::uint8_t> payload) {
    if (opcode == 0x00 && payload.size() == 10) {
        const auto token = ReadU32Le(payload.subspan(0, 4));
        const auto packed_ipv4 = ReadU32Le(payload.subspan(4, 4));
        const auto port = ReadU16Le(payload.subspan(8, 2));
        return " meaning=client-rudp-init token=0x" + HexU32(token) +
               " advertised_ipv4=" + Ipv4FromPackedLe(packed_ipv4) +
               " advertised_port=" + std::to_string(port);
    }

    if (opcode == 0x00 && payload.size() == 4) {
        const auto tick = ReadU32Le(payload);
        return " meaning=client-rudp-heartbeat tick_delta=" + std::to_string(tick);
    }

    if (opcode == 0x01 && payload.empty()) {
        return " meaning=client-ready";
    }

    if (opcode == 0x02 && payload.size() == 4U) {
        return " meaning=current-packet-or-timing-request value=" + std::to_string(ReadU32Le(payload));
    }

    if (opcode == 0x02 && payload.size() >= 5U) {
        const auto scene = DecodeUtf16Tail(payload, 3);
        return " meaning=room-header announced_entities=" + std::to_string(payload[0]) +
               " rule_field=" + std::to_string(ReadU16Le(payload.subspan(1, 2))) +
               (scene.empty() ? "" : " scene='" + scene + "'");
    }

    if (opcode == 0x03 && payload.size() >= 60U) {
        const auto strings = DecodeUtf16ZStrings(payload, 30, 3);
        std::string resource;
        std::string display_name;
        std::string group_name;
        if (!strings.empty()) {
            resource = strings[0];
        }
        if (strings.size() > 1U) {
            display_name = strings[1];
        }
        if (strings.size() > 2U) {
            group_name = strings[2];
        }
        return " meaning=entity-snapshot object_id=" + std::to_string(ReadU16Le(payload.subspan(4, 2))) +
               " category=" + std::to_string(payload[7]) +
               " state=[" + std::to_string(payload[8]) + "," + std::to_string(payload[9]) + "," +
               std::to_string(payload[10]) + "," + std::to_string(payload[11]) + "]" +
               " pos=(" + FormatF32(ReadF32Le(payload.subspan(18, 4))) + "," +
               FormatF32(ReadF32Le(payload.subspan(22, 4))) + "," +
               FormatF32(ReadF32Le(payload.subspan(26, 4))) + ")" +
               (resource.empty() ? "" : " resource='" + resource + "'") +
               (display_name.empty() ? "" : " name='" + display_name + "'") +
               (group_name.empty() ? "" : " group='" + group_name + "'");
    }

    if (opcode == 0x03 && payload.size() >= 24U) {
        const auto motion = DecodeUtf16Tail(payload, 24);
        if (!motion.empty()) {
            return " meaning=combat-action source=" + std::to_string(ReadU16Le(payload.subspan(0, 2))) +
                   " target=" + std::to_string(ReadU16Le(payload.subspan(2, 2))) +
                   " elapsed_tick=" + std::to_string(ReadU32Le(payload.subspan(4, 4))) +
                   " action_fields=[" + std::to_string(payload[8]) + "," + std::to_string(payload[9]) + "," +
                   std::to_string(payload[10]) + "]" +
                   " motion_tag=0x" + HexU16(ReadU16Le(payload.subspan(11, 2))) +
                   " event_field_0a=0x" + HexU16(ReadU16Le(payload.subspan(13, 2))) +
                   " action_0f_10=" + std::to_string(payload[15]) + "," + std::to_string(payload[16]) +
                   " event_field_0c=0x" + HexU32(ReadU32Le(payload.subspan(17, 4))) +
                   " event_field_10=" + std::to_string(ReadU16Le(payload.subspan(21, 2))) +
                   " motion='" + motion + "'";
        }
    }

    if (opcode == 0x04 && payload.empty()) {
        return " meaning=initial-sync-complete";
    }

    if (opcode == 0x05 && payload.size() == 4U) {
        return " meaning=start-elapsed elapsed_ms=" + std::to_string(ReadU32Le(payload));
    }

    if (opcode == 0x06 && payload.size() == 4U) {
        return " meaning=current-packet-or-timing-reply value=" + std::to_string(ReadU32Le(payload));
    }

    if (opcode == 0x07 && payload.size() == 8U) {
        return " meaning=game-state-tick elapsed_ms=" + std::to_string(ReadU32Le(payload.subspan(0, 4))) +
               " remaining_ms=" + std::to_string(ReadU32Le(payload.subspan(4, 4)));
    }

    if (opcode == 0x07 && payload.size() >= 2U) {
        return " meaning=client-clear-life-spirit object=" + std::to_string(ReadU16Le(payload.subspan(0, 2)));
    }

    if (opcode == 0x08 && payload.size() >= 20U) {
        const auto motion = DecodeUtf16Tail(payload, 20);
        return " meaning=combat-motion-event source=" + std::to_string(ReadU16Le(payload.subspan(0, 2))) +
               " target=" + std::to_string(ReadU16Le(payload.subspan(2, 2))) +
               " elapsed_tick=" + std::to_string(ReadU32Le(payload.subspan(4, 4))) +
               " motion_tag=0x" + HexU16(ReadU16Le(payload.subspan(8, 2))) +
               " event_field_0a=0x" + HexU16(ReadU16Le(payload.subspan(10, 2))) +
               " event_field_0c=0x" + HexU32(ReadU32Le(payload.subspan(12, 4))) +
               " event_field_10=" + std::to_string(ReadU16Le(payload.subspan(16, 2))) +
               " event_field_12=" + std::to_string(ReadU16Le(payload.subspan(18, 2))) +
               (motion.empty() ? "" : " motion='" + motion + "'");
    }

    if (opcode == 0x0a && payload.empty()) {
        return " meaning=session-start";
    }

    if (opcode == 0x10 && payload.size() == 11U) {
        return " meaning=combat-stat object=" + std::to_string(ReadU16Le(payload.subspan(0, 2))) +
               " life=" + std::to_string(ReadU16Le(payload.subspan(2, 2))) + "/" +
               std::to_string(ReadU16Le(payload.subspan(4, 2))) +
               " spirit=" + std::to_string(ReadU16Le(payload.subspan(6, 2))) + "/" +
               std::to_string(ReadU16Le(payload.subspan(8, 2))) +
               " status=" + std::to_string(payload[10]);
    }

    if (opcode == 0x11 && payload.size() == 13U) {
        return " meaning=combat-hit-relation source=" + std::to_string(ReadU16Le(payload.subspan(0, 2))) +
               " target=" + std::to_string(ReadU16Le(payload.subspan(2, 2))) +
               " hit_index=" + std::to_string(ReadU16Le(payload.subspan(4, 2))) +
               " damage_or_delta=" + std::to_string(ReadU16Le(payload.subspan(6, 2))) +
               " fields=[" + std::to_string(payload[8]) + "," + std::to_string(payload[9]) + "," +
               std::to_string(payload[10]) + "," + std::to_string(payload[11]) + "," +
               std::to_string(payload[12]) + "]";
    }

    if (opcode == 0x17 && payload.size() >= 4U) {
        const auto motion = DecodeUtf16Tail(payload, 4);
        return " meaning=combat-target-motion target=" + std::to_string(ReadU16Le(payload.subspan(0, 2))) +
               " motion_tag=0x" + HexU16(ReadU16Le(payload.subspan(2, 2))) +
               (motion.empty() ? "" : " motion='" + motion + "'");
    }

    if (opcode == 0x1e && payload.size() == 4U) {
        return " meaning=combat-damage-notice source=" + std::to_string(ReadU16Le(payload.subspan(0, 2))) +
               " damage_or_delta=" + std::to_string(ReadU16Le(payload.subspan(2, 2)));
    }

    if (opcode == 0x23 && payload.size() == 13U) {
        return " meaning=control-record object=" + std::to_string(ReadU16Le(payload.subspan(0, 2))) +
               " slot=" + std::to_string(payload[2]) +
               " value=" + std::to_string(payload[3]) +
               " field_04=0x" + HexU32(ReadU32Le(payload.subspan(4, 4))) +
               " field_08=0x" + HexU32(ReadU32Le(payload.subspan(8, 4))) +
               " flag=" + std::to_string(payload[12]);
    }

    if (opcode == 0x3a && payload.size() >= 20U) {
        const auto tick_sequence = ReadU16Le(payload.subspan(0, 2));
        const auto object_id = ReadU16Le(payload.subspan(2, 2));
        const auto motion = DecodeUtf16Tail(payload, 20);
        return " meaning=client-mission-macro-motion tick_sequence=" + std::to_string(tick_sequence) +
               " object_id=" + std::to_string(object_id) +
               " pos=(" + FormatF32(ReadF32Le(payload.subspan(4, 4))) + "," +
               FormatF32(ReadF32Le(payload.subspan(8, 4))) + "," +
               FormatF32(ReadF32Le(payload.subspan(12, 4))) + ")" +
               " motion_tag=0x" + HexU16(ReadU16Le(payload.subspan(16, 2))) +
               " field_12=0x" + HexU8(payload[18]) +
               " field_13=0x" + HexU8(payload[19]) +
               " field_12_13=0x" + HexU16(ReadU16Le(payload.subspan(18, 2))) +
               (motion.empty() ? "" : " motion='" + motion + "'");
    }

    if (opcode == 0x3a && payload.size() >= 4U) {
        return " meaning=client-mission-macro-motion tick_sequence=" +
               std::to_string(ReadU16Le(payload.subspan(0, 2))) +
               " object_id=" + std::to_string(ReadU16Le(payload.subspan(2, 2))) +
               " short_payload_len=" + std::to_string(payload.size());
    }

    if (opcode == 0x3c && payload.size() == 4U) {
        return " meaning=ultimate-or-special-request target=" + std::to_string(ReadU16Le(payload.subspan(0, 2))) +
               " player=" + std::to_string(ReadU16Le(payload.subspan(2, 2)));
    }

    return {};
}

std::string DescribeInnerPacket(const DecodedPacket& packet) {
    return DescribeInnerPayload(packet.opcode, packet.payload);
}

std::string DescribeInnerStreamDecode(std::span<const std::uint8_t> encoded_stream) {
    const auto packets = DecodeInnerStream(encoded_stream);
    if (packets.empty()) {
        return " decoded_inner_count=0";
    }

    std::ostringstream stream;
    stream << " decoded_inner_count=" << packets.size();
    for (std::size_t index = 0; index < packets.size(); ++index) {
        const auto& packet = packets[index];
        const auto description = DescribeInnerPacket(packet);
        stream << " inner[" << index << "]=opcode=0x" << core::OpcodeHex(packet.opcode)
               << " payload_len=" << packet.payload.size()
               << " checksum_ok=" << (packet.checksum_ok ? "true" : "false")
               << description;
        if (description.empty() || !packet.checksum_ok) {
            stream << " payload_hex=" << core::HexBytes(packet.payload);
        }
    }
    return stream.str();
}

std::string DescribeAckList(std::span<const std::uint8_t> payload, std::uint16_t ack_count) {
    if (payload.size() != 3U + static_cast<std::size_t>(ack_count) * 2U) {
        return " ack_count=" + std::to_string(ack_count) + " malformed_ack_payload_len=" +
               std::to_string(payload.size());
    }

    std::ostringstream stream;
    stream << " ack_count=" << ack_count << " acked_sequence_bases=[";
    for (std::uint16_t index = 0; index < ack_count; ++index) {
        if (index != 0) {
            stream << ",";
        }
        stream << ReadU16Le(payload.subspan(3U + static_cast<std::size_t>(index) * 2U, 2U));
    }
    stream << "]";
    return stream.str();
}

}  // namespace cpp_server::udp
