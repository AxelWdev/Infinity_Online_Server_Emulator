#include "cpp_server/server/TcpLzssServer.h"

#include <optional>

#include "cpp_server/packets/shared/PacketSupport.h"
#include "cpp_server/server/RoomStatePackets.h"
#include "cpp_server/server/SessionPacketEffects.h"

namespace cpp_server::server {

void TcpLzssServer::auto_send_room_info(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    (void)frame;
    {
        std::scoped_lock lock(session->state_mutex);
        if (!session->awaiting_room_enter_token) {
            logger_.log("[auto] client=" + std::to_string(session->client_id) +
                        " opcode=0x0D -> skipped payload-bearing 0x0D reply without a pending 0x8D trigger");
            return;
        }
        session->awaiting_room_enter_token = false;
    }
    const auto effective = effective_options_for_session(session);
    const auto room_info_packet = room_info_for_session(session, effective);
    const auto room_info = packets::shared::ToFrame(room_info_packet);
    const auto player_state = room_player_state_for_session(session);
    const auto player_id_table = player_state
                                     ? std::optional<packets::s2c::RoomPlayerIdTable_1D>(
                                           BuildRoomPlayerIdTableFromState(room_info_packet, *player_state))
                                     : std::nullopt;
    const auto packet_85_frames = packets::shared::ToFrames(room_slot_entries_for_session(session, effective));
    const auto packet_9e_frames = packets::shared::ToFrames(room_user_state_entries_for_session(session, effective));

    std::vector<core::LogicalPacketFrame> reseed_packets;
    reseed_packets.reserve((player_id_table ? 1U : 0U) + (player_state ? 1U : 0U) + packet_85_frames.size() +
                           packet_9e_frames.size());
    if (player_id_table) {
        reseed_packets.push_back(packets::shared::ToFrame(*player_id_table));
    }
    if (player_state) {
        reseed_packets.push_back(packets::shared::ToFrame(*player_state));
    } else {
        logger_.log("[auto] client=" + std::to_string(session->client_id) +
                    " opcode=0x0D -> no 0x16 room player state because no selected character was captured");
    }
    reseed_packets.insert(reseed_packets.end(), packet_85_frames.begin(), packet_85_frames.end());
    reseed_packets.insert(reseed_packets.end(), packet_9e_frames.begin(), packet_9e_frames.end());

    start_delayed_action(
        session,
        "opcode=0x0D -> payload-bearing 0x0D room-info reply logical",
        config_.auto_delay_ms,
        [this, session, room_info]() {
            {
                std::scoped_lock lock(session->state_mutex);
                if (session->pending_room_leave_request) {
                    logger_.log("[auto] client=" + std::to_string(session->client_id) +
                                " opcode=0x0D skipped delayed room-info reply after room leave");
                    return;
                }
            }
            ApplySentPacketSideEffects(session, room_info);
            session->send_logical_frame(room_info, logger_);
        });

    start_delayed_action(
        session,
        "opcode=0x0D -> delayed 0x1D/0x16/0x85 room-state reseed logical",
        config_.auto_delay_ms + 75,
        [this, session, reseed_packets = std::move(reseed_packets)]() {
            {
                std::scoped_lock lock(session->state_mutex);
                if (session->pending_room_leave_request) {
                    logger_.log("[auto] client=" + std::to_string(session->client_id) +
                                " opcode=0x0D skipped delayed room-state reseed after room leave");
                    return;
                }
            }
            for (const auto& packet : reseed_packets) {
                ApplySentPacketSideEffects(session, packet);
                session->send_logical_frame(packet, logger_);
            }
        });
}


}  // namespace cpp_server::server
