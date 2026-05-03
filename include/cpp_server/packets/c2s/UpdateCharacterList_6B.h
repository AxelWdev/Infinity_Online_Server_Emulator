#pragma once

#include <span>

#include "cpp_server/packets/shared/RawPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct UpdateCharacterList_6B : shared::RawPacketBase<0x6B> {
    static UpdateCharacterList_6B deserialize(std::span<const std::uint8_t> payload) {
        UpdateCharacterList_6B packet;
        packet.payload_bytes = shared::CopyPayload(payload);
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
