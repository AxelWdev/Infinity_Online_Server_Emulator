#pragma once

#include <cstdint>
#include <string_view>

#include "cpp_server/core/AccountDatabase.h"
#include "cpp_server/packets/s2c/LoginChallenge_9F.h"
#include "cpp_server/packets/s2c/RoomInfo_0D.h"
#include "cpp_server/packets/s2c/RoomPlayerIdTable_1D.h"
#include "cpp_server/packets/s2c/RoomPlayerState_16.h"
#include "cpp_server/packets/s2c/EnumGameRoom_0E.h"
#include "cpp_server/server/ClientSession.h"

namespace cpp_server::server {

[[nodiscard]] bool HasCreatedRoomContext(const ClientSession& session);
[[nodiscard]] std::uint32_t StableReconnectLoginDword(std::string_view login_id);
[[nodiscard]] packets::s2c::LoginChallenge_9F BuildLoginSuccessPacket(const core::AccountRecord& account);
[[nodiscard]] packets::s2c::RoomPlayerIdTable_1D BuildRoomPlayerIdTableFromState(
    const packets::s2c::RoomInfo_0D& room_info,
    const packets::s2c::RoomPlayerState_16& player_state);
[[nodiscard]] packets::s2c::RoomInfo_0D BuildRoomInfoFromRuntimeEntry(
    const packets::s2c::EnumGameRoom_0E& entry);

}  // namespace cpp_server::server
