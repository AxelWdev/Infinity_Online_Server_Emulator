#pragma once

#include <cstdint>
#include <span>

#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct UpdateCharacterList_6B {
    static constexpr std::uint8_t kOpcode = 0x6B;

    std::uint32_t character_id{};
    std::uint32_t room_config_slot_0{};
    std::uint32_t room_config_slot_1{};
    std::uint32_t room_config_slot_2{};
    std::uint32_t room_config_slot_3{};
    std::uint32_t room_config_slot_4{};
    std::uint32_t room_config_slot_5{};
    std::uint32_t room_config_slot_6{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        ByteWriter writer;
        writer.write_u32(character_id);
        writer.write_u32(room_config_slot_0);
        writer.write_u32(room_config_slot_1);
        writer.write_u32(room_config_slot_2);
        writer.write_u32(room_config_slot_3);
        writer.write_u32(room_config_slot_4);
        writer.write_u32(room_config_slot_5);
        writer.write_u32(room_config_slot_6);
        return writer.take();
    }

    static UpdateCharacterList_6B deserialize(std::span<const std::uint8_t> payload) {
        ByteReader reader(payload);
        UpdateCharacterList_6B packet;
        packet.character_id = reader.read_u32();
        packet.room_config_slot_0 = reader.read_u32();
        packet.room_config_slot_1 = reader.read_u32();
        packet.room_config_slot_2 = reader.read_u32();
        packet.room_config_slot_3 = reader.read_u32();
        packet.room_config_slot_4 = reader.read_u32();
        packet.room_config_slot_5 = reader.read_u32();
        packet.room_config_slot_6 = reader.read_u32();
        return packet;
    }
};

}  // namespace cpp_server::packets::s2c
