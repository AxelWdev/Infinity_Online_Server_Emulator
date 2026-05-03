#pragma once

#include <cstdint>
#include <vector>

#include "cpp_server/core/ByteBuffer.h"

namespace cpp_server::packets::shared {

template <std::uint8_t OpcodeValue>
struct RawPacketBase {
    static constexpr std::uint8_t kOpcode = OpcodeValue;

    core::ByteVector payload_bytes{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        return payload_bytes;
    }
};

}  // namespace cpp_server::packets::shared
