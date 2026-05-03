#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "cpp_server/core/ByteBuffer.h"
#include "cpp_server/udp/UdpPeerState.h"
#include "cpp_server/udp/UdpProtocol.h"

namespace cpp_server::udp {

enum class OutgoingChannel {
    Reliable,
    Sequenced,
    Unsequenced,
};

struct OutgoingInnerPacket {
    OutgoingChannel channel{};
    std::uint8_t opcode{};
    core::ByteVector payload{};
    std::string description{};
};

struct CombatEventResult {
    std::vector<OutgoingInnerPacket> packets{};
    std::optional<std::string> log_line{};
};

[[nodiscard]] CombatEventResult HandleClientCombatEvent(
    PeerState& peer_state,
    const DecodedPacket& packet,
    bool experimental_game_udp_sync);

}  // namespace cpp_server::udp
