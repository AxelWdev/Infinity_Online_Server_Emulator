#include "cpp_server/server/RoomStatePackets.h"

namespace cpp_server::server {

bool HasCreatedRoomContext(const ClientSession& session) {
    return session.created_room_name || session.created_mission_title || session.created_mission_rule_id ||
           session.created_room_max_players;
}

std::uint32_t StableReconnectLoginDword(std::string_view login_id) {
    std::uint32_t value = 2166136261u;
    for (const unsigned char ch : login_id) {
        value ^= ch;
        value *= 16777619u;
    }
    return value == 0 ? 1u : value;
}

packets::s2c::LoginChallenge_9F BuildLoginSuccessPacket(const core::AccountRecord& account) {
    packets::s2c::LoginChallenge_9F packet;
    packet.reconnect_login_dword = StableReconnectLoginDword(account.login_id);
    packet.current_nickname = account.nickname.value_or("");
    packet.reconnect_login_string = account.login_id;
    return packet;
}

packets::s2c::RoomPlayerIdTable_1D BuildRoomPlayerIdTableFromState(
    const packets::s2c::RoomInfo_0D& room_info,
    const packets::s2c::RoomPlayerState_16& player_state) {
    packets::s2c::RoomPlayerIdTable_1D packet;
    packet.room_context = room_info.room_name;
    packet.local_player_name = player_state.player_name;
    packet.player_count = 1;
    packet.room_state_code = room_info.room_state_code;
    packet.rule_or_mission_id = room_info.rule_or_mission_id;
    packet.mission_icon_count = room_info.mission_icon_count;
    packet.limit_minutes = room_info.limit_minutes;
    packet.limit_kills = room_info.limit_kills;
    packet.player_ids[0] = player_state.player_id;
    return packet;
}

packets::s2c::RoomInfo_0D BuildRoomInfoFromRuntimeEntry(const packets::s2c::EnumGameRoom_0E& entry) {
    packets::s2c::RoomInfo_0D packet;
    packet.room_name = entry.primary_name;
    packet.field_01 = 0;
    packet.room_state_code = entry.room_state_code;
    packet.password_required_flag = entry.password_required_flag;
    packet.room_id = entry.room_id;
    packet.current_players = entry.current_players;
    packet.max_players = entry.max_players;
    packet.rule_or_mission_id = entry.rule_or_mission_id;
    packet.field_0a_reserved = entry.field_0a_reserved;
    packet.mission_icon_count = entry.mission_icon_count;
    packet.flags = entry.flags;
    packet.limit_minutes = entry.limit_minutes;
    packet.limit_kills = entry.limit_kills;
    return packet;
}

}  // namespace cpp_server::server
