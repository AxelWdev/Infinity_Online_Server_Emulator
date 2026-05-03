#pragma once

#include <cstdint>
#include <span>
#include <stdexcept>

#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct GameServerConnect_10 {
    static constexpr std::uint8_t kOpcode = 0x10;

    std::uint32_t packed_ipv4{};
    std::uint16_t udp_port{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        ByteWriter writer;
        writer.write_u32(packed_ipv4);
        writer.write_u16(udp_port);
        return writer.take();
    }

    static GameServerConnect_10 deserialize(std::span<const std::uint8_t> payload) {
        if (payload.size() != 6) {
            throw std::runtime_error("GameServerConnect_10 expects a 6-byte payload");
        }

        ByteReader reader(payload);
        GameServerConnect_10 packet;
        packet.packed_ipv4 = reader.read_u32();
        packet.udp_port = reader.read_u16();
        return packet;
    }
};

}  // namespace cpp_server::packets::s2c
