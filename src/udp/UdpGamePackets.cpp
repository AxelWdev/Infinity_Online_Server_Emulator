#include "cpp_server/udp/UdpGamePackets.h"

#include <algorithm>
#include <stdexcept>

#include "cpp_server/core/Utf.h"
#include "cpp_server/udp/UdpProtocol.h"

namespace cpp_server::udp {

std::uint8_t EntityCategory(std::uint16_t entity_object_id) {
    if (entity_object_id < 0x03ec) {
        return 1;
    }
    if (entity_object_id >= 0x07d4 && entity_object_id <= 0x0fa3) {
        return 2;
    }
    if (entity_object_id >= 0x138c && entity_object_id <= 0x1b5b) {
        return 3;
    }
    if (entity_object_id >= 0x1f44 && entity_object_id < 10000) {
        return 4;
    }
    return 0;
}

std::uint32_t WorldDurationMs(const packets::s2c::EnumGameRoom_0E& room) {
    if (room.limit_minutes != 0) {
        return static_cast<std::uint32_t>(room.limit_minutes) * 60U * 1000U;
    }
    return 0x00dbd63eU;
}

bool IsNativeTrainingWorld(std::uint16_t rule_field, std::string_view scene_key) {
    return rule_field == 3 || scene_key == "basilica";
}

core::ByteVector BuildRoomHeaderPayload(
    std::uint16_t rule_field,
    std::string_view scene_key,
    std::uint8_t announced_entity_count) {
    const auto encoded_name = core::EncodeUtf16Le(scene_key, true);
    if (encoded_name.size() > 250U) {
        throw std::runtime_error("UDP room header name is too long");
    }

    core::ByteVector payload;
    payload.reserve(3U + encoded_name.size());
    payload.push_back(announced_entity_count);
    AppendU16Le(payload, rule_field);
    payload.insert(payload.end(), encoded_name.begin(), encoded_name.end());
    return payload;
}

core::ByteVector BuildControl23Payload(const ControlRecord23& record) {
    core::ByteVector payload;
    payload.reserve(13U);
    AppendU16Le(payload, record.entity_object_id);
    payload.push_back(record.slot);
    payload.push_back(record.value);
    AppendU32Le(payload, record.field_04);
    AppendU32Le(payload, record.field_08);
    payload.push_back(record.flag);
    return payload;
}

core::ByteVector BuildEntityPayload(const GameEntity& entity) {
    core::ByteVector payload;
    payload.reserve(0x1eU + (entity.resource_key.size() + entity.display_name.size() + entity.group_name.size() + 3U) * 2U +
                    entity.optional_tail.size());
    AppendU32Le(payload, entity.owner_context_id);
    AppendU16Le(payload, entity.entity_object_id);
    payload.push_back(entity.field_06);
    payload.push_back(entity.category);
    payload.insert(payload.end(), entity.state_bytes.begin(), entity.state_bytes.end());
    payload.insert(payload.end(), entity.appearance_bytes.begin(), entity.appearance_bytes.end());
    AppendF32Le(payload, entity.position.x);
    AppendF32Le(payload, entity.position.y);
    AppendF32Le(payload, entity.position.z);
    AppendUtf16Z(payload, entity.resource_key);
    AppendUtf16Z(payload, entity.display_name);
    AppendUtf16Z(payload, entity.group_name);
    payload.insert(payload.end(), entity.optional_tail.begin(), entity.optional_tail.end());
    return payload;
}

core::ByteVector BuildCombatStatUpdatePayload(
    std::uint16_t object_id,
    std::uint16_t life_force_current,
    std::uint16_t life_force_max,
    std::uint16_t spiritual_strength_current,
    std::uint16_t spiritual_strength_max,
    std::uint8_t status_byte) {
    core::ByteVector payload;
    payload.reserve(11U);
    AppendU16Le(payload, object_id);
    AppendU16Le(payload, life_force_current);
    AppendU16Le(payload, life_force_max);
    AppendU16Le(payload, spiritual_strength_current);
    AppendU16Le(payload, spiritual_strength_max);
    payload.push_back(status_byte);
    return payload;
}

core::ByteVector BuildCombatDamageNoticePayload(std::uint16_t source_object_id, std::uint16_t damage) {
    core::ByteVector payload;
    payload.reserve(4U);
    AppendU16Le(payload, source_object_id);
    AppendU16Le(payload, damage);
    return payload;
}

core::ByteVector BuildCombatHitRelationPayload(
    std::uint16_t source_object_id,
    std::uint16_t target_object_id,
    std::uint16_t hit_index,
    std::uint16_t damage,
    std::uint8_t relation_field_08) {
    core::ByteVector payload;
    payload.reserve(13U);
    AppendU16Le(payload, source_object_id);
    AppendU16Le(payload, target_object_id);
    AppendU16Le(payload, hit_index);
    AppendU16Le(payload, damage);
    payload.push_back(relation_field_08);
    payload.push_back(1);
    payload.push_back(0);
    payload.push_back(0);
    payload.push_back(0);
    return payload;
}

core::ByteVector BuildCombatTargetMotionPayload(
    std::uint16_t target_object_id,
    std::uint16_t motion_tag,
    std::string_view motion_name) {
    core::ByteVector payload;
    payload.reserve(4U + (motion_name.size() + 1U) * 2U);
    AppendU16Le(payload, target_object_id);
    AppendU16Le(payload, motion_tag);
    AppendUtf16Z(payload, motion_name);
    return payload;
}

core::ByteVector BuildCombatMotionEventPayload(
    std::uint16_t source_object_id,
    std::uint16_t target_object_id,
    std::uint32_t elapsed_tick,
    std::uint16_t motion_tag,
    std::uint16_t motion_event_field_0a,
    std::uint32_t motion_event_field_0c,
    std::uint16_t motion_event_field_10,
    std::uint16_t motion_event_field_12,
    std::string_view motion_name) {
    core::ByteVector payload;
    payload.reserve(20U + (motion_name.size() + 1U) * 2U);
    AppendU16Le(payload, source_object_id);
    AppendU16Le(payload, target_object_id);
    AppendU32Le(payload, elapsed_tick);
    AppendU16Le(payload, motion_tag);
    AppendU16Le(payload, motion_event_field_0a);
    AppendU32Le(payload, motion_event_field_0c);
    AppendU16Le(payload, motion_event_field_10);
    AppendU16Le(payload, motion_event_field_12);
    AppendUtf16Z(payload, motion_name);
    return payload;
}

core::ByteVector BuildPlayerTail(
    const packets::s2c::RoomPlayerState_16& player_state,
    const std::vector<std::uint16_t>& skill_ids) {
    core::ByteVector tail;
    tail.reserve(0x20U + skill_ids.size() * 2U);
    for (const auto item_id : player_state.equipment_fields) {
        AppendU32Le(tail, item_id);
    }
    AppendU32Le(tail, 0);
    for (const auto skill_id : skill_ids) {
        AppendU16Le(tail, skill_id);
    }
    return tail;
}

WorldSnapshot BuildWorldSnapshot(
    const packets::s2c::EnumGameRoom_0E& room,
    const packets::s2c::RoomPlayerState_16& player_state,
    std::uint32_t owner_context_id,
    std::string scene_key,
    const std::optional<core::MissionSpawnPosition>& player_spawn_position,
    const std::vector<std::uint16_t>& player_skill_ids,
    const core::GameDataCatalog& game_data_catalog) {
    WorldSnapshot world;
    world.scene_key = std::move(scene_key);
    world.rule_field = room.rule_or_mission_id;
    world.duration_ms = WorldDurationMs(room);
    world.control_records.push_back(ControlRecord23{0x0040, 1, 1, 0, 0, 0});
    world.control_records.push_back(ControlRecord23{0x0040, 2, 1, 0, 0, 0});

    const core::MissionSpawnPosition spawn = player_spawn_position.value_or(core::MissionSpawnPosition{0.0F, 0.0F, 0.0F});
    const auto character_asset_key = game_data_catalog.character_asset_key_for_id(player_state.field_0d);
    if (!character_asset_key) {
        throw std::runtime_error("missing character asset key for character_id=" + std::to_string(player_state.field_0d));
    }
    if (player_state.field_0d > 0xffU) {
        throw std::runtime_error("UDP player character id does not fit in u8: " + std::to_string(player_state.field_0d));
    }
    const auto player_character_id = static_cast<std::uint8_t>(player_state.field_0d);

    GameEntity player;
    player.owner_context_id = owner_context_id;
    player.entity_object_id = 0x0040;
    player.field_06 = player_character_id;
    player.category = 1;
    player.state_bytes = {0, 0, 0, 2};
    player.position = spawn;
    player.resource_key = *character_asset_key;
    player.display_name = player_state.player_name.empty() ? std::string{"Player"} : player_state.player_name;
    player.group_name.clear();
    player.optional_tail = BuildPlayerTail(player_state, player_skill_ids);
    world.entities.push_back(std::move(player));

    if (const auto mission = game_data_catalog.find_mission(world.rule_field);
        mission && mission->scene_key == world.scene_key) {
        if (mission->time_limit_seconds != 0) {
            world.duration_ms = mission->time_limit_seconds * 1000U;
        }
        for (const auto& definition : mission->entity_definitions) {
            GameEntity entity;
            entity.owner_context_id = owner_context_id;
            entity.entity_object_id = definition.entity_object_id;
            entity.field_06 = definition.field_06;
            entity.category = definition.category.value_or(EntityCategory(definition.entity_object_id));
            entity.state_bytes = definition.state_bytes;
            entity.appearance_bytes = definition.appearance_bytes;
            entity.position = definition.position;
            entity.resource_key = definition.resource_key;
            entity.display_name = definition.display_name;
            entity.group_name = definition.group_name;
            world.entities.push_back(std::move(entity));
        }
    }

    if (IsNativeTrainingWorld(world.rule_field, world.scene_key)) {
        for (std::uint16_t index = 0; index < 10; ++index) {
            GameEntity trainer;
            trainer.owner_context_id = owner_context_id;
            trainer.entity_object_id = static_cast<std::uint16_t>(5068U + index * 8U);
            trainer.field_06 = 0;
            trainer.category = EntityCategory(trainer.entity_object_id);
            trainer.state_bytes = {0x15, 0, 0, 2};
            trainer.position = core::MissionSpawnPosition{
                spawn.x + static_cast<float>((index % 5U) * 250U + 250U),
                spawn.y,
                spawn.z + static_cast<float>((index / 5U) * 250U + 250U)};
            trainer.resource_key = "roseguard_training_key_basic";
            trainer.display_name = "TrainerGroup";
            trainer.group_name.clear();
            world.entities.push_back(std::move(trainer));
        }
    }

    return world;
}

}  // namespace cpp_server::udp
