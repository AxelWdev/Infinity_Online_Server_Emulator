#pragma once

#include <cstdint>
#include <span>

#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct TrainingStart_7F {
    static constexpr std::uint8_t kOpcode = 0x7F;

    std::uint8_t mode{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        return {mode};
    }

    static TrainingStart_7F deserialize(std::span<const std::uint8_t> payload) {
        ByteReader reader(payload);
        TrainingStart_7F packet;
        packet.mode = reader.read_u8();
        if (reader.remaining() != 0) {
            throw std::runtime_error("TrainingStart_7F expects a one-byte payload");
        }
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
