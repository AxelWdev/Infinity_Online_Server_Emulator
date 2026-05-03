#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cpp_server/core/ByteBuffer.h"
#include "cpp_server/core/GameDataCatalog.h"
#include "cpp_server/packets/s2c/EnumGameRoom_0E.h"
#include "cpp_server/packets/s2c/RoomPlayerState_16.h"

namespace cpp_server::udp {

struct GameEntity {
    std::uint32_t owner_context_id{};
    std::uint16_t entity_object_id{};
    std::uint8_t field_06{};
    std::uint8_t category{};
    std::array<std::uint8_t, 4> state_bytes{};
    std::array<std::uint8_t, 6> appearance_bytes{};
    core::MissionSpawnPosition position{};
    std::string resource_key{};
    std::string display_name{};
    std::string group_name{};
    core::ByteVector optional_tail{};
};

struct ControlRecord23 {
    std::uint16_t entity_object_id{};
    std::uint8_t slot{};
    std::uint8_t value{};
    std::uint32_t field_04{};
    std::uint32_t field_08{};
    std::uint8_t flag{};
};

struct WorldSnapshot {
    std::string scene_key{};
    std::uint16_t rule_field{};
    std::uint32_t duration_ms{};
    std::vector<ControlRecord23> control_records{};
    std::vector<GameEntity> entities{};
};

[[nodiscard]] std::uint8_t EntityCategory(std::uint16_t entity_object_id);
[[nodiscard]] std::uint32_t WorldDurationMs(const packets::s2c::EnumGameRoom_0E& room);
[[nodiscard]] bool IsNativeTrainingWorld(std::uint16_t rule_field, std::string_view scene_key);

[[nodiscard]] core::ByteVector BuildRoomHeaderPayload(
    std::uint16_t rule_field,
    std::string_view scene_key,
    std::uint8_t announced_entity_count);
[[nodiscard]] core::ByteVector BuildControl23Payload(const ControlRecord23& record);
[[nodiscard]] core::ByteVector BuildEntityPayload(const GameEntity& entity);
[[nodiscard]] core::ByteVector BuildCombatStatUpdatePayload(
    std::uint16_t object_id,
    std::uint16_t life_force_current,
    std::uint16_t life_force_max,
    std::uint16_t spiritual_strength_current,
    std::uint16_t spiritual_strength_max,
    std::uint8_t status_byte);
[[nodiscard]] core::ByteVector BuildCombatDamageNoticePayload(std::uint16_t source_object_id, std::uint16_t damage);
[[nodiscard]] core::ByteVector BuildCombatHitRelationPayload(
    std::uint16_t source_object_id,
    std::uint16_t target_object_id,
    std::uint16_t hit_index,
    std::uint16_t damage,
    std::uint8_t relation_field_08 = 0);
[[nodiscard]] core::ByteVector BuildCombatTargetMotionPayload(
    std::uint16_t target_object_id,
    std::uint16_t motion_tag,
    std::string_view motion_name);
[[nodiscard]] core::ByteVector BuildCombatMotionEventPayload(
    std::uint16_t source_object_id,
    std::uint16_t target_object_id,
    std::uint32_t elapsed_tick,
    std::uint16_t motion_tag,
    std::uint16_t motion_event_field_0a,
    std::uint32_t motion_event_field_0c,
    std::uint16_t motion_event_field_10,
    std::uint16_t motion_event_field_12,
    std::string_view motion_name);
[[nodiscard]] core::ByteVector BuildPlayerTail(
    const packets::s2c::RoomPlayerState_16& player_state,
    const std::vector<std::uint16_t>& skill_ids);
[[nodiscard]] WorldSnapshot BuildWorldSnapshot(
    const packets::s2c::EnumGameRoom_0E& room,
    const packets::s2c::RoomPlayerState_16& player_state,
    std::uint32_t owner_context_id,
    std::string scene_key,
    const std::optional<core::MissionSpawnPosition>& player_spawn_position,
    const std::vector<std::uint16_t>& player_skill_ids,
    const core::GameDataCatalog& game_data_catalog);

}  // namespace cpp_server::udp
