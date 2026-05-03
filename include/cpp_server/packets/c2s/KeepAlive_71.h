#pragma once

#include <span>

#include "cpp_server/packets/shared/RawPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct KeepAlive_71 : shared::RawPacketBase<0x71> {
    static KeepAlive_71 deserialize(std::span<const std::uint8_t> payload) {
        KeepAlive_71 packet;
        packet.payload_bytes = shared::CopyPayload(payload);
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
