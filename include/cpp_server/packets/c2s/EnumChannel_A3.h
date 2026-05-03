#pragma once

#include <span>

#include "cpp_server/packets/shared/RawPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct EnumChannel_A3 : shared::RawPacketBase<0xA3> {
    static EnumChannel_A3 deserialize(std::span<const std::uint8_t> payload) {
        EnumChannel_A3 packet;
        packet.payload_bytes = shared::CopyPayload(payload);
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
