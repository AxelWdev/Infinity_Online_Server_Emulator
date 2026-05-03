#pragma once

#include <span>

#include "cpp_server/packets/shared/RawPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct RoomEnterTrigger_8D : shared::RawPacketBase<0x8D> {
    static RoomEnterTrigger_8D deserialize(std::span<const std::uint8_t> payload) {
        RoomEnterTrigger_8D packet;
        packet.payload_bytes = shared::CopyPayload(payload);
        return packet;
    }
};

}  // namespace cpp_server::packets::s2c
