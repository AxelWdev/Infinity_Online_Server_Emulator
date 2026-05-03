#include "cpp_server/server/TcpLzssServer.h"

#include <algorithm>
#include <chrono>

#include "cpp_server/packets/c2s/PacketCatalog.h"
#include "cpp_server/server/RoomStatePackets.h"

namespace cpp_server::server {

void TcpLzssServer::capture_dynamic_room_state(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    if (frame.opcode == 0x6D) {
        std::scoped_lock lock(session->state_mutex);
        const bool is_room_character_select = HasCreatedRoomContext(*session) &&
                                              !session->awaiting_room_create_completion &&
                                              !session->pending_room_leave_request;
        session->awaiting_room_create_completion = !is_room_character_select;
        session->awaiting_room_enter_token = false;
        session->pending_room_leave_request = false;
        if (!is_room_character_select) {
            session->created_room_name.reset();
        }
        if (frame.payload.size() == 4) {
            const auto selected_character_id = static_cast<std::uint32_t>(frame.payload[0]) |
                                               (static_cast<std::uint32_t>(frame.payload[1]) << 8) |
                                               (static_cast<std::uint32_t>(frame.payload[2]) << 16) |
                                               (static_cast<std::uint32_t>(frame.payload[3]) << 24);
            session->observed_equipment_character_id = selected_character_id;
            logger_.log("[state] client=" + std::to_string(session->client_id) +
                        " captured observed selected character_id=" + std::to_string(selected_character_id) +
                        (is_room_character_select ? " from in-room 0x6D character-select"
                                                  : " from pre-create 0x6D sideband"));
        }
        return;
    }

    if (frame.opcode == 0x84 && !frame.payload.empty()) {
        try {
            const auto packet = packets::c2s::FullRoomStateUploadTail_84::deserialize(frame.payload);
            logger_.log("[state] client=" + std::to_string(session->client_id) + " observed 0x84 upload tail name='" +
                        packet.room_name + "'; not using it as room_name");
        } catch (...) {
        }
        return;
    }

    if (frame.opcode == 0x0C && frame.payload.size() >= 9) {
        try {
            const auto packet = packets::c2s::MissionRoomMetadataUpload_0C::deserialize(frame.payload);
            {
                std::scoped_lock lock(session->state_mutex);
                session->awaiting_room_create_completion = true;
                session->awaiting_room_enter_token = false;
                session->pending_room_leave_request = false;
                session->created_mission_title = packet.mission_title;
                session->created_room_max_players = packet.max_players;
                session->created_mission_rule_id = packet.mission_rule_id;
            }
            logger_.log("[state] client=" + std::to_string(session->client_id) + " captured mission_rule_id=" +
                        std::to_string(packet.mission_rule_id) + " max_players=" + std::to_string(packet.max_players) +
                        " mission_title='" + packet.mission_title + "' from 0x0C; armed room-create completion");
            upsert_created_room_from_session(session);
        } catch (...) {
        }
    }
}

void TcpLzssServer::note_packet_context(ClientSessionPtr session, std::uint8_t opcode) {
    const auto now = std::chrono::steady_clock::now();
    std::scoped_lock lock(session->state_mutex);
    session->last_opcode = opcode;
    session->last_packet_monotonic = now;
    if (opcode == 0x85 || opcode == 0x9E) {
        session->full_room_state_upload_deadline =
            std::max(session->full_room_state_upload_deadline, now + std::chrono::seconds(2));
    } else if (opcode == 0x84) {
        session->full_room_state_upload_deadline = now + std::chrono::milliseconds(500);
    }
}

bool TcpLzssServer::is_full_room_state_upload_active(const ClientSessionPtr& session) {
    std::scoped_lock lock(session->state_mutex);
    return std::chrono::steady_clock::now() <= session->full_room_state_upload_deadline;
}

void TcpLzssServer::handle_state_update_upload(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    try {
        const auto packet = packets::c2s::StateUpdate_9E::deserialize(frame.payload);
        auto message = "[state] client=" + std::to_string(session->client_id) +
                       " observed client 0x9E mission/state upload entry_id=" +
                       std::to_string(packet.state_entry_id) + " player_name='" + packet.player_name + "'";
        if (!packet.trailing_bytes.empty()) {
            message += " trailing_bytes=" + std::to_string(packet.trailing_bytes.size());
        }
        logger_.log(message + "; no server reply known for this upload row");
    } catch (const std::exception& ex) {
        logger_.log("[state] client=" + std::to_string(session->client_id) +
                    " invalid client 0x9E mission/state upload payload: " + ex.what());
    }
}

}  // namespace cpp_server::server
