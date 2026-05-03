#pragma once

#include <span>

#include "cpp_server/packets/shared/RawPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct PostConnectLobbyFollowUp_02 : shared::RawPacketBase<0x02> {
    static PostConnectLobbyFollowUp_02 deserialize(std::span<const std::uint8_t> payload) {
        PostConnectLobbyFollowUp_02 packet;
        packet.payload_bytes = shared::CopyPayload(payload);
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
