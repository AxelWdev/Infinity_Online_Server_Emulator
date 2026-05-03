#pragma once

#include <span>

#include "cpp_server/packets/shared/RawPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct StreamedTextListRequest_52 : shared::RawPacketBase<0x52> {
    static StreamedTextListRequest_52 deserialize(std::span<const std::uint8_t> payload) {
        StreamedTextListRequest_52 packet;
        packet.payload_bytes = shared::CopyPayload(payload);
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
