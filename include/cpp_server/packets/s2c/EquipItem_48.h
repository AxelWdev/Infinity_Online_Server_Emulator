#pragma once

#include <span>

#include "cpp_server/packets/shared/EmptyPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct EquipItem_48 : shared::EmptyPacketBase<0x48> {
    static EquipItem_48 deserialize(std::span<const std::uint8_t> payload) {
        shared::ExpectEmptyPayload(payload, "EquipItem_48");
        return {};
    }
};

}  // namespace cpp_server::packets::s2c
