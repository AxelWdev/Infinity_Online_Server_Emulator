#pragma once

#include <span>

#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct EquipItem_48 {
    static constexpr std::uint8_t kOpcode = 0x48;

    std::uint32_t field_00{};
    std::uint32_t field_04{};
    std::uint32_t field_08{};
    std::uint32_t field_0c{};
    std::uint32_t field_10{};
    std::uint32_t field_14{};
    std::uint32_t field_18{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        ByteWriter writer;
        writer.write_u32(field_00);
        writer.write_u32(field_04);
        writer.write_u32(field_08);
        writer.write_u32(field_0c);
        writer.write_u32(field_10);
        writer.write_u32(field_14);
        writer.write_u32(field_18);
        return writer.take();
    }

    static EquipItem_48 deserialize(std::span<const std::uint8_t> payload) {
        if (payload.size() != 28) {
            throw std::runtime_error("EquipItem_48 expects a 28-byte payload");
        }

        ByteReader reader(payload);
        EquipItem_48 packet;
        packet.field_00 = reader.read_u32();
        packet.field_04 = reader.read_u32();
        packet.field_08 = reader.read_u32();
        packet.field_0c = reader.read_u32();
        packet.field_10 = reader.read_u32();
        packet.field_14 = reader.read_u32();
        packet.field_18 = reader.read_u32();
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
