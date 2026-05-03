#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>

#include "cpp_server/core/Utf.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct RoomPlayerIdTable_1D {
    static constexpr std::uint8_t kOpcode = 0x1D;

    std::string room_context{};
    std::string local_player_name{};
    std::uint8_t player_count{};
    std::uint8_t room_state_code{};
    std::uint16_t rule_or_mission_id{};
    std::uint8_t mission_icon_count{};
    std::uint8_t limit_minutes{};
    std::uint8_t limit_kills{};
    std::array<std::uint32_t, 16> player_ids{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        const auto encoded_room_context = core::EncodeUtf16Le(room_context);
        const auto encoded_player_name = core::EncodeUtf16Le(local_player_name);
        const auto room_context_chars = encoded_room_context.size() / 2;
        const auto player_name_chars = encoded_player_name.size() / 2;
        if (room_context_chars > 0xFF || player_name_chars > 0xFF) {
            throw std::runtime_error("RoomPlayerIdTable_1D string is too long");
        }

        const auto payload_len = 0x63U + encoded_room_context.size() + encoded_player_name.size();
        if (payload_len > 0xFF) {
            throw std::runtime_error("RoomPlayerIdTable_1D payload is too long");
        }

        core::ByteVector payload(payload_len, 0);
        payload[0x04] = static_cast<std::uint8_t>(room_context_chars);
        payload[0x05] = static_cast<std::uint8_t>(player_name_chars);
        payload[0x06] = player_count;
        payload[0x07] = room_state_code;
        write_u16_at(payload, 0x08, rule_or_mission_id);
        payload[0x0c] = mission_icon_count;
        payload[0x0d] = limit_minutes;
        payload[0x0e] = limit_kills;
        for (std::size_t index = 0; index < player_ids.size(); ++index) {
            write_u32_at(payload, 0x0f + index * 4, player_ids[index]);
        }

        auto string_offset = payload.begin() + 0x63;
        string_offset = std::copy(encoded_room_context.begin(), encoded_room_context.end(), string_offset);
        std::copy(encoded_player_name.begin(), encoded_player_name.end(), string_offset);
        return payload;
    }

    static RoomPlayerIdTable_1D deserialize(std::span<const std::uint8_t> payload) {
        if (payload.size() < 0x63) {
            throw std::runtime_error("RoomPlayerIdTable_1D payload is too short");
        }

        RoomPlayerIdTable_1D packet;
        const auto room_context_chars = payload[0x04];
        const auto player_name_chars = payload[0x05];
        packet.player_count = payload[0x06];
        packet.room_state_code = payload[0x07];
        packet.rule_or_mission_id = read_u16_at(payload, 0x08);
        packet.mission_icon_count = payload[0x0c];
        packet.limit_minutes = payload[0x0d];
        packet.limit_kills = payload[0x0e];
        for (std::size_t index = 0; index < packet.player_ids.size(); ++index) {
            packet.player_ids[index] = read_u32_at(payload, 0x0f + index * 4);
        }

        const auto room_context_bytes = static_cast<std::size_t>(room_context_chars) * 2;
        const auto player_name_bytes = static_cast<std::size_t>(player_name_chars) * 2;
        if (payload.size() < 0x63 + room_context_bytes + player_name_bytes) {
            throw std::runtime_error("RoomPlayerIdTable_1D strings are truncated");
        }
        packet.room_context = core::DecodeUtf16Le(payload.subspan(0x63, room_context_bytes));
        packet.local_player_name =
            core::DecodeUtf16Le(payload.subspan(0x63 + room_context_bytes, player_name_bytes));
        return packet;
    }

private:
    static void write_u16_at(core::ByteVector& payload, std::size_t offset, std::uint16_t value) {
        if (payload.size() < offset + 2) {
            throw std::runtime_error("RoomPlayerIdTable_1D write offset outside payload");
        }
        payload[offset] = static_cast<std::uint8_t>(value & 0xFF);
        payload[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    }

    static void write_u32_at(core::ByteVector& payload, std::size_t offset, std::uint32_t value) {
        if (payload.size() < offset + 4) {
            throw std::runtime_error("RoomPlayerIdTable_1D write offset outside payload");
        }
        payload[offset] = static_cast<std::uint8_t>(value & 0xFF);
        payload[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
        payload[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
        payload[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
    }

    static std::uint16_t read_u16_at(std::span<const std::uint8_t> payload, std::size_t offset) {
        if (payload.size() < offset + 2) {
            throw std::runtime_error("RoomPlayerIdTable_1D read offset outside payload");
        }
        return static_cast<std::uint16_t>(payload[offset]) |
               static_cast<std::uint16_t>(payload[offset + 1] << 8);
    }

    static std::uint32_t read_u32_at(std::span<const std::uint8_t> payload, std::size_t offset) {
        if (payload.size() < offset + 4) {
            throw std::runtime_error("RoomPlayerIdTable_1D read offset outside payload");
        }
        return static_cast<std::uint32_t>(payload[offset]) |
               (static_cast<std::uint32_t>(payload[offset + 1]) << 8) |
               (static_cast<std::uint32_t>(payload[offset + 2]) << 16) |
               (static_cast<std::uint32_t>(payload[offset + 3]) << 24);
    }
};

}  // namespace cpp_server::packets::s2c
