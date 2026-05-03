#pragma once

#include <span>

#include "cpp_server/packets/shared/EmptyPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct EnterChannel_03 : shared::EmptyPacketBase<0x03> {
    static EnterChannel_03 deserialize(std::span<const std::uint8_t> payload) {
        shared::ExpectEmptyPayload(payload, "EnterChannel_03");
        return {};
    }
};

}  // namespace cpp_server::packets::s2c
