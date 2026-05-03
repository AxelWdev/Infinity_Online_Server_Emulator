#pragma once

#include <cstdint>
#include <span>

#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct PostTrainingStartState_22 {
    static constexpr std::uint8_t kOpcode = 0x22;

    std::uint8_t state_value{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        return {state_value};
    }

    static PostTrainingStartState_22 deserialize(std::span<const std::uint8_t> payload) {
        ByteReader reader(payload);
        PostTrainingStartState_22 packet;
        packet.state_value = reader.read_u8();
        if (reader.remaining() != 0) {
            throw std::runtime_error("PostTrainingStartState_22 expects a one-byte payload");
        }
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
