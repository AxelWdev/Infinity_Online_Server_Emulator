#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct EnumHakanTrainingClearInfo_9D {
    static constexpr std::uint8_t kOpcode = 0x9D;

    std::array<std::uint8_t, 4> clear_flags{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        return core::ByteVector(clear_flags.begin(), clear_flags.end());
    }

    static EnumHakanTrainingClearInfo_9D deserialize(std::span<const std::uint8_t> payload) {
        if (payload.size() != 4) {
            throw std::runtime_error("EnumHakanTrainingClearInfo_9D expects a four-byte payload");
        }

        EnumHakanTrainingClearInfo_9D packet;
        for (std::size_t index = 0; index < packet.clear_flags.size(); ++index) {
            packet.clear_flags[index] = payload[index];
        }
        return packet;
    }
};

}  // namespace cpp_server::packets::s2c
