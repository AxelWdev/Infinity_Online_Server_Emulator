#pragma once

#include <span>

#include "cpp_server/packets/shared/EmptyPacketBase.h"

namespace cpp_server::packets::s2c {

struct AssignQuickSlot_47 : shared::EmptyPacketBase<0x47> {
    static AssignQuickSlot_47 deserialize(std::span<const std::uint8_t> payload) {
        shared::ExpectEmptyPayload(payload, "AssignQuickSlot_47");
        return {};
    }
};

}  // namespace cpp_server::packets::s2c
