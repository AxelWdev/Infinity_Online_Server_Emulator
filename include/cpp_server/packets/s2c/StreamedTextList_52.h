#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "cpp_server/core/Utf.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct StreamedTextList_52 {
    static constexpr std::uint8_t kOpcode = 0x52;

    std::uint8_t field_00{};
    std::string text{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        ByteWriter writer;
        writer.write_u8(field_00);
        writer.write_bytes(core::EncodeUtf16Le(text, true));
        return writer.take();
    }

    static StreamedTextList_52 deserialize(std::span<const std::uint8_t> payload) {
        ByteReader reader(payload);
        StreamedTextList_52 packet;
        packet.field_00 = reader.read_u8();
        auto text_bytes = reader.read_remaining();
        if (text_bytes.size() >= 2 && text_bytes[text_bytes.size() - 1] == 0x00 &&
            text_bytes[text_bytes.size() - 2] == 0x00) {
            text_bytes.resize(text_bytes.size() - 2);
        }
        packet.text = core::DecodeUtf16Le(text_bytes);
        return packet;
    }
};

}  // namespace cpp_server::packets::s2c
