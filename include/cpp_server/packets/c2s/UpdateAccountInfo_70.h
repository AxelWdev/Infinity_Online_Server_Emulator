#pragma once

#include <span>

#include "cpp_server/packets/shared/RawPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct UpdateAccountInfo_70 : shared::RawPacketBase<0x70> {
    static UpdateAccountInfo_70 deserialize(std::span<const std::uint8_t> payload) {
        UpdateAccountInfo_70 packet;
        packet.payload_bytes = shared::CopyPayload(payload);
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
