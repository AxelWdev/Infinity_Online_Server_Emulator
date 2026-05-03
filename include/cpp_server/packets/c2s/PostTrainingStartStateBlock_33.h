#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct PostTrainingStartStateBlock_33 {
    static constexpr std::uint8_t kOpcode = 0x33;

    std::array<std::uint8_t, 12> state_bytes{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        return core::ByteVector(state_bytes.begin(), state_bytes.end());
    }

    static PostTrainingStartStateBlock_33 deserialize(std::span<const std::uint8_t> payload) {
        if (payload.size() != 12) {
            throw std::runtime_error("PostTrainingStartStateBlock_33 expects a 12-byte payload");
        }

        PostTrainingStartStateBlock_33 packet;
        for (std::size_t index = 0; index < packet.state_bytes.size(); ++index) {
            packet.state_bytes[index] = payload[index];
        }
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
