#include "cpp_server/server/TcpLzssServer.h"

#include <optional>

#include "cpp_server/packets/shared/PacketSupport.h"
#include "cpp_server/server/RoomStatePackets.h"
#include "cpp_server/server/SessionPacketEffects.h"

namespace cpp_server::server {

void TcpLzssServer::auto_send_room_character_select_state(
    ClientSessionPtr session,
    const core::LogicalPacketFrame& frame) {
    bool is_room_character_select = false;
    {
        std::scoped_lock lock(session->state_mutex);
        is_room_character_select = HasCreatedRoomContext(*session) &&
                                   !session->awaiting_room_create_completion &&
                                   !session->pending_room_leave_request;
    }

    if (!is_room_character_select) {
        handle_observed_no_response_packet(session, frame);
        return;
    }

    const auto effective = effective_options_for_session(session);
    const auto player_state = room_player_state_for_session(session);
    const auto player_state_frame = player_state ? std::optional<core::LogicalPacketFrame>(
                                                       packets::shared::ToFrame(*player_state))
                                                  : std::nullopt;
    const auto packet_85_frames = packets::shared::ToFrames(room_slot_entries_for_session(session, effective));

    start_delayed_action(
        session,
        "opcode=0x6D -> refreshed in-room 0x16/0x85 character-select state logical",
        config_.auto_delay_ms,
        [this, session, player_state_frame, packet_85_frames]() {
            {
                std::scoped_lock lock(session->state_mutex);
                if (session->pending_room_leave_request || session->awaiting_room_create_completion) {
                    logger_.log("[auto] client=" + std::to_string(session->client_id) +
                                " opcode=0x6D skipped delayed in-room character-select refresh after room state changed");
                    return;
                }
            }

            if (player_state_frame) {
                ApplySentPacketSideEffects(session, *player_state_frame);
                session->send_logical_frame(*player_state_frame, logger_);
            } else {
                logger_.log("[auto] client=" + std::to_string(session->client_id) +
                            " opcode=0x6D -> no 0x16 room player state because no selected character was captured");
            }

            for (const auto& packet : packet_85_frames) {
                ApplySentPacketSideEffects(session, packet);
                session->send_logical_frame(packet, logger_);
            }
        });
}

}  // namespace cpp_server::server
