#pragma once

#include <span>

#include "cpp_server/packets/shared/RawPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct EnumQuickSlot_44 : shared::RawPacketBase<0x44> {
    static EnumQuickSlot_44 deserialize(std::span<const std::uint8_t> payload) {
        EnumQuickSlot_44 packet;
        packet.payload_bytes = shared::CopyPayload(payload);
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
