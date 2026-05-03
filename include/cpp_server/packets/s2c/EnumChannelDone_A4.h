#pragma once

#include <span>

#include "cpp_server/packets/shared/EmptyPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct EnumChannelDone_A4 : shared::EmptyPacketBase<0xA4> {
    static EnumChannelDone_A4 deserialize(std::span<const std::uint8_t> payload) {
        shared::ExpectEmptyPayload(payload, "EnumChannelDone_A4");
        return {};
    }
};

}  // namespace cpp_server::packets::s2c
