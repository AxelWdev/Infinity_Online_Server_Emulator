#pragma once

#include <cstdint>
#include <span>

#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct FullRoomStateSlot_85 {
    static constexpr std::uint8_t kOpcode = 0x85;

    std::uint8_t lookup_key_00{};
    std::uint16_t field_01{};
    std::uint16_t field_03{};
    std::uint32_t field_05{};
    std::uint32_t field_09{};
    std::uint32_t field_0d{};
    std::uint16_t field_11{};
    std::uint16_t field_13{};
    std::uint16_t field_15{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        ByteWriter writer;
        writer.write_u8(lookup_key_00);
        writer.write_u16(field_01);
        writer.write_u16(field_03);
        writer.write_u32(field_05);
        writer.write_u32(field_09);
        writer.write_u32(field_0d);
        writer.write_u16(field_11);
        writer.write_u16(field_13);
        writer.write_u16(field_15);
        return writer.take();
    }

    static FullRoomStateSlot_85 deserialize(std::span<const std::uint8_t> payload) {
        ByteReader reader(payload);
        FullRoomStateSlot_85 packet;
        packet.lookup_key_00 = reader.read_u8();
        packet.field_01 = reader.read_u16();
        packet.field_03 = reader.read_u16();
        packet.field_05 = reader.read_u32();
        packet.field_09 = reader.read_u32();
        packet.field_0d = reader.read_u32();
        packet.field_11 = reader.read_u16();
        packet.field_13 = reader.read_u16();
        packet.field_15 = reader.read_u16();
        return packet;
    }
};

}  // namespace cpp_server::packets::s2c
