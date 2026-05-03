#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>

#include "cpp_server/core/Utf.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct FullRoomStateReply_84 {
    static constexpr std::uint8_t kOpcode = 0x84;

    std::string string0{};
    std::string string1{};
    std::uint32_t field_02{};
    std::uint32_t field_06{};
    std::uint32_t field_0a{};
    std::uint32_t unused_0e_11_u32{};
    std::uint32_t field_12{};
    std::uint32_t field_16{};
    std::uint8_t field_1a{};
    std::array<std::uint32_t, 8> field_1b{};
    std::uint32_t field_3b{};
    std::uint32_t field_3f{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        const auto encoded_string0 = core::EncodeUtf16Le(string0);
        const auto encoded_string1 = core::EncodeUtf16Le(string1);
        const auto string0_chars = encoded_string0.size() / 2;
        const auto string1_chars = encoded_string1.size() / 2;
        if (string0_chars > 0xFF || string1_chars > 0xFF) {
            throw std::runtime_error("FullRoomStateReply_84 string field is too long");
        }

        ByteWriter writer;
        writer.write_u8(static_cast<std::uint8_t>(string0_chars));
        writer.write_u8(static_cast<std::uint8_t>(string1_chars));
        writer.write_u32(field_02);
        writer.write_u32(field_06);
        writer.write_u32(field_0a);
        writer.write_u32(unused_0e_11_u32);
        writer.write_u32(field_12);
        writer.write_u32(field_16);
        writer.write_u8(field_1a);
        for (const auto value : field_1b) {
            writer.write_u32(value);
        }
        writer.write_u32(field_3b);
        writer.write_u32(field_3f);
        writer.write_bytes(encoded_string0);
        writer.write_bytes(encoded_string1);
        return writer.take();
    }

    static FullRoomStateReply_84 deserialize(std::span<const std::uint8_t> payload) {
        ByteReader reader(payload);
        FullRoomStateReply_84 packet;
        const auto string0_chars = reader.read_u8();
        const auto string1_chars = reader.read_u8();
        packet.field_02 = reader.read_u32();
        packet.field_06 = reader.read_u32();
        packet.field_0a = reader.read_u32();
        packet.unused_0e_11_u32 = reader.read_u32();
        packet.field_12 = reader.read_u32();
        packet.field_16 = reader.read_u32();
        packet.field_1a = reader.read_u8();
        for (auto& value : packet.field_1b) {
            value = reader.read_u32();
        }
        packet.field_3b = reader.read_u32();
        packet.field_3f = reader.read_u32();
        packet.string0 = core::DecodeUtf16Le(reader.read_bytes(string0_chars * 2));
        packet.string1 = core::DecodeUtf16Le(reader.read_bytes(string1_chars * 2));
        return packet;
    }
};

}  // namespace cpp_server::packets::s2c
