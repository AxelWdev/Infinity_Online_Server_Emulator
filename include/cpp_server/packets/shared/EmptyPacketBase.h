#pragma once

#include <cstdint>
#include <vector>

#include "cpp_server/core/ByteBuffer.h"

namespace cpp_server::packets::shared {

template <std::uint8_t OpcodeValue>
struct EmptyPacketBase {
    static constexpr std::uint8_t kOpcode = OpcodeValue;

    [[nodiscard]] core::ByteVector serialize_payload() const {
        return {};
    }
};

}  // namespace cpp_server::packets::shared
