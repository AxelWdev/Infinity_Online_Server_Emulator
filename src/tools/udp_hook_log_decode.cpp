#include "cpp_server/core/ByteBuffer.h"
#include "cpp_server/core/Utf.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <regex>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::array<std::uint8_t, 8> kUdpDecodePermutation{3, 4, 1, 5, 7, 6, 0, 2};
constexpr std::array<std::uint8_t, 13> kUdpXorTable{
    0x3f, 0x26, 0x42, 0xfd, 0x8a, 0x72, 0x34, 0x74, 0xc8, 0xbf, 0xcd, 0xa4, 0x94};

struct DecodedUdpPacket {
    std::uint8_t opcode{};
    cpp_server::core::ByteVector payload{};
    bool checksum_ok{};
};

struct HookDatagram {
    std::size_t line_number{};
    std::string direction;
    std::string socket;
    std::string endpoint;
    cpp_server::core::ByteVector bytes;
};

std::uint16_t read_u16_le(std::span<const std::uint8_t> bytes) {
    return static_cast<std::uint16_t>(bytes[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[1]) << 8U);
}

std::uint32_t read_u32_le(std::span<const std::uint8_t> bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

std::string hex_u8(std::uint8_t value) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned>(value);
    return stream.str();
}

std::string hex_u32(std::uint32_t value) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(8) << value;
    return stream.str();
}

std::string hex_u16(std::uint16_t value) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(4) << value;
    return stream.str();
}

std::uint8_t udp_transport_check_byte(std::span<const std::uint8_t> bytes) {
    std::uint8_t sum = 0;
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        if (index == 2) {
            continue;
        }
        sum = static_cast<std::uint8_t>(sum + bytes[index]);
    }
    return static_cast<std::uint8_t>(static_cast<std::uint8_t>(bytes.size()) - sum + 0x78U);
}

bool udp_transport_checksum_ok(std::span<const std::uint8_t> bytes) {
    return bytes.size() >= 3 && bytes[2] == udp_transport_check_byte(bytes);
}

std::uint8_t decode_udp_header_length(std::uint8_t value) {
    return static_cast<std::uint8_t>(
        ((kUdpDecodePermutation[(value >> 2U) & 7U] * 4U) | (value & 3U)) * 8U |
        kUdpDecodePermutation[value >> 5U]);
}

std::uint8_t decode_udp_opcode(std::uint8_t encoded_opcode, std::uint8_t payload_len) {
    const auto value = static_cast<std::uint8_t>(kUdpXorTable[payload_len % kUdpXorTable.size()] ^ encoded_opcode);
    return static_cast<std::uint8_t>(
        ((kUdpDecodePermutation[(value & 0x1cU) >> 2U] << 5U) | kUdpDecodePermutation[value >> 5U]) & 0x3fU |
        ((value & 3U) << 3U));
}

std::optional<DecodedUdpPacket> decode_udp_inner_packet(std::span<const std::uint8_t> encoded) {
    if (encoded.size() < 3) {
        return std::nullopt;
    }

    const auto payload_len = decode_udp_header_length(encoded[0]);
    if (encoded.size() != static_cast<std::size_t>(payload_len) + 3U) {
        return std::nullopt;
    }

    const auto check_base = static_cast<std::uint8_t>(kUdpXorTable[payload_len % kUdpXorTable.size()] ^ encoded[1]);
    DecodedUdpPacket packet;
    packet.opcode = decode_udp_opcode(encoded[1], payload_len);
    packet.payload.resize(payload_len);

    std::uint8_t payload_check = 0;
    std::uint16_t xor_index = static_cast<std::uint16_t>(encoded[1] + 7U);
    for (std::size_t index = 0; index < payload_len; ++index) {
        const auto decoded = static_cast<std::uint8_t>(
            kUdpXorTable[xor_index % kUdpXorTable.size()] ^ encoded[index + 3U]);
        packet.payload[index] = decoded;
        payload_check = static_cast<std::uint8_t>(
            payload_check + ((index & 1U) == 0U ? static_cast<std::uint8_t>(-decoded) : decoded));
        ++xor_index;
    }

    packet.checksum_ok = static_cast<std::uint8_t>(encoded[2] ^ payload_check ^ check_base) == payload_len;
    return packet;
}

std::vector<DecodedUdpPacket> decode_udp_inner_stream(std::span<const std::uint8_t> encoded_stream) {
    std::vector<DecodedUdpPacket> packets;
    std::size_t offset = 0;
    while (offset < encoded_stream.size()) {
        if (encoded_stream.size() - offset < 3U) {
            break;
        }
        const auto payload_len = decode_udp_header_length(encoded_stream[offset]);
        const auto packet_len = static_cast<std::size_t>(payload_len) + 3U;
        if (packet_len > encoded_stream.size() - offset) {
            break;
        }
        if (const auto packet = decode_udp_inner_packet(encoded_stream.subspan(offset, packet_len))) {
            packets.push_back(*packet);
        } else {
            break;
        }
        offset += packet_len;
    }
    return packets;
}

std::string ipv4_from_packed_le(std::uint32_t packed_ipv4) {
    return std::to_string(packed_ipv4 & 0xffU) + "." +
           std::to_string((packed_ipv4 >> 8U) & 0xffU) + "." +
           std::to_string((packed_ipv4 >> 16U) & 0xffU) + "." +
           std::to_string((packed_ipv4 >> 24U) & 0xffU);
}

std::string decode_utf16_tail(std::span<const std::uint8_t> payload, std::size_t offset) {
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
    return cpp_server::core::DecodeUtf16Le(tail);
}

std::string describe_inner_packet(const DecodedUdpPacket& packet) {
    std::ostringstream stream;
    stream << " inner_opcode=0x" << hex_u8(packet.opcode)
           << " inner_len=" << packet.payload.size()
           << " inner_checksum=" << (packet.checksum_ok ? "ok" : "bad");

    if (packet.opcode == 0x00 && packet.payload.size() == 10) {
        const auto payload = std::span<const std::uint8_t>(packet.payload);
        const auto token = read_u32_le(payload.subspan(0, 4));
        const auto packed_ipv4 = read_u32_le(payload.subspan(4, 4));
        const auto port = read_u16_le(payload.subspan(8, 2));
        stream << " client_init token=0x" << hex_u32(token)
               << " advertised=" << ipv4_from_packed_le(packed_ipv4) << ":" << port;
    } else if (packet.opcode == 0x00 && packet.payload.size() == 4) {
        stream << " heartbeat_tick=" << read_u32_le(std::span<const std::uint8_t>(packet.payload));
    } else if (packet.opcode == 0x02 && packet.payload.size() >= 3) {
        const auto payload = std::span<const std::uint8_t>(packet.payload);
        stream << " room_header count_byte=" << static_cast<unsigned>(payload[0])
               << " rule_field=" << read_u16_le(payload.subspan(1, 2));
        const auto name = decode_utf16_tail(payload, 3);
        if (!name.empty()) {
            stream << " string='" << name << "'";
        }
    } else if (packet.opcode == 0x03 && packet.payload.size() >= 24U && !decode_utf16_tail(packet.payload, 24).empty()) {
        const auto payload = std::span<const std::uint8_t>(packet.payload);
        const auto motion = decode_utf16_tail(payload, 24);
        stream << " combat_action_event source_object_id=" << read_u16_le(payload.subspan(0, 2))
               << " target_object_id=" << read_u16_le(payload.subspan(2, 2))
               << " elapsed_tick=0x" << hex_u32(read_u32_le(payload.subspan(4, 4)))
               << " action_field_08=" << static_cast<unsigned>(payload[8])
               << " action_field_09=" << static_cast<unsigned>(payload[9])
               << " action_field_0a=" << static_cast<unsigned>(payload[10])
               << " motion_tag=0x" << hex_u16(read_u16_le(payload.subspan(11, 2)))
               << " motion_event_field_0a_source=0x" << hex_u16(read_u16_le(payload.subspan(13, 2)))
               << " action_field_0f=" << static_cast<unsigned>(payload[15])
               << " action_field_10=" << static_cast<unsigned>(payload[16])
               << " motion_event_field_0c=0x" << hex_u32(read_u32_le(payload.subspan(17, 4)))
               << " motion_event_field_10=" << read_u16_le(payload.subspan(21, 2))
               << " action_field_17=" << static_cast<unsigned>(payload[23])
               << " motion='" << motion << "'";
    } else if (packet.opcode == 0x03 && packet.payload.size() >= 0x1eU) {
        const auto payload = std::span<const std::uint8_t>(packet.payload);
        stream << " entity owner_context_id=" << read_u32_le(payload.subspan(0, 4))
               << " entity_object_id=" << read_u16_le(payload.subspan(4, 2))
               << " b6=" << static_cast<unsigned>(payload[6])
               << " category=" << static_cast<unsigned>(payload[7]);
    } else if (packet.opcode == 0x08 && packet.payload.size() >= 20U) {
        const auto payload = std::span<const std::uint8_t>(packet.payload);
        const auto motion = decode_utf16_tail(payload, 20);
        stream << " combat_motion_event source_object_id=" << read_u16_le(payload.subspan(0, 2))
               << " target_object_id=" << read_u16_le(payload.subspan(2, 2))
               << " elapsed_tick=0x" << hex_u32(read_u32_le(payload.subspan(4, 4)))
               << " field_08=0x" << hex_u16(read_u16_le(payload.subspan(8, 2)))
               << " motion_event_field_0a=0x" << hex_u16(read_u16_le(payload.subspan(10, 2)))
               << " motion_event_field_0c=0x" << hex_u32(read_u32_le(payload.subspan(12, 4)))
               << " motion_event_field_10=" << read_u16_le(payload.subspan(16, 2))
               << " motion_event_field_12=" << read_u16_le(payload.subspan(18, 2));
        if (!motion.empty()) {
            stream << " motion='" << motion << "'";
        }
    } else if (packet.opcode == 0x10 && packet.payload.size() == 11U) {
        const auto payload = std::span<const std::uint8_t>(packet.payload);
        stream << " combat_stat_update object_id=" << read_u16_le(payload.subspan(0, 2))
               << " life_force_current=" << read_u16_le(payload.subspan(2, 2))
               << " life_force_max=" << read_u16_le(payload.subspan(4, 2))
               << " spiritual_strength_current=" << read_u16_le(payload.subspan(6, 2))
               << " spiritual_strength_max=" << read_u16_le(payload.subspan(8, 2))
               << " status_byte=" << static_cast<unsigned>(payload[10]);
    } else if (packet.opcode == 0x11 && packet.payload.size() == 13U) {
        const auto payload = std::span<const std::uint8_t>(packet.payload);
        stream << " combat_hit_relation source_object_id=" << read_u16_le(payload.subspan(0, 2))
               << " target_object_id=" << read_u16_le(payload.subspan(2, 2))
               << " hit_index=" << read_u16_le(payload.subspan(4, 2))
               << " damage_or_delta=" << read_u16_le(payload.subspan(6, 2))
               << " field_08=" << static_cast<unsigned>(payload[8])
               << " field_09=" << static_cast<unsigned>(payload[9])
               << " field_0a=" << static_cast<unsigned>(payload[10])
               << " field_0b=" << static_cast<unsigned>(payload[11])
               << " field_0c=" << static_cast<unsigned>(payload[12]);
    } else if (packet.opcode == 0x17 && packet.payload.size() >= 4U) {
        const auto payload = std::span<const std::uint8_t>(packet.payload);
        const auto motion = decode_utf16_tail(payload, 4);
        stream << " combat_target_motion target_object_id=" << read_u16_le(payload.subspan(0, 2))
               << " motion_tag=0x" << hex_u16(read_u16_le(payload.subspan(2, 2)));
        if (!motion.empty()) {
            stream << " motion='" << motion << "'";
        }
    } else if (packet.opcode == 0x1e && packet.payload.size() == 4U) {
        const auto payload = std::span<const std::uint8_t>(packet.payload);
        stream << " combat_damage_notice source_object_id=" << read_u16_le(payload.subspan(0, 2))
               << " damage_or_delta=" << read_u16_le(payload.subspan(2, 2));
    } else if (packet.opcode == 0x3a && packet.payload.size() >= 20U) {
        const auto payload = std::span<const std::uint8_t>(packet.payload);
        const auto motion = decode_utf16_tail(payload, 20);
        stream << " macro_motion tick_sequence=" << read_u16_le(payload.subspan(0, 2))
               << " object_id=" << read_u16_le(payload.subspan(2, 2));
        if (!motion.empty()) {
            stream << " motion='" << motion << "'";
        }
    }

    stream << " payload=" << cpp_server::core::HexBytes(packet.payload);
    return stream.str();
}

std::string describe_ack_list(std::span<const std::uint8_t> bytes, std::uint16_t ack_count) {
    if (bytes.size() != 3U + static_cast<std::size_t>(ack_count) * 2U) {
        return " malformed_ack_count=" + std::to_string(ack_count);
    }

    std::ostringstream stream;
    stream << " ack_count=" << ack_count << " acked=[";
    for (std::uint16_t index = 0; index < ack_count; ++index) {
        if (index != 0) {
            stream << ",";
        }
        stream << read_u16_le(bytes.subspan(3U + static_cast<std::size_t>(index) * 2U, 2U));
    }
    stream << "]";
    return stream.str();
}

std::optional<HookDatagram> parse_hook_datagram(std::string_view line, std::size_t line_number) {
    static const std::regex pattern(
        R"(^\[UDP-HOOK\] (sendto|recvfrom) socket=([^ ]+) endpoint=([^ ]+) len=([0-9]+) hex=(.*)$)");

    std::cmatch match;
    const std::string line_copy(line);
    if (!std::regex_match(line_copy.c_str(), match, pattern)) {
        return std::nullopt;
    }

    HookDatagram datagram;
    datagram.line_number = line_number;
    datagram.direction = match[1].str();
    datagram.socket = match[2].str();
    datagram.endpoint = match[3].str();
    datagram.bytes = cpp_server::core::ParseHexString(match[5].str());

    const auto expected_len = static_cast<std::size_t>(std::stoul(match[4].str()));
    if (datagram.bytes.size() != expected_len) {
        throw std::runtime_error("line " + std::to_string(line_number) + " hex length mismatch");
    }
    return datagram;
}

void print_usage() {
    std::cout << "udp_hook_log_decode <udp_hook_traffic.log> [output.txt]\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2 || argc > 3 || std::string_view(argv[1]) == "--help" || std::string_view(argv[1]) == "-h") {
            print_usage();
            return argc < 2 ? 1 : 0;
        }

        const std::filesystem::path input_path = argv[1];
        std::ifstream input(input_path);
        if (!input) {
            throw std::runtime_error("failed to open input log: " + input_path.string());
        }

        std::ofstream output_file;
        std::ostream* out = &std::cout;
        if (argc == 3) {
            const std::filesystem::path output_path = argv[2];
            if (!output_path.parent_path().empty()) {
                std::filesystem::create_directories(output_path.parent_path());
            }
            output_file.open(output_path, std::ios::out | std::ios::trunc);
            if (!output_file) {
                throw std::runtime_error("failed to open output report: " + output_path.string());
            }
            out = &output_file;
        }

        std::vector<HookDatagram> datagrams;
        std::string line;
        std::size_t line_number = 0;
        while (std::getline(input, line)) {
            ++line_number;
            if (auto datagram = parse_hook_datagram(line, line_number)) {
                datagrams.push_back(std::move(*datagram));
            }
        }

        std::size_t send_count = 0;
        std::size_t recv_count = 0;
        for (const auto& datagram : datagrams) {
            if (datagram.direction == "sendto") {
                ++send_count;
            } else if (datagram.direction == "recvfrom") {
                ++recv_count;
            }
        }

        *out << "# UDP Hook Decode Report\n\n";
        *out << "input=" << input_path.string() << "\n";
        *out << "datagrams=" << datagrams.size()
             << " sendto=" << send_count
             << " recvfrom=" << recv_count << "\n\n";

        for (std::size_t index = 0; index < datagrams.size(); ++index) {
            const auto& datagram = datagrams[index];
            const auto bytes = std::span<const std::uint8_t>(datagram.bytes);
            *out << std::setw(4) << index << " line=" << datagram.line_number
                 << " " << datagram.direction
                 << " socket=" << datagram.socket
                 << " endpoint=" << datagram.endpoint
                 << " len=" << bytes.size();

            if (bytes.size() < 3) {
                *out << " raw_short bytes=" << cpp_server::core::HexBytes(bytes) << "\n";
                continue;
            }

            const bool checksum_ok = udp_transport_checksum_ok(bytes);
            const auto transport_word = read_u16_le(bytes);
            const auto kind = static_cast<std::uint8_t>(transport_word & 7U);
            const auto sequence_base = static_cast<std::uint16_t>(transport_word & 0xfff8U);
            *out << " kind=" << static_cast<unsigned>(kind)
                 << " seq=" << sequence_base
                 << " transport_checksum=" << (checksum_ok ? "ok" : "bad");

            if (!checksum_ok) {
                *out << " expected_check=0x" << hex_u8(udp_transport_check_byte(bytes))
                     << " bytes=" << cpp_server::core::HexBytes(bytes) << "\n";
                continue;
            }

            if (kind == 0 || kind == 1) {
                const auto packets = decode_udp_inner_stream(bytes.subspan(3));
                *out << " inner_count=" << packets.size();
                for (std::size_t packet_index = 0; packet_index < packets.size(); ++packet_index) {
                    *out << "\n       [" << packet_index << "]" << describe_inner_packet(packets[packet_index]);
                }
                *out << "\n";
            } else if (kind == 2) {
                if (const auto packet = decode_udp_inner_packet(bytes.subspan(3))) {
                    *out << describe_inner_packet(*packet) << "\n";
                } else {
                    *out << " reliable_inner_decode=failed bytes=" << cpp_server::core::HexBytes(bytes.subspan(3))
                         << "\n";
                }
            } else if (kind == 3) {
                *out << " probe\n";
            } else if (kind == 4 || kind == 5) {
                *out << describe_ack_list(bytes, static_cast<std::uint16_t>(transport_word >> 3U)) << "\n";
            } else if (kind == 6) {
                *out << " probe_reply\n";
            } else {
                *out << " unknown_transport_kind bytes=" << cpp_server::core::HexBytes(bytes) << "\n";
            }
        }

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[error] " << ex.what() << '\n';
        return 1;
    }
}
