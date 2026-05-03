#pragma once

#include <span>

#include "cpp_server/packets/shared/PacketSupport.h"
#include "cpp_server/packets/shared/RawPacketBase.h"

namespace cpp_server::packets::c2s {

struct UpdateLimitedItemInfo_34 : shared::RawPacketBase<0x34> {
    static UpdateLimitedItemInfo_34 deserialize(std::span<const std::uint8_t> payload) {
        UpdateLimitedItemInfo_34 packet;
        packet.payload_bytes = shared::CopyPayload(payload);
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
