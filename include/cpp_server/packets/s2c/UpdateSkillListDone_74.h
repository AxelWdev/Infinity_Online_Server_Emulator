#pragma once

#include <span>

#include "cpp_server/packets/shared/EmptyPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct UpdateSkillListDone_74 : shared::EmptyPacketBase<0x74> {
    static UpdateSkillListDone_74 deserialize(std::span<const std::uint8_t> payload) {
        shared::ExpectEmptyPayload(payload, "UpdateSkillListDone_74");
        return {};
    }
};

}  // namespace cpp_server::packets::s2c
