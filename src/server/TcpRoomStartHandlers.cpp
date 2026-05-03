#include "cpp_server/server/TcpLzssServer.h"

#include "cpp_server/core/PacketLog.h"
#include "cpp_server/packets/s2c/GameServerConnect_10.h"
#include "cpp_server/packets/shared/PacketSupport.h"
#include "cpp_server/server/SessionPacketEffects.h"

namespace cpp_server::server {

void TcpLzssServer::handle_room_start_request(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    packets::s2c::GameServerConnect_10 packet;
    packet.packed_ipv4 = auto_enum_packed_ipv4_;
    packet.udp_port = game_udp_server_.bound_port();
    const auto reply = packets::shared::ToFrame(packet);
    game_udp_pending_client_id_.store(session->client_id);

    logger_.log("[auto] client=" + std::to_string(session->client_id) +
                " opcode=0x" + core::OpcodeHex(frame.opcode) +
                " observed room-start request payload=" + core::HexBytes(frame.payload) +
                "; replying with 0x10 game-server endpoint " + core::FormatIpv4(packet.packed_ipv4) + ":" +
                std::to_string(packet.udp_port));

    start_delayed_action(
        session,
        "opcode=0x" + core::OpcodeHex(frame.opcode) + " -> 0x10 game-server endpoint logical",
        config_.auto_delay_ms,
        [this, session, reply]() {
            ApplySentPacketSideEffects(session, reply);
            session->send_logical_frame(reply, logger_);
        });
}

}  // namespace cpp_server::server
