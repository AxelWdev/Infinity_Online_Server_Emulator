#pragma once

#include <span>
#include <stdexcept>

#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct AssignQuickSlot_47 {
    static constexpr std::uint8_t kOpcode = 0x47;

    std::uint8_t slot_index{};
    std::uint32_t current_entry_key{};
    std::uint8_t quickslot_item_kind{};
    std::uint16_t item_or_skill_id{};
    std::uint16_t field_08{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        ByteWriter writer;
        writer.write_u8(slot_index);
        writer.write_u32(current_entry_key);
        writer.write_u8(quickslot_item_kind);
        writer.write_u16(item_or_skill_id);
        writer.write_u16(field_08);
        return writer.take();
    }

    static AssignQuickSlot_47 deserialize(std::span<const std::uint8_t> payload) {
        if (payload.size() != 10) {
            throw std::runtime_error("AssignQuickSlot_47 expects a 10-byte payload");
        }

        ByteReader reader(payload);
        AssignQuickSlot_47 packet;
        packet.slot_index = reader.read_u8();
        packet.current_entry_key = reader.read_u32();
        packet.quickslot_item_kind = reader.read_u8();
        packet.item_or_skill_id = reader.read_u16();
        packet.field_08 = reader.read_u16();
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
