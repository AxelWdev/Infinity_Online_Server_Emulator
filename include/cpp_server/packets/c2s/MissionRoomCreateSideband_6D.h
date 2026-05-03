#pragma once

#include <span>

#include "cpp_server/packets/shared/RawPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct MissionRoomCreateSideband_6D : shared::RawPacketBase<0x6D> {
    static MissionRoomCreateSideband_6D deserialize(std::span<const std::uint8_t> payload) {
        MissionRoomCreateSideband_6D packet;
        packet.payload_bytes = shared::CopyPayload(payload);
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
