#pragma once

#include <span>

#include "cpp_server/packets/shared/RawPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct EnumItem_3F : shared::RawPacketBase<0x3F> {
    static EnumItem_3F deserialize(std::span<const std::uint8_t> payload) {
        EnumItem_3F packet;
        packet.payload_bytes = shared::CopyPayload(payload);
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
