#include "cpp_server/server/TcpLzssServer.h"

#include <algorithm>
#include <chrono>
#include <optional>

namespace cpp_server::server {

void TcpLzssServer::upsert_created_room_from_session(const ClientSessionPtr& session) {
    std::optional<std::string> login_id;
    std::optional<std::string> mission_title;
    std::optional<std::uint16_t> mission_rule_id;
    std::optional<std::uint8_t> max_players;
    {
        std::scoped_lock lock(session->state_mutex);
        login_id = session->authenticated_login_id;
        mission_title = session->created_mission_title;
        mission_rule_id = session->created_mission_rule_id;
        max_players = session->created_room_max_players;
    }

    if (!mission_title || mission_title->empty() || !mission_rule_id) {
        return;
    }

    packets::s2c::EnumGameRoom_0E entry;
    entry.primary_name = *mission_title;
    entry.current_players = 1;
    entry.max_players = max_players.value_or(4);
    entry.rule_or_mission_id = *mission_rule_id;
    const auto mission_catalog_entry = game_data_catalog_.find_mission(*mission_rule_id);
    if (mission_catalog_entry && mission_catalog_entry->max_players != 0) {
        entry.max_players = mission_catalog_entry->max_players;
    }

    const auto configured_options = options();
    const auto template_it = std::find_if(
        configured_options.packet_0e_entries.begin(),
        configured_options.packet_0e_entries.end(),
        [&](const auto& configured) { return configured.rule_or_mission_id == *mission_rule_id; });
    if (template_it != configured_options.packet_0e_entries.end()) {
        entry.secondary_name = template_it->secondary_name;
        entry.room_state_code = template_it->room_state_code;
        entry.password_required_flag = template_it->password_required_flag;
        entry.field_0a_reserved = template_it->field_0a_reserved;
        entry.mission_icon_count = template_it->mission_icon_count;
        entry.flags = template_it->flags;
        entry.limit_minutes = template_it->limit_minutes;
        entry.limit_kills = template_it->limit_kills;
    }

    const auto upsert_result = room_registry_.upsert(session->client_id, login_id, std::move(entry));
    if (upsert_result.created) {
        logger_.log("[room] client=" + std::to_string(session->client_id) +
                    " created runtime room id=" + std::to_string(upsert_result.entry.room_id) +
                    " mission_rule_id=" + std::to_string(*mission_rule_id) +
                    " title='" + upsert_result.entry.primary_name + "'" +
                    (mission_catalog_entry
                         ? " mission_ui_key='" + mission_catalog_entry->mission_ui_key + "' catalog_title='" +
                               mission_catalog_entry->english_room_title + "' scene_key='" +
                               mission_catalog_entry->scene_key + "'"
                         : " mission_ui_key=<not found in mission.csv>"));
    } else {
        logger_.log("[room] client=" + std::to_string(session->client_id) +
                    " updated runtime room id=" + std::to_string(upsert_result.entry.room_id) +
                    " mission_rule_id=" + std::to_string(*mission_rule_id) +
                    " title='" + upsert_result.entry.primary_name + "'" +
                    (mission_catalog_entry
                         ? " mission_ui_key='" + mission_catalog_entry->mission_ui_key + "' catalog_title='" +
                               mission_catalog_entry->english_room_title + "' scene_key='" +
                               mission_catalog_entry->scene_key + "'"
                         : " mission_ui_key=<not found in mission.csv>"));
    }
}

void TcpLzssServer::remove_created_rooms_for_session(const ClientSessionPtr& session) {
    std::optional<std::string> login_id;
    {
        std::scoped_lock lock(session->state_mutex);
        login_id = session->authenticated_login_id;
    }

    if (room_registry_.remove_for_host(session->client_id, login_id)) {
        logger_.log("[room] client=" + std::to_string(session->client_id) +
                    " removed runtime room(s) for host");
    }
}

void TcpLzssServer::clear_created_room_state_for_session(const ClientSessionPtr& session) {
    std::scoped_lock lock(session->state_mutex);
    session->created_room_name.reset();
    session->created_mission_title.reset();
    session->created_mission_rule_id.reset();
    session->created_room_max_players.reset();
    session->awaiting_room_create_completion = false;
    session->awaiting_room_enter_token = false;
    session->pending_room_leave_request = false;
    session->full_room_state_upload_deadline = std::chrono::steady_clock::time_point{};
}

}  // namespace cpp_server::server
