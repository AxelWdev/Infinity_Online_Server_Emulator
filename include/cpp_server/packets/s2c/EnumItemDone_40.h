#pragma once

#include <span>

#include "cpp_server/packets/shared/EmptyPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct EnumItemDone_40 : shared::EmptyPacketBase<0x40> {
    static EnumItemDone_40 deserialize(std::span<const std::uint8_t> payload) {
        shared::ExpectEmptyPayload(payload, "EnumItemDone_40");
        return {};
    }
};

}  // namespace cpp_server::packets::s2c
