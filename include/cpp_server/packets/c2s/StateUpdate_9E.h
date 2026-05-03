#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "cpp_server/core/Utf.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct StateUpdate_9E {
    static constexpr std::uint8_t kOpcode = 0x9E;

    std::uint8_t name_char_count{};
    std::uint16_t state_entry_id{};
    std::string player_name{};
    core::ByteVector trailing_bytes{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        ByteWriter writer;
        writer.write_u8(name_char_count);
        writer.write_u16(state_entry_id);
        writer.write_bytes(core::EncodeUtf16Le(player_name));
        writer.write_bytes(trailing_bytes);
        return writer.take();
    }

    static StateUpdate_9E deserialize(std::span<const std::uint8_t> payload) {
        ByteReader reader(payload);
        StateUpdate_9E packet;
        packet.name_char_count = reader.read_u8();
        packet.state_entry_id = reader.read_u16();
        packet.player_name = core::DecodeUtf16Le(reader.read_bytes(packet.name_char_count * 2));
        packet.trailing_bytes = reader.read_remaining();
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
