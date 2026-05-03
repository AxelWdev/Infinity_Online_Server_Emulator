#pragma once

#include <algorithm>
#include <array>
#include <span>
#include <stdexcept>
#include <string>

#include "cpp_server/core/Utf.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct LoginChallenge_9F {
    static constexpr std::uint8_t kOpcode = 0x9F;

    std::uint32_t reconnect_login_dword{};
    bool field_04_nonzero{};
    std::array<std::uint8_t, 4> field_07_to_0A_reserved{};
    bool team_mode_enabled{};
    std::string current_nickname{};
    std::string reconnect_login_string{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        const auto encoded_nickname = core::EncodeUtf16Le(current_nickname);
        const auto encoded_reconnect_login = core::EncodeUtf16Le(reconnect_login_string);
        const auto nickname_chars = encoded_nickname.size() / 2;
        const auto reconnect_login_chars = encoded_reconnect_login.size() / 2;
        if (nickname_chars > 0xFF || reconnect_login_chars > 0xFF) {
            throw std::runtime_error("LoginChallenge_9F string field is too long");
        }

        ByteWriter writer;
        writer.write_u32(reconnect_login_dword);
        writer.write_u8(field_04_nonzero ? 1 : 0);
        writer.write_u8(static_cast<std::uint8_t>(reconnect_login_chars));
        writer.write_u8(static_cast<std::uint8_t>(nickname_chars));
        writer.write_bytes(std::span<const std::uint8_t>(field_07_to_0A_reserved));
        writer.write_u8(team_mode_enabled ? 1 : 0);
        writer.write_bytes(encoded_nickname);
        writer.write_bytes(encoded_reconnect_login);
        return writer.take();
    }

    static LoginChallenge_9F deserialize(std::span<const std::uint8_t> payload) {
        ByteReader reader(payload);
        LoginChallenge_9F packet;
        packet.reconnect_login_dword = reader.read_u32();
        packet.field_04_nonzero = reader.read_u8() != 0;
        const auto reconnect_login_chars = reader.read_u8();
        const auto nickname_chars = reader.read_u8();
        const auto reserved = reader.read_bytes(packet.field_07_to_0A_reserved.size());
        std::copy(reserved.begin(), reserved.end(), packet.field_07_to_0A_reserved.begin());
        packet.team_mode_enabled = reader.read_u8() != 0;
        packet.current_nickname =
            core::DecodeUtf16Le(reader.read_bytes(static_cast<std::size_t>(nickname_chars) * 2));
        packet.reconnect_login_string =
            core::DecodeUtf16Le(reader.read_bytes(static_cast<std::size_t>(reconnect_login_chars) * 2));
        return packet;
    }
};

}  // namespace cpp_server::packets::s2c
