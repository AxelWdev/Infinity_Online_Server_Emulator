#pragma once

#include <span>

#include "cpp_server/packets/shared/RawPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct RoomEnterToken_0D : shared::RawPacketBase<0x0D> {
    static RoomEnterToken_0D deserialize(std::span<const std::uint8_t> payload) {
        RoomEnterToken_0D packet;
        packet.payload_bytes = shared::CopyPayload(payload);
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
