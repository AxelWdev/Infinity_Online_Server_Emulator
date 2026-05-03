#pragma once

#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "cpp_server/core/ByteBuffer.h"
#include "cpp_server/core/LogicalPacket.h"

namespace cpp_server::packets {

using core::ByteReader;
using core::ByteVector;
using core::ByteWriter;

}  // namespace cpp_server::packets

namespace cpp_server::packets::shared {

using core::ByteReader;
using core::ByteVector;
using core::ByteWriter;

inline ByteVector CopyPayload(std::span<const std::uint8_t> payload) {
    return ByteVector(payload.begin(), payload.end());
}

inline void ExpectEmptyPayload(std::span<const std::uint8_t> payload, std::string_view packet_name) {
    if (!payload.empty()) {
        throw std::runtime_error(std::string(packet_name) + " expects an empty payload");
    }
}

template <typename PacketT>
[[nodiscard]] inline core::LogicalPacketFrame ToFrame(const PacketT& packet) {
    return core::LogicalPacketFrame{PacketT::kOpcode, packet.serialize_payload()};
}

template <typename PacketT>
[[nodiscard]] inline std::vector<core::LogicalPacketFrame> ToFrames(const std::vector<PacketT>& packets) {
    std::vector<core::LogicalPacketFrame> frames;
    frames.reserve(packets.size());
    for (const auto& packet : packets) {
        frames.push_back(ToFrame(packet));
    }
    return frames;
}

}  // namespace cpp_server::packets::shared
