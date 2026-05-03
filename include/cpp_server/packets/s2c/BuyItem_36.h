#pragma once

#include <span>

#include "cpp_server/packets/shared/EmptyPacketBase.h"

namespace cpp_server::packets::s2c {

struct BuyItem_36 : shared::EmptyPacketBase<0x36> {
    static BuyItem_36 deserialize(std::span<const std::uint8_t> payload) {
        shared::ExpectEmptyPayload(payload, "BuyItem_36");
        return {};
    }
};

}  // namespace cpp_server::packets::s2c
