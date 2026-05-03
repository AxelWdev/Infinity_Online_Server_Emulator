#include "cpp_server/server/TcpLzssServer.h"

#include <optional>

#include "cpp_server/core/PacketLog.h"
#include "cpp_server/packets/shared/PacketSupport.h"
#include "cpp_server/server/RoomStatePackets.h"
#include "cpp_server/server/SessionPacketEffects.h"

namespace cpp_server::server {

void TcpLzssServer::auto_send_room_create_completion(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    (void)frame;
    {
        std::scoped_lock lock(session->state_mutex);
        if (!session->awaiting_room_create_completion) {
            logger_.log("[auto] client=" + std::to_string(session->client_id) +
                        " opcode=0x0C -> skipped inbound 0x17 completion without a pending room-create context");
            return;
        }
        session->awaiting_room_create_completion = false;
    }
    const auto effective = effective_options_for_session(session);
    const auto room_info_packet = room_info_for_session(session, effective);
    const auto room_info = packets::shared::ToFrame(room_info_packet);
    const auto player_state = room_player_state_for_session(session);
    const auto player_state_frame = player_state ? std::optional<core::LogicalPacketFrame>(
                                                       packets::shared::ToFrame(*player_state))
                                                  : std::nullopt;
    const auto player_id_table_frame =
        player_state ? std::optional<core::LogicalPacketFrame>(
                           packets::shared::ToFrame(BuildRoomPlayerIdTableFromState(room_info_packet, *player_state)))
                     : std::nullopt;
    const auto packet_85_frames = packets::shared::ToFrames(room_slot_entries_for_session(session, effective));
    const auto packet_9e_frames = packets::shared::ToFrames(room_user_state_entries_for_session(session, effective));
    const auto completion = core::BuildLogicalPacket(0x17);
    start_delayed_action(
        session,
        "opcode=0x0C -> inbound 0x0D/0x1D/0x16/0x85/0x17 room-create state logical",
        config_.auto_delay_ms,
        [this,
         session,
         room_info,
         player_id_table_frame,
         player_state_frame,
         packet_85_frames,
         packet_9e_frames,
         completion]() {
            ApplySentPacketSideEffects(session, room_info);
            session->send_logical_frame(room_info, logger_);
            if (player_id_table_frame) {
                ApplySentPacketSideEffects(session, *player_id_table_frame);
                session->send_logical_frame(*player_id_table_frame, logger_);
            }
            if (player_state_frame) {
                ApplySentPacketSideEffects(session, *player_state_frame);
                session->send_logical_frame(*player_state_frame, logger_);
            } else {
                logger_.log("[auto] client=" + std::to_string(session->client_id) +
                            " opcode=0x0C -> no 0x16 room player state because no selected character was captured");
            }
            for (const auto& packet : packet_85_frames) {
                ApplySentPacketSideEffects(session, packet);
                session->send_logical_frame(packet, logger_);
            }
            for (const auto& packet : packet_9e_frames) {
                ApplySentPacketSideEffects(session, packet);
                session->send_logical_frame(packet, logger_);
            }
            ApplySentPacketSideEffects(session, completion);
            session->send_logical_frame(completion, logger_);
        });
}

}  // namespace cpp_server::server
