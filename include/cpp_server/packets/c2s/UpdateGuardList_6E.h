#pragma once

#include <span>

#include "cpp_server/packets/shared/RawPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct UpdateGuardList_6E : shared::RawPacketBase<0x6E> {
    static UpdateGuardList_6E deserialize(std::span<const std::uint8_t> payload) {
        UpdateGuardList_6E packet;
        packet.payload_bytes = shared::CopyPayload(payload);
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
