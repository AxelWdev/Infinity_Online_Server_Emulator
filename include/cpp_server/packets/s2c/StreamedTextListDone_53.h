#pragma once

#include <span>

#include "cpp_server/packets/shared/EmptyPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct StreamedTextListDone_53 : shared::EmptyPacketBase<0x53> {
    static StreamedTextListDone_53 deserialize(std::span<const std::uint8_t> payload) {
        shared::ExpectEmptyPayload(payload, "StreamedTextListDone_53");
        return {};
    }
};

}  // namespace cpp_server::packets::s2c
