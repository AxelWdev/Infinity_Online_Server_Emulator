#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "cpp_server/core/ByteBuffer.h"
#include "cpp_server/core/SocketPlatform.h"

namespace cpp_server::udp {

struct DecodedPacket {
    std::uint8_t opcode{};
    core::ByteVector payload{};
    bool checksum_ok{};
};

[[nodiscard]] std::uint16_t ReadU16Le(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::uint32_t ReadU32Le(std::span<const std::uint8_t> bytes);
void AppendU16Le(core::ByteVector& bytes, std::uint16_t value);
void AppendU32Le(core::ByteVector& bytes, std::uint32_t value);
void AppendF32Le(core::ByteVector& bytes, float value);
void AppendUtf16Z(core::ByteVector& bytes, std::string_view text);

[[nodiscard]] std::string Ipv4FromPackedLe(std::uint32_t packed_ipv4);
[[nodiscard]] std::string HexU32(std::uint32_t value);
[[nodiscard]] std::string EndpointKey(const sockaddr_in& address);

[[nodiscard]] std::uint8_t TransportCheckByte(std::span<const std::uint8_t> bytes);
[[nodiscard]] bool TransportChecksumOk(std::span<const std::uint8_t> bytes);
[[nodiscard]] core::ByteVector BuildProbeReply(std::uint16_t incoming_word);
[[nodiscard]] core::ByteVector BuildAckList(std::uint16_t sequence_base, std::uint8_t ack_kind);

[[nodiscard]] core::ByteVector EncodeInnerPacket(std::uint8_t opcode, std::span<const std::uint8_t> payload);
[[nodiscard]] core::ByteVector BuildReliableFrame(std::uint16_t sequence_base, std::span<const std::uint8_t> inner_packet);
[[nodiscard]] core::ByteVector BuildSequencedFrame(std::uint16_t sequence_base, std::span<const std::uint8_t> inner_packet);
[[nodiscard]] core::ByteVector BuildUnsequencedFrame(std::span<const std::uint8_t> inner_packet);

[[nodiscard]] std::optional<DecodedPacket> DecodeInnerPacket(std::span<const std::uint8_t> encoded);
[[nodiscard]] std::vector<DecodedPacket> DecodeInnerStream(std::span<const std::uint8_t> encoded_stream);
[[nodiscard]] std::string DecodeUtf16Tail(std::span<const std::uint8_t> payload, std::size_t offset);
[[nodiscard]] std::string DescribeInnerPayload(std::uint8_t opcode, std::span<const std::uint8_t> payload);
[[nodiscard]] std::string DescribeInnerPacket(const DecodedPacket& packet);
[[nodiscard]] std::string DescribeInnerStreamDecode(std::span<const std::uint8_t> encoded_stream);
[[nodiscard]] std::string DescribeAckList(std::span<const std::uint8_t> payload, std::uint16_t ack_count);

}  // namespace cpp_server::udp
