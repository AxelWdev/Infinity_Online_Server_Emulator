#pragma once

#include <cstdint>
#include <span>

#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct StateUpdate_9E {
    static constexpr std::uint8_t kOpcode = 0x9E;

    std::uint32_t state_entry_id{};      // +0x00; primary match key in client +0x4594 table.
    std::uint8_t state_kind{};           // +0x04; copied to client record +0x04.
    std::uint32_t state_value{};         // +0x05; copied to record +0x08; mission-clear scripts test it.
    std::uint32_t state_aux_value{};     // +0x09; copied to client record +0x0c.
    std::uint8_t state_flag{};           // +0x0d; copied to client record +0x10.
    std::uint8_t display_lookup_key{};   // +0x0e; lookup key for the display string copied to record +0x12.
    std::uint32_t display_resource_id{}; // +0x0f; copied to record +0x54; used by append-path display lookup.
    std::uint8_t record_flag_58{};       // +0x13; copied to client record +0x58.
    std::uint8_t record_flag_59{};       // +0x14; copied to client record +0x59.

    [[nodiscard]] core::ByteVector serialize_payload() const {
        ByteWriter writer;
        writer.write_u32(state_entry_id);
        writer.write_u8(state_kind);
        writer.write_u32(state_value);
        writer.write_u32(state_aux_value);
        writer.write_u8(state_flag);
        writer.write_u8(display_lookup_key);
        writer.write_u32(display_resource_id);
        writer.write_u8(record_flag_58);
        writer.write_u8(record_flag_59);
        return writer.take();
    }

    static StateUpdate_9E deserialize(std::span<const std::uint8_t> payload) {
        ByteReader reader(payload);
        StateUpdate_9E packet;
        packet.state_entry_id = reader.read_u32();
        packet.state_kind = reader.read_u8();
        packet.state_value = reader.read_u32();
        packet.state_aux_value = reader.read_u32();
        packet.state_flag = reader.read_u8();
        packet.display_lookup_key = reader.read_u8();
        packet.display_resource_id = reader.read_u32();
        packet.record_flag_58 = reader.read_u8();
        packet.record_flag_59 = reader.read_u8();
        return packet;
    }
};

}  // namespace cpp_server::packets::s2c
