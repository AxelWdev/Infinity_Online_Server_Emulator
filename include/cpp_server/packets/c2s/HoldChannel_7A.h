#pragma once

#include <span>

#include "cpp_server/packets/shared/RawPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct HoldChannel_7A : shared::RawPacketBase<0x7A> {
    static HoldChannel_7A deserialize(std::span<const std::uint8_t> payload) {
        HoldChannel_7A packet;
        packet.payload_bytes = shared::CopyPayload(payload);
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
