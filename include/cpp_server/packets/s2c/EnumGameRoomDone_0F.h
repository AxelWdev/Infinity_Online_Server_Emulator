#pragma once

#include <cstdint>
#include <span>

#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct EnumGameRoomDone_0F {
    static constexpr std::uint8_t kOpcode = 0x0F;

    std::uint16_t room_count{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        ByteWriter writer;
        if (room_count <= 0xFF) {
            writer.write_u8(static_cast<std::uint8_t>(room_count));
        } else {
            writer.write_u16(room_count);
        }
        return writer.take();
    }

    static EnumGameRoomDone_0F deserialize(std::span<const std::uint8_t> payload) {
        EnumGameRoomDone_0F packet;
        if (payload.size() == 1) {
            packet.room_count = payload[0];
            return packet;
        }
        if (payload.size() == 2) {
            ByteReader reader(payload);
            packet.room_count = reader.read_u16();
            return packet;
        }
        throw std::runtime_error("EnumGameRoomDone_0F expects a 1-byte or 2-byte payload");
    }
};

}  // namespace cpp_server::packets::s2c
