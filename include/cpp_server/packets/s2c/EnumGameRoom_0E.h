#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "cpp_server/core/Utf.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct EnumGameRoom_0E {
    static constexpr std::uint8_t kOpcode = 0x0E;

    std::string primary_name{};
    std::string secondary_name{};
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
        const auto encoded_primary = core::EncodeUtf16Le(primary_name);
        const auto encoded_secondary = core::EncodeUtf16Le(secondary_name);
        const auto primary_chars = encoded_primary.size() / 2;
        const auto secondary_chars = encoded_secondary.size() / 2;
        if (primary_chars > 0xFF || secondary_chars > 0xFF) {
            throw std::runtime_error("EnumGameRoom_0E name field is too long");
        }

        ByteWriter writer;
        writer.write_u8(static_cast<std::uint8_t>(primary_chars));
        writer.write_u8(static_cast<std::uint8_t>(secondary_chars));
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
        writer.write_bytes(encoded_primary);
        writer.write_bytes(encoded_secondary);
        return writer.take();
    }

    static EnumGameRoom_0E deserialize(std::span<const std::uint8_t> payload) {
        ByteReader reader(payload);
        EnumGameRoom_0E packet;
        const auto primary_chars = reader.read_u8();
        const auto secondary_chars = reader.read_u8();
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
        packet.primary_name = core::DecodeUtf16Le(reader.read_bytes(primary_chars * 2));
        packet.secondary_name = core::DecodeUtf16Le(reader.read_bytes(secondary_chars * 2));
        return packet;
    }
};

}  // namespace cpp_server::packets::s2c
