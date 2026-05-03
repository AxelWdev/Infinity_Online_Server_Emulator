#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "cpp_server/core/Utf.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct UpdateGuardList_6E {
    static constexpr std::uint8_t kOpcode = 0x6E;

    std::uint32_t guard_instance_id{};
    std::string guard_nickname{};
    std::uint32_t guard_kind_id{};
    std::uint32_t selectable_flag{};
    std::uint32_t equipped_item_slot_0_id{};
    std::uint32_t equipped_item_slot_1_id{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        const auto encoded_name = core::EncodeUtf16Le(guard_nickname);
        const auto name_chars = encoded_name.size() / 2;
        if (name_chars > 0xFF) {
            throw std::runtime_error("UpdateGuardList_6E guard_nickname is too long");
        }

        ByteWriter writer;
        writer.write_u32(guard_instance_id);
        writer.write_u8(static_cast<std::uint8_t>(name_chars));
        writer.write_u32(guard_kind_id);
        writer.write_u32(selectable_flag);
        writer.write_u32(equipped_item_slot_0_id);
        writer.write_u32(equipped_item_slot_1_id);
        writer.write_bytes(encoded_name);
        return writer.take();
    }

    static UpdateGuardList_6E deserialize(std::span<const std::uint8_t> payload) {
        ByteReader reader(payload);
        UpdateGuardList_6E packet;
        packet.guard_instance_id = reader.read_u32();
        const auto name_chars = reader.read_u8();
        packet.guard_kind_id = reader.read_u32();
        packet.selectable_flag = reader.read_u32();
        packet.equipped_item_slot_0_id = reader.read_u32();
        packet.equipped_item_slot_1_id = reader.read_u32();
        packet.guard_nickname = core::DecodeUtf16Le(reader.read_bytes(name_chars * 2));
        return packet;
    }
};

}  // namespace cpp_server::packets::s2c
