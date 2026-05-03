#include "cpp_server/server/TcpLzssServer.h"

#include <chrono>

#include "cpp_server/packets/shared/PacketSupport.h"
#include "cpp_server/server/RoomStatePackets.h"

namespace cpp_server::server {

void TcpLzssServer::auto_send_full_room_state_commit(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    (void)frame;
    bool has_runtime_room_context = false;
    {
        std::scoped_lock lock(session->state_mutex);
        session->full_room_state_upload_deadline = std::chrono::steady_clock::time_point{};
        has_runtime_room_context = HasCreatedRoomContext(*session);
    }

    auto packet_84 = effective_options_for_session(session).packet_84;
    auto action = std::string{"opcode=0x84 -> payload-bearing 0x84 full-room-state reply logical"};
    if (!has_runtime_room_context) {
        packet_84.string0.clear();
        packet_84.string1.clear();
        action = "opcode=0x84 -> scrubbed pre-room 0x84 full-room-state completion logical";
        logger_.log("[auto] client=" + std::to_string(session->client_id) +
                    " opcode=0x84 sending scrubbed pre-room full-room-state completion");
    }

    const auto packet = packets::shared::ToFrame(packet_84);
    start_delayed_action(session, action,
                         config_.auto_delay_ms, [this, session, packet]() { session->send_logical_frame(packet, logger_); });
}

}  // namespace cpp_server::server
