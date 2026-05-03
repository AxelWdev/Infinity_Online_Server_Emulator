#pragma once

#include <cstdint>
#include <span>

#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct UpdateSkillList_73 {
    static constexpr std::uint8_t kOpcode = 0x73;

    std::uint32_t field_00{};
    std::uint32_t field_04{};
    std::uint32_t skill_item_id{};
    std::uint32_t duration_minutes{};
    std::uint8_t update_existing_flag{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        ByteWriter writer;
        writer.write_u32(field_00);
        writer.write_u32(field_04);
        writer.write_u32(skill_item_id);
        writer.write_u32(duration_minutes);
        writer.write_u8(update_existing_flag);
        return writer.take();
    }

    static UpdateSkillList_73 deserialize(std::span<const std::uint8_t> payload) {
        ByteReader reader(payload);
        UpdateSkillList_73 packet;
        packet.field_00 = reader.read_u32();
        packet.field_04 = reader.read_u32();
        packet.skill_item_id = reader.read_u32();
        packet.duration_minutes = reader.read_u32();
        packet.update_existing_flag = reader.read_u8();
        return packet;
    }
};

}  // namespace cpp_server::packets::s2c
