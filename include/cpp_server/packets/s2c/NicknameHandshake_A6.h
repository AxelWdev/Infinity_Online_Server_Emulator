#pragma once

#include <span>

#include "cpp_server/packets/shared/EmptyPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct NicknameHandshake_A6 : shared::EmptyPacketBase<0xA6> {
    static NicknameHandshake_A6 deserialize(std::span<const std::uint8_t> payload) {
        shared::ExpectEmptyPayload(payload, "NicknameHandshake_A6");
        NicknameHandshake_A6 packet;
        return packet;
    }
};

}  // namespace cpp_server::packets::s2c
