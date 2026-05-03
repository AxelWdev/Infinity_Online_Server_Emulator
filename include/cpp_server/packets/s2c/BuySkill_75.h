#pragma once

#include <span>

#include "cpp_server/packets/shared/EmptyPacketBase.h"

namespace cpp_server::packets::s2c {

struct BuySkill_75 : shared::EmptyPacketBase<0x75> {
    static BuySkill_75 deserialize(std::span<const std::uint8_t> payload) {
        shared::ExpectEmptyPayload(payload, "BuySkill_75");
        return {};
    }
};

}  // namespace cpp_server::packets::s2c
