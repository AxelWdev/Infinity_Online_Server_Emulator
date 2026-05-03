#pragma once

#include <span>

#include "cpp_server/packets/shared/RawPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct PendingRequest_67 : shared::RawPacketBase<0x67> {
    static PendingRequest_67 deserialize(std::span<const std::uint8_t> payload) {
        PendingRequest_67 packet;
        packet.payload_bytes = shared::CopyPayload(payload);
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
