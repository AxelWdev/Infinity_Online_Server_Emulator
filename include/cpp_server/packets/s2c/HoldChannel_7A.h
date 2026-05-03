#pragma once

#include <span>

#include "cpp_server/packets/shared/EmptyPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct HoldChannel_7A : shared::EmptyPacketBase<0x7A> {
    static HoldChannel_7A deserialize(std::span<const std::uint8_t> payload) {
        shared::ExpectEmptyPayload(payload, "HoldChannel_7A");
        return {};
    }
};

}  // namespace cpp_server::packets::s2c
