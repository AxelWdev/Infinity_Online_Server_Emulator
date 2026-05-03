#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "cpp_server/core/Utf.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct MissionRoomMetadataUpload_0C {
    static constexpr std::uint8_t kOpcode = 0x0C;

    std::uint8_t mission_title_char_count{};
    std::uint8_t field_01_unknown{};
    std::uint8_t max_players{};
    std::uint16_t mission_rule_id{};
    std::uint32_t field_05_unknown{};
    std::string mission_title{};
    core::ByteVector trailing_bytes{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        ByteWriter writer;
        writer.write_u8(mission_title_char_count);
        writer.write_u8(field_01_unknown);
        writer.write_u8(max_players);
        writer.write_u16(mission_rule_id);
        writer.write_u32(field_05_unknown);
        writer.write_bytes(core::EncodeUtf16Le(mission_title));
        writer.write_bytes(trailing_bytes);
        return writer.take();
    }

    static MissionRoomMetadataUpload_0C deserialize(std::span<const std::uint8_t> payload) {
        ByteReader reader(payload);
        MissionRoomMetadataUpload_0C packet;
        packet.mission_title_char_count = reader.read_u8();
        packet.field_01_unknown = reader.read_u8();
        packet.max_players = reader.read_u8();
        packet.mission_rule_id = reader.read_u16();
        packet.field_05_unknown = reader.read_u32();
        packet.mission_title = core::DecodeUtf16Le(reader.read_bytes(packet.mission_title_char_count * 2));
        packet.trailing_bytes = reader.read_remaining();
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
