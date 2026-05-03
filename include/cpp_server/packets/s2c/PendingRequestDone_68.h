#pragma once

#include <span>

#include "cpp_server/packets/shared/EmptyPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct PendingRequestDone_68 : shared::EmptyPacketBase<0x68> {
    static PendingRequestDone_68 deserialize(std::span<const std::uint8_t> payload) {
        shared::ExpectEmptyPayload(payload, "PendingRequestDone_68");
        return {};
    }
};

}  // namespace cpp_server::packets::s2c
