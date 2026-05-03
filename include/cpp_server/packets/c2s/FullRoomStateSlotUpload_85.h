#pragma once

#include <span>

#include "cpp_server/packets/shared/RawPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct FullRoomStateSlotUpload_85 : shared::RawPacketBase<0x85> {
    static FullRoomStateSlotUpload_85 deserialize(std::span<const std::uint8_t> payload) {
        FullRoomStateSlotUpload_85 packet;
        packet.payload_bytes = shared::CopyPayload(payload);
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
