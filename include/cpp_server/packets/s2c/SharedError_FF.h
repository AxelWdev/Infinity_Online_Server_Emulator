#pragma once

#include <cstdint>
#include <span>

#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct SharedError_FF {
    static constexpr std::uint8_t kOpcode = 0xFF;

    std::uint16_t selector_opcode{};
    std::uint16_t error_code{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        ByteWriter writer;
        writer.write_u16(selector_opcode);
        writer.write_u16(error_code);
        return writer.take();
    }

    static SharedError_FF deserialize(std::span<const std::uint8_t> payload) {
        ByteReader reader(payload);
        SharedError_FF packet;
        packet.selector_opcode = reader.read_u16();
        packet.error_code = reader.read_u16();
        return packet;
    }
};

}  // namespace cpp_server::packets::s2c
