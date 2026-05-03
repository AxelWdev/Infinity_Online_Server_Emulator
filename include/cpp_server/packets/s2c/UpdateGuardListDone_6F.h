#pragma once

#include <span>

#include "cpp_server/packets/shared/EmptyPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct UpdateGuardListDone_6F : shared::EmptyPacketBase<0x6F> {
    static UpdateGuardListDone_6F deserialize(std::span<const std::uint8_t> payload) {
        shared::ExpectEmptyPayload(payload, "UpdateGuardListDone_6F");
        return {};
    }
};

}  // namespace cpp_server::packets::s2c
