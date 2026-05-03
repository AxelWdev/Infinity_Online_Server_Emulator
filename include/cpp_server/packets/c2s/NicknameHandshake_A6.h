#pragma once

#include <span>
#include <stdexcept>
#include <string>

#include "cpp_server/core/Utf.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct NicknameHandshake_A6 {
    static constexpr std::uint8_t kOpcode = 0xA6;

    std::string character_name{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        const auto encoded_name = core::EncodeUtf16Le(character_name);
        const auto name_chars = encoded_name.size() / 2;
        if (name_chars > 0xFF) {
            throw std::runtime_error("NicknameHandshake_A6 character_name is too long");
        }

        ByteWriter writer;
        writer.write_u8(static_cast<std::uint8_t>(name_chars));
        writer.write_bytes(encoded_name);
        return writer.take();
    }

    static NicknameHandshake_A6 deserialize(std::span<const std::uint8_t> payload) {
        ByteReader reader(payload);
        NicknameHandshake_A6 packet;
        const auto name_chars = reader.read_u8();
        packet.character_name = core::DecodeUtf16Le(reader.read_bytes(static_cast<std::size_t>(name_chars) * 2));
        if (reader.remaining() != 0) {
            throw std::runtime_error("NicknameHandshake_A6 contains unexpected trailing bytes");
        }
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
