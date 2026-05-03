#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

#include "cpp_server/core/ByteBuffer.h"

namespace cpp_server::core {

struct LogicalPacketFrame {
    std::uint8_t opcode{};
    ByteVector payload{};

    [[nodiscard]] ByteVector serialize() const {
        if (payload.size() > 0xFF) {
            throw std::runtime_error("logical packet payload must fit in one byte");
        }

        ByteWriter writer;
        writer.write_u8(opcode);
        writer.write_u8(static_cast<std::uint8_t>(payload.size()));
        writer.write_bytes(payload);
        return writer.take();
    }
};

[[nodiscard]] inline LogicalPacketFrame BuildLogicalPacket(std::uint8_t opcode, std::span<const std::uint8_t> payload) {
    return LogicalPacketFrame{opcode, ByteVector(payload.begin(), payload.end())};
}

[[nodiscard]] inline LogicalPacketFrame BuildLogicalPacket(std::uint8_t opcode) {
    return LogicalPacketFrame{opcode, {}};
}

[[nodiscard]] inline LogicalPacketFrame BuildZeroPayloadPacket(std::uint8_t opcode, std::size_t payload_len) {
    return LogicalPacketFrame{opcode, ByteVector(payload_len, 0)};
}

[[nodiscard]] inline std::optional<LogicalPacketFrame> TryPopLogicalPacket(ByteVector& buffer) {
    if (buffer.size() < 2) {
        return std::nullopt;
    }

    const auto opcode = buffer[0];
    const auto payload_len = buffer[1];
    const auto frame_len = static_cast<std::size_t>(2 + payload_len);
    if (buffer.size() < frame_len) {
        return std::nullopt;
    }

    LogicalPacketFrame frame;
    frame.opcode = opcode;
    frame.payload.assign(buffer.begin() + 2, buffer.begin() + static_cast<std::ptrdiff_t>(frame_len));
    buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(frame_len));
    return frame;
}

}  // namespace cpp_server::core
