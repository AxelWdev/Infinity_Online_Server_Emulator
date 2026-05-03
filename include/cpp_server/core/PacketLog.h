#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "cpp_server/core/LogicalPacket.h"

namespace cpp_server::core {

[[nodiscard]] std::string OpcodeHex(std::uint8_t opcode);
[[nodiscard]] std::string PacketInfoText(std::uint8_t opcode);
[[nodiscard]] std::string PacketSummaryText(const LogicalPacketFrame& frame);
[[nodiscard]] std::string FormatPacketLogLine(
    std::string_view direction,
    int client_id,
    const LogicalPacketFrame& frame,
    std::string_view status = {});
[[nodiscard]] std::string FormatPacketPayloadLine(
    std::string_view direction,
    int client_id,
    const LogicalPacketFrame& frame);
[[nodiscard]] std::string FormatUnhandledPacketLine(
    int client_id,
    const LogicalPacketFrame& frame,
    std::string_view reason);
[[nodiscard]] std::optional<LogicalPacketFrame> TryParseSerializedLogicalPacket(
    std::span<const std::uint8_t> packet);

}  // namespace cpp_server::core
