#include "cpp_server/server/TcpLzssServer.h"

#include <chrono>

#include "cpp_server/core/PacketLog.h"
#include "cpp_server/server/SessionPacketEffects.h"

namespace cpp_server::server {

void TcpLzssServer::handle_room_leave_request(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    logger_.log("[room] client=" + std::to_string(session->client_id) +
                " opcode=0x12 observed room leave/reset request payload=" + core::HexBytes(frame.payload));
    {
        std::scoped_lock lock(session->state_mutex);
        session->pending_room_leave_request = true;
        session->awaiting_room_create_completion = false;
        session->awaiting_room_enter_token = false;
        session->full_room_state_upload_deadline = std::chrono::steady_clock::time_point{};
    }

    const auto reset_ack = core::BuildLogicalPacket(0x12);
    ApplySentPacketSideEffects(session, reset_ack);
    session->send_logical_frame(reset_ack, logger_);
    remove_created_rooms_for_session(session);
    clear_created_room_state_for_session(session);
    logger_.log("[room] client=" + std::to_string(session->client_id) +
                " opcode=0x12 sent room reset acknowledgement and cleared hosted room state");
}

}  // namespace cpp_server::server
