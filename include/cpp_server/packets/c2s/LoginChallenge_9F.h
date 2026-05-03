#pragma once

#include <span>
#include <stdexcept>
#include <string>

#include "cpp_server/core/Utf.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct LoginChallenge_9F {
    static constexpr std::uint8_t kOpcode = 0x9F;

    std::string login_id{};
    std::string password_value{};
    std::uint16_t field_02_fixed_012c{0x012C};
    bool force_disconnect_flag{};
    core::ByteVector trailing_bytes{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        const auto encoded_login = core::EncodeUtf16Le(login_id);
        const auto encoded_password = core::EncodeUtf16Le(password_value);
        const auto login_chars = encoded_login.size() / 2;
        const auto password_chars = encoded_password.size() / 2;
        if (login_chars > 0xFF || password_chars > 0xFF) {
            throw std::runtime_error("LoginChallenge_9F string field is too long");
        }

        ByteWriter writer;
        writer.write_u8(static_cast<std::uint8_t>(login_chars));
        writer.write_u8(static_cast<std::uint8_t>(password_chars));
        writer.write_u16(field_02_fixed_012c);
        writer.write_u8(force_disconnect_flag ? 1 : 0);
        writer.write_bytes(encoded_login);
        writer.write_bytes(encoded_password);
        writer.write_bytes(trailing_bytes);
        return writer.take();
    }

    static LoginChallenge_9F deserialize(std::span<const std::uint8_t> payload) {
        ByteReader reader(payload);
        LoginChallenge_9F packet;
        const auto login_chars = reader.read_u8();
        const auto password_chars = reader.read_u8();
        packet.field_02_fixed_012c = reader.read_u16();
        packet.force_disconnect_flag = reader.read_u8() != 0;
        packet.login_id = core::DecodeUtf16Le(reader.read_bytes(static_cast<std::size_t>(login_chars) * 2));
        packet.password_value = core::DecodeUtf16Le(reader.read_bytes(static_cast<std::size_t>(password_chars) * 2));
        packet.trailing_bytes = reader.read_remaining();
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
