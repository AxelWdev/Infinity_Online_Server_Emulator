#include "cpp_server/server/TcpLzssServer.h"

#include <optional>

#include "cpp_server/server/RoomStatePackets.h"

namespace cpp_server::server {

std::vector<packets::s2c::EnumGameRoom_0E> TcpLzssServer::created_room_entries() {
    return room_registry_.entries();
}

std::optional<packets::s2c::EnumGameRoom_0E> TcpLzssServer::runtime_room_entry_for_session(
    const ClientSessionPtr& session) {
    std::optional<std::string> login_id;
    {
        std::scoped_lock lock(session->state_mutex);
        login_id = session->authenticated_login_id;
    }

    return room_registry_.find_for_host(session->client_id, login_id);
}

packets::s2c::RoomInfo_0D TcpLzssServer::room_info_for_session(
    const ClientSessionPtr& session,
    const core::ServerOptions& effective_options) {
    if (const auto runtime_room = runtime_room_entry_for_session(session)) {
        auto packet = BuildRoomInfoFromRuntimeEntry(*runtime_room);
        logger_.log("[room] client=" + std::to_string(session->client_id) +
                    " built 0x0D from runtime room id=" + std::to_string(packet.room_id) +
                    " name='" + packet.room_name + "' current_players=" +
                    std::to_string(packet.current_players) + " max_players=" +
                    std::to_string(packet.max_players) + " rule_or_mission_id=" +
                    std::to_string(packet.rule_or_mission_id));
        return packet;
    }

    logger_.log("[room] client=" + std::to_string(session->client_id) +
                " built 0x0D from option fallback because no runtime room context was found");
    return effective_options.packet_0d;
}

std::string TcpLzssServer::character_list_source_for_session(const ClientSessionPtr& session) {
    std::optional<std::string> authenticated_login_id;
    {
        std::scoped_lock lock(session->state_mutex);
        authenticated_login_id = session->authenticated_login_id;
    }

    if (authenticated_login_id) {
        if (const auto profile = account_database_.find_packet_profile(*authenticated_login_id)) {
            if (profile->inventory) {
                return "account_database.inventory";
            }
            if (profile->character_list) {
                return "account_database.packet_6b_entries";
            }
        }
    }

    return "none";
}

std::string TcpLzssServer::item_list_source_for_session(const ClientSessionPtr& session) {
    std::optional<std::string> authenticated_login_id;
    {
        std::scoped_lock lock(session->state_mutex);
        authenticated_login_id = session->authenticated_login_id;
    }

    if (authenticated_login_id) {
        if (const auto profile = account_database_.find_packet_profile(*authenticated_login_id)) {
            if (profile->inventory) {
                return "account_database.inventory";
            }
            if (profile->item_list) {
                return "account_database.packet_3f_entries";
            }
        }
    }

    return "none";
}

}  // namespace cpp_server::server
