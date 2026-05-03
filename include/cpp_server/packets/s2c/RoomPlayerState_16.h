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

struct RoomPlayerState_16 {
    static constexpr std::uint8_t kOpcode = 0x16;

    std::uint8_t player_name_chars{};
    std::uint8_t secondary_name_chars{};
    std::uint8_t field_06{};
    std::uint8_t field_07{};
    std::uint8_t field_08{};
    std::uint32_t field_0d{};
    std::array<std::uint32_t, 7> equipment_fields{};
    std::uint32_t player_id{};
    std::uint32_t field_31{};
    std::uint8_t flags{};
    std::string secondary_name{};
    std::string player_name{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        const auto encoded_player_name = core::EncodeUtf16Le(player_name);
        const auto encoded_secondary_name = core::EncodeUtf16Le(secondary_name);
        const auto computed_player_chars = encoded_player_name.size() / 2;
        const auto computed_secondary_chars = encoded_secondary_name.size() / 2;
        if (computed_player_chars > 0x3E || computed_secondary_chars > 0x20) {
            throw std::runtime_error("RoomPlayerState_16 string is too long");
        }

        const auto name_chars = player_name_chars != 0
                                    ? player_name_chars
                                    : static_cast<std::uint8_t>(computed_player_chars);
        const auto second_chars = secondary_name_chars != 0
                                      ? secondary_name_chars
                                      : static_cast<std::uint8_t>(computed_secondary_chars);
        const auto payload_len = 0x83U + encoded_player_name.size();
        if (payload_len > 0xFF) {
            throw std::runtime_error("RoomPlayerState_16 payload is too long");
        }

        core::ByteVector payload(payload_len, 0);
        payload[0x04] = name_chars;
        payload[0x05] = second_chars;
        payload[0x06] = field_06;
        payload[0x07] = field_07;
        payload[0x08] = field_08;
        write_u32_at(payload, 0x0d, field_0d);
        for (std::size_t index = 0; index < equipment_fields.size(); ++index) {
            write_u32_at(payload, 0x11 + index * 4, equipment_fields[index]);
        }
        write_u32_at(payload, 0x2d, player_id);
        write_u32_at(payload, 0x31, field_31);
        payload[0x35] = flags;

        if (!encoded_secondary_name.empty()) {
            std::copy(encoded_secondary_name.begin(), encoded_secondary_name.end(), payload.begin() + 0x37);
        }
        if (!encoded_player_name.empty()) {
            std::copy(encoded_player_name.begin(), encoded_player_name.end(), payload.begin() + 0x77);
        }
        return payload;
    }

    static RoomPlayerState_16 deserialize(std::span<const std::uint8_t> payload) {
        if (payload.size() < 0x77) {
            throw std::runtime_error("RoomPlayerState_16 payload is too short");
        }

        RoomPlayerState_16 packet;
        packet.player_name_chars = payload[0x04];
        packet.secondary_name_chars = payload[0x05];
        packet.field_06 = payload[0x06];
        packet.field_07 = payload[0x07];
        packet.field_08 = payload[0x08];
        packet.field_0d = read_u32_at(payload, 0x0d);
        for (std::size_t index = 0; index < packet.equipment_fields.size(); ++index) {
            packet.equipment_fields[index] = read_u32_at(payload, 0x11 + index * 4);
        }
        packet.player_id = read_u32_at(payload, 0x2d);
        packet.field_31 = read_u32_at(payload, 0x31);
        packet.flags = payload[0x35];

        const auto secondary_bytes = static_cast<std::size_t>(packet.secondary_name_chars) * 2;
        if (payload.size() < 0x37 + secondary_bytes) {
            throw std::runtime_error("RoomPlayerState_16 secondary name is truncated");
        }
        packet.secondary_name = core::DecodeUtf16Le(payload.subspan(0x37, secondary_bytes));

        const auto player_bytes = static_cast<std::size_t>(packet.player_name_chars) * 2;
        if (payload.size() < 0x77 + player_bytes) {
            throw std::runtime_error("RoomPlayerState_16 player name is truncated");
        }
        packet.player_name = core::DecodeUtf16Le(payload.subspan(0x77, player_bytes));
        return packet;
    }

private:
    static void write_u32_at(core::ByteVector& payload, std::size_t offset, std::uint32_t value) {
        if (payload.size() < offset + 4) {
            throw std::runtime_error("RoomPlayerState_16 write offset outside payload");
        }
        payload[offset] = static_cast<std::uint8_t>(value & 0xFF);
        payload[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
        payload[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
        payload[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
    }

    static std::uint32_t read_u32_at(std::span<const std::uint8_t> payload, std::size_t offset) {
        if (payload.size() < offset + 4) {
            throw std::runtime_error("RoomPlayerState_16 read offset outside payload");
        }
        return static_cast<std::uint32_t>(payload[offset]) |
               (static_cast<std::uint32_t>(payload[offset + 1]) << 8) |
               (static_cast<std::uint32_t>(payload[offset + 2]) << 16) |
               (static_cast<std::uint32_t>(payload[offset + 3]) << 24);
    }
};

}  // namespace cpp_server::packets::s2c
