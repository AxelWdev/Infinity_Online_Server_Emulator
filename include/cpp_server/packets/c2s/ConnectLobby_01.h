#pragma once

#include <span>
#include <string>

#include "cpp_server/core/Utf.h"
#include "cpp_server/packets/shared/RawPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct ConnectLobby_01 {
    static constexpr std::uint8_t kOpcode = 0x01;

    std::uint8_t login_char_count{};
    std::uint32_t login_dword{};
    std::string login_id{};
    core::ByteVector trailing_bytes{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        const auto encoded_login = core::EncodeUtf16Le(login_id);
        const auto login_chars = encoded_login.size() / 2;
        if (login_chars > 0xFF) {
            throw std::runtime_error("ConnectLobby_01 login field is too long");
        }

        ByteWriter writer;
        writer.write_u8(static_cast<std::uint8_t>(login_chars));
        writer.write_u32(login_dword);
        writer.write_bytes(encoded_login);
        writer.write_bytes(trailing_bytes);
        return writer.take();
    }

    static ConnectLobby_01 deserialize(std::span<const std::uint8_t> payload) {
        ByteReader reader(payload);
        ConnectLobby_01 packet;
        packet.login_char_count = reader.read_u8();
        packet.login_dword = reader.read_u32();
        packet.login_id = core::DecodeUtf16Le(reader.read_bytes(static_cast<std::size_t>(packet.login_char_count) * 2));
        packet.trailing_bytes = reader.read_remaining();
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
