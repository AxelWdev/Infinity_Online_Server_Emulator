#pragma once

#include <span>

#include "cpp_server/packets/shared/RawPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct UpdateSkillList_73 : shared::RawPacketBase<0x73> {
    static UpdateSkillList_73 deserialize(std::span<const std::uint8_t> payload) {
        UpdateSkillList_73 packet;
        packet.payload_bytes = shared::CopyPayload(payload);
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
