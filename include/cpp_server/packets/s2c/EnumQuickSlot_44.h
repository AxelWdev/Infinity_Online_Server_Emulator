#pragma once

#include <cstdint>
#include <span>

#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct EnumQuickSlot_44 {
    static constexpr std::uint8_t kOpcode = 0x44;

    std::uint32_t entry_key{};
    std::uint8_t slot_index{};
    std::uint32_t quickslot_entry_id{};
    std::uint8_t slot_state{};
    std::uint16_t display_lookup_key{};
    std::uint32_t duration_minutes{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        ByteWriter writer;
        writer.write_u32(entry_key);
        writer.write_u8(slot_index);
        writer.write_u32(quickslot_entry_id);
        writer.write_u8(slot_state);
        writer.write_u16(display_lookup_key);
        writer.write_u32(duration_minutes);
        return writer.take();
    }

    static EnumQuickSlot_44 deserialize(std::span<const std::uint8_t> payload) {
        ByteReader reader(payload);
        EnumQuickSlot_44 packet;
        packet.entry_key = reader.read_u32();
        packet.slot_index = reader.read_u8();
        packet.quickslot_entry_id = reader.read_u32();
        packet.slot_state = reader.read_u8();
        packet.display_lookup_key = reader.read_u16();
        packet.duration_minutes = reader.read_u32();
        return packet;
    }
};

}  // namespace cpp_server::packets::s2c
