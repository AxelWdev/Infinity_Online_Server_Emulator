#include "cpp_server/server/TcpLzssServer.h"

#include <optional>

#include "cpp_server/core/LogicalPacket.h"
#include "cpp_server/game/InventoryService.h"

namespace cpp_server::server {

std::optional<udp::InitialSyncContext> TcpLzssServer::game_udp_initial_sync_context() {
    const auto client_id = game_udp_pending_client_id_.load();
    if (client_id <= 0) {
        logger_.log("[game-udp][sync] no pending TCP room-start client is associated with this UDP peer yet");
        return std::nullopt;
    }

    ClientSessionPtr session;
    {
        std::scoped_lock lock(sessions_mutex_);
        const auto session_it = sessions_.find(client_id);
        if (session_it != sessions_.end() && !session_it->second->closed) {
            session = session_it->second;
        }
    }
    if (!session) {
        logger_.log("[game-udp][sync] pending client=" + std::to_string(client_id) +
                    " is no longer connected");
        return std::nullopt;
    }

    const auto room = runtime_room_entry_for_session(session);
    if (!room) {
        logger_.log("[game-udp][sync] client=" + std::to_string(client_id) +
                    " has no runtime room for initial UDP sync");
        return std::nullopt;
    }

    const auto mission = game_data_catalog_.find_mission(room->rule_or_mission_id);
    if (!mission || mission->scene_key.empty()) {
        logger_.log("[game-udp][sync] client=" + std::to_string(client_id) +
                    " room_rule=" + std::to_string(room->rule_or_mission_id) +
                    " has no confirmed scene_key; withholding experimental gameplay sync");
        return std::nullopt;
    }

    const auto player_state = room_player_state_for_session(session);
    if (!player_state) {
        logger_.log("[game-udp][sync] client=" + std::to_string(client_id) +
                    " has no selected character/player state for initial UDP sync");
        return std::nullopt;
    }

    std::vector<std::uint16_t> player_skill_ids;
    {
        std::optional<std::string> login_id;
        {
            std::scoped_lock lock(session->state_mutex);
            login_id = session->authenticated_login_id;
        }
        if (login_id) {
            if (const auto inventory = account_database_.find_inventory(*login_id)) {
                player_skill_ids =
                    game::OwnedSkillIdsForCharacter(game_data_catalog_, *inventory, player_state->field_0d);
            }
        }
    }

    udp::InitialSyncContext context;
    context.client_id = client_id;
    context.room = *room;
    context.player_state = *player_state;
    context.scene_key = mission->scene_key;
    context.player_spawn_position = mission->player_spawn_position;
    context.player_skill_ids = std::move(player_skill_ids);
    return context;
}

void TcpLzssServer::send_mission_result_from_udp(udp::MissionResultEvent event) {
    ClientSessionPtr session;
    {
        std::scoped_lock lock(sessions_mutex_);
        const auto session_it = sessions_.find(event.client_id);
        if (session_it != sessions_.end() && !session_it->second->closed) {
            session = session_it->second;
        }
    }

    if (!session) {
        logger_.log("[game-udp][mission-result] client=" + std::to_string(event.client_id) +
                    " is no longer connected; dropped TCP opcode=0x29 result");
        return;
    }

    core::ByteVector payload;
    payload.push_back(event.next_mission);
    payload.push_back(event.mission_result);
    const auto frame = core::BuildLogicalPacket(0x29, payload);
    const auto result_text = event.mission_result == 1 ? "success" : "failure";
    const auto delay_ms = event.delay_ms < 0 ? 0 : event.delay_ms;

    start_delayed_action(
        session,
        "opcode=0x29 mission-result from UDP result=" + std::string(result_text) +
            " rule=" + std::to_string(event.rule_field) + " scene='" + event.scene_key +
            "' reason='" + event.reason + "'",
        delay_ms,
        [this, session, frame]() { session->send_logical_frame(frame, logger_); });
}

}  // namespace cpp_server::server
