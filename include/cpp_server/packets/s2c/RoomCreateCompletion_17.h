#pragma once

#include <span>

#include "cpp_server/packets/shared/EmptyPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct RoomCreateCompletion_17 : shared::EmptyPacketBase<0x17> {
    static RoomCreateCompletion_17 deserialize(std::span<const std::uint8_t> payload) {
        shared::ExpectEmptyPayload(payload, "RoomCreateCompletion_17");
        return {};
    }
};

}  // namespace cpp_server::packets::s2c
