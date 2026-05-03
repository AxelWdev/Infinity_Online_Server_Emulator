#pragma once

#include <span>

#include "cpp_server/packets/shared/EmptyPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct EnumQuickSlotDone_45 : shared::EmptyPacketBase<0x45> {
    static EnumQuickSlotDone_45 deserialize(std::span<const std::uint8_t> payload) {
        shared::ExpectEmptyPayload(payload, "EnumQuickSlotDone_45");
        return {};
    }
};

}  // namespace cpp_server::packets::s2c
