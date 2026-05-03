#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "cpp_server/core/Utf.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct FullRoomStateUploadTail_84 {
    static constexpr std::uint8_t kOpcode = 0x84;

    std::uint8_t room_name_char_count{};
    std::string room_name{};
    core::ByteVector trailing_bytes{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        ByteWriter writer;
        writer.write_u8(room_name_char_count);
        writer.write_bytes(core::EncodeUtf16Le(room_name));
        writer.write_bytes(trailing_bytes);
        return writer.take();
    }

    static FullRoomStateUploadTail_84 deserialize(std::span<const std::uint8_t> payload) {
        ByteReader reader(payload);
        FullRoomStateUploadTail_84 packet;
        packet.room_name_char_count = reader.read_u8();
        packet.room_name = core::DecodeUtf16Le(reader.read_bytes(packet.room_name_char_count * 2));
        packet.trailing_bytes = reader.read_remaining();
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
