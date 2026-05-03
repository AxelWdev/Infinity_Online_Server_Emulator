#pragma once

#include <span>

#include "cpp_server/packets/shared/RawPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct EnumGameRoom_0E : shared::RawPacketBase<0x0E> {
    static EnumGameRoom_0E deserialize(std::span<const std::uint8_t> payload) {
        EnumGameRoom_0E packet;
        packet.payload_bytes = shared::CopyPayload(payload);
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
