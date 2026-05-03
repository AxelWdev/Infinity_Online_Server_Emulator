#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "cpp_server/core/GameDataCatalog.h"
#include "cpp_server/packets/s2c/EnumGameRoom_0E.h"
#include "cpp_server/packets/s2c/RoomPlayerState_16.h"

namespace cpp_server::udp {

struct InitialSyncContext {
    int client_id{};
    packets::s2c::EnumGameRoom_0E room{};
    packets::s2c::RoomPlayerState_16 player_state{};
    std::string scene_key{};
    std::optional<core::MissionSpawnPosition> player_spawn_position{};
    std::vector<std::uint16_t> player_skill_ids{};
};

using InitialSyncProvider = std::function<std::optional<InitialSyncContext>()>;

struct MissionResultEvent {
    int client_id{};
    std::uint16_t rule_field{};
    std::string scene_key{};
    std::uint8_t next_mission{};
    std::uint8_t mission_result{};
    int delay_ms{};
    std::string reason{};
};

using MissionResultCallback = std::function<void(MissionResultEvent)>;

}  // namespace cpp_server::udp
