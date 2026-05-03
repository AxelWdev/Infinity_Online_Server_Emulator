#pragma once

#include <span>

#include "cpp_server/packets/shared/RawPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct EnterChannel_03 : shared::RawPacketBase<0x03> {
    static EnterChannel_03 deserialize(std::span<const std::uint8_t> payload) {
        EnterChannel_03 packet;
        packet.payload_bytes = shared::CopyPayload(payload);
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
