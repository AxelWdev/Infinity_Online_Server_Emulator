#pragma once

#include <span>

#include "cpp_server/packets/shared/RawPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct PostRoomCreateSync_79 : shared::RawPacketBase<0x79> {
    static PostRoomCreateSync_79 deserialize(std::span<const std::uint8_t> payload) {
        PostRoomCreateSync_79 packet;
        packet.payload_bytes = shared::CopyPayload(payload);
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
