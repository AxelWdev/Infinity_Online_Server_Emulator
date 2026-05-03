#pragma once

#include <cstdint>
#include <span>

#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct UpdateItemList_3F {
    static constexpr std::uint8_t kOpcode = 0x3F;

    std::uint32_t field_00{};
    std::uint32_t field_04{};
    std::uint32_t field_08{};
    std::uint32_t field_0c{};
    std::uint32_t field_10{};
    std::uint32_t field_14{};
    std::uint32_t field_18{};
    std::uint32_t field_1c{};
    std::uint32_t field_20{};
    std::uint32_t field_24{};
    std::uint16_t field_28{};
    std::uint32_t field_2a{};
    std::uint32_t field_2e{};
    std::uint8_t field_32{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        ByteWriter writer;
        writer.write_u32(field_00);
        writer.write_u32(field_04);
        writer.write_u32(field_08);
        writer.write_u32(field_0c);
        writer.write_u32(field_10);
        writer.write_u32(field_14);
        writer.write_u32(field_18);
        writer.write_u32(field_1c);
        writer.write_u32(field_20);
        writer.write_u32(field_24);
        writer.write_u16(field_28);
        writer.write_u32(field_2a);
        writer.write_u32(field_2e);
        writer.write_u8(field_32);
        return writer.take();
    }

    static UpdateItemList_3F deserialize(std::span<const std::uint8_t> payload) {
        ByteReader reader(payload);
        UpdateItemList_3F packet;
        packet.field_00 = reader.read_u32();
        packet.field_04 = reader.read_u32();
        packet.field_08 = reader.read_u32();
        packet.field_0c = reader.read_u32();
        packet.field_10 = reader.read_u32();
        packet.field_14 = reader.read_u32();
        packet.field_18 = reader.read_u32();
        packet.field_1c = reader.read_u32();
        packet.field_20 = reader.read_u32();
        packet.field_24 = reader.read_u32();
        packet.field_28 = reader.read_u16();
        packet.field_2a = reader.read_u32();
        packet.field_2e = reader.read_u32();
        packet.field_32 = reader.read_u8();
        return packet;
    }
};

}  // namespace cpp_server::packets::s2c
