#include "cpp_server/server/TcpLzssServer.h"

#include <algorithm>
#include <optional>

#include "cpp_server/game/InventoryService.h"

namespace cpp_server::server {

core::ServerOptions TcpLzssServer::options() {
    return options_store_.load();
}

core::ServerOptions TcpLzssServer::effective_options_for_session(const ClientSessionPtr& session) {
    auto effective = options();
    // These packets carry account-owned state and must not inherit global defaults.
    effective.packet_6b_entries.clear();
    effective.packet_3f_entries.clear();
    effective.packet_44_entries.clear();
    effective.packet_73_entries.clear();
    std::optional<std::string> authenticated_login_id;

    {
        std::scoped_lock lock(session->state_mutex);
        authenticated_login_id = session->authenticated_login_id;
    }

    if (authenticated_login_id) {
        if (const auto profile = account_database_.find_packet_profile(*authenticated_login_id)) {
            if (profile->account_info) {
                effective.packet_70 = *profile->account_info;
                effective.has_packet_42_entries = false;
                effective.packet_42_entries.clear();
            }
            if (profile->character_list) {
                effective.packet_6b_entries = *profile->character_list;
            }
            if (profile->item_list) {
                effective.packet_3f_entries = *profile->item_list;
            }
            if (profile->quickslot_list) {
                effective.packet_44_entries = *profile->quickslot_list;
            }
            if (profile->skill_list) {
                effective.packet_73_entries = *profile->skill_list;
            }
            if (profile->guard_list) {
                effective.packet_6e_entries = *profile->guard_list;
                effective.has_packet_42_entries = false;
                effective.packet_42_entries.clear();
            }
            if (profile->inventory) {
                effective.packet_6b_entries = game::BuildCharacterListFromInventory(*profile->inventory);
                effective.packet_3f_entries = game::BuildItemListFromInventory(*profile->inventory);
                effective.packet_73_entries = game::BuildSkillListFromInventory(game_data_catalog_, *profile->inventory);

                // Keep reading legacy packet_73_entries if this account has not been migrated yet.
                if (game::IsPlaceholderSkillList(effective.packet_73_entries) && profile->skill_list) {
                    effective.packet_73_entries = *profile->skill_list;
                }
            }
        }
    }

    if (game::IsPlaceholderQuickslotList(effective.packet_44_entries) && authenticated_login_id) {
        if (const auto inventory = account_database_.find_inventory(*authenticated_login_id)) {
            effective.packet_44_entries = game::BuildQuickslotListFromInventoryItems(game_data_catalog_, *inventory);
        }
    }

    game::NormalizeQuickslotList(effective.packet_44_entries);
    game::NormalizeSkillDurations(effective.packet_73_entries);

    std::scoped_lock lock(session->state_mutex);
    if (session->created_room_name || session->created_mission_title || session->created_mission_rule_id ||
        session->created_room_max_players) {
        if (effective.packet_0e_entries.empty()) {
            effective.packet_0e_entries.emplace_back();
        }

        auto& entry = effective.packet_0e_entries.front();
        if (session->created_room_name) {
            entry.primary_name = *session->created_room_name;
            effective.packet_84.string0 = *session->created_room_name;
            effective.packet_0d.room_name = *session->created_room_name;
        }
        if (session->created_mission_title) {
            if (!session->created_room_name) {
                entry.primary_name = *session->created_mission_title;
                effective.packet_84.string0 = *session->created_mission_title;
                effective.packet_0d.room_name = *session->created_mission_title;
            }
            entry.secondary_name = *session->created_mission_title;
            effective.packet_84.string1 = *session->created_mission_title;
        }
        if (session->created_mission_rule_id) {
            entry.rule_or_mission_id = *session->created_mission_rule_id;
            effective.packet_0d.rule_or_mission_id = *session->created_mission_rule_id;
        }
        if (session->created_room_max_players) {
            entry.max_players = *session->created_room_max_players;
            entry.current_players = std::max<std::uint8_t>(1, entry.current_players == 0 ? 1 : entry.current_players);
            effective.packet_0d.max_players = *session->created_room_max_players;
            effective.packet_0d.current_players = entry.current_players;
        }
    }

    return effective;
}

}  // namespace cpp_server::server
