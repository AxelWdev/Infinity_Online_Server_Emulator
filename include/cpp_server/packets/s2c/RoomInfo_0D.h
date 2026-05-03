#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "cpp_server/core/Utf.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct RoomInfo_0D {
    static constexpr std::uint8_t kOpcode = 0x0D;

    std::string room_name{};
    std::uint8_t field_01{};
    std::uint8_t room_state_code{};
    std::uint8_t password_required_flag{};
    std::uint16_t room_id{};
    std::uint8_t current_players{};
    std::uint8_t max_players{};
    std::uint16_t rule_or_mission_id{};
    std::uint8_t field_0a_reserved{};
    std::uint8_t mission_icon_count{};
    std::uint8_t flags{};
    std::uint8_t limit_minutes{};
    std::uint8_t limit_kills{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        const auto encoded_room_name = core::EncodeUtf16Le(room_name);
        const auto room_name_chars = encoded_room_name.size() / 2;
        if (room_name_chars > 0xFF) {
            throw std::runtime_error("RoomInfo_0D room_name is too long");
        }

        ByteWriter writer;
        writer.write_u8(static_cast<std::uint8_t>(room_name_chars));
        writer.write_u8(field_01);
        writer.write_u8(room_state_code);
        writer.write_u8(password_required_flag);
        writer.write_u16(room_id);
        writer.write_u8(current_players);
        writer.write_u8(max_players);
        writer.write_u16(rule_or_mission_id);
        writer.write_u8(field_0a_reserved);
        writer.write_u8(mission_icon_count);
        writer.write_u8(flags);
        writer.write_u8(limit_minutes);
        writer.write_u8(limit_kills);
        writer.write_bytes(encoded_room_name);
        return writer.take();
    }

    static RoomInfo_0D deserialize(std::span<const std::uint8_t> payload) {
        ByteReader reader(payload);
        RoomInfo_0D packet;
        const auto room_name_chars = reader.read_u8();
        packet.field_01 = reader.read_u8();
        packet.room_state_code = reader.read_u8();
        packet.password_required_flag = reader.read_u8();
        packet.room_id = reader.read_u16();
        packet.current_players = reader.read_u8();
        packet.max_players = reader.read_u8();
        packet.rule_or_mission_id = reader.read_u16();
        packet.field_0a_reserved = reader.read_u8();
        packet.mission_icon_count = reader.read_u8();
        packet.flags = reader.read_u8();
        packet.limit_minutes = reader.read_u8();
        packet.limit_kills = reader.read_u8();
        packet.room_name = core::DecodeUtf16Le(reader.read_bytes(room_name_chars * 2));
        return packet;
    }
};

}  // namespace cpp_server::packets::s2c
