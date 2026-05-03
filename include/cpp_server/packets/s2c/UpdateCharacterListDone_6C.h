#pragma once

#include <span>

#include "cpp_server/packets/shared/EmptyPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct UpdateCharacterListDone_6C : shared::EmptyPacketBase<0x6C> {
    static UpdateCharacterListDone_6C deserialize(std::span<const std::uint8_t> payload) {
        shared::ExpectEmptyPayload(payload, "UpdateCharacterListDone_6C");
        return {};
    }
};

}  // namespace cpp_server::packets::s2c
