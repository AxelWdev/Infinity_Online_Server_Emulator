#pragma once

#include <span>

#include "cpp_server/packets/shared/EmptyPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct ConnectLobby_01 : shared::EmptyPacketBase<0x01> {
    static ConnectLobby_01 deserialize(std::span<const std::uint8_t> payload) {
        shared::ExpectEmptyPayload(payload, "ConnectLobby_01");
        return {};
    }
};

}  // namespace cpp_server::packets::s2c
