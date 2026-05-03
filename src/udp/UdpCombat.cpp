#include "cpp_server/udp/UdpCombat.h"

#include <algorithm>
#include <span>
#include <string>

#include "cpp_server/udp/UdpGamePackets.h"

namespace cpp_server::udp {

namespace {

constexpr std::uint16_t kMaxSpiritualStrength = 100;
constexpr std::uint16_t kDefaultPlayerAttackDamage = 6;
constexpr std::uint16_t kPlayerSpiritGainPerHit = 1;
constexpr std::uint16_t kMinTargetSpiritGainPerHit = 4;
constexpr std::uint16_t kDefaultNpcAttackDamage = 5;
constexpr std::uint16_t kDefaultNpcSpiritGain = 3;
constexpr std::uint16_t kDeathMotionTag = 0x3c00;
constexpr std::uint32_t kComboResetTicks = 1800;
constexpr std::uint8_t kObjectiveCategory = 2;
constexpr std::uint8_t kHostileCategory = 3;

constexpr std::string_view kGrabHitMotion =
    "\xec\x9e\xa1\xea\xb8\xb0\xed\x94\xbc\xea\xb2\xa9";
constexpr std::string_view kSpecialDamageMotion =
    "\xed\x95\x84\xec\x82\xb4\xea\xb8\xb0\xeb\x8d\xb0\xeb\xaf\xb8\xec\xa7\x80";
constexpr std::string_view kUltimateAttackPrefix =
    "\xed\x95\x84\xec\x82\xb4\xea\xb3\xb5\xea\xb2\xa9";
constexpr std::string_view kUltimateStartMotion =
    "\xed\x95\x84\xec\x82\xb4\xea\xb8\xb0\xec\x8b\x9c\xec\x9e\x91";

struct ClientActionFields {
    std::uint32_t elapsed_tick{};
    std::uint16_t motion_tag{};
    std::uint16_t motion_event_field_0a{0x2e66};
    std::uint32_t motion_event_field_0c{};
    std::uint16_t motion_event_field_10{};
    std::uint16_t motion_event_field_12{1};
    std::uint8_t action_field_0f{};
    std::uint8_t action_field_10{};
};

ClientActionFields ExtractClientActionFields(std::span<const std::uint8_t> payload) {
    ClientActionFields fields;
    if (payload.size() < 24U) {
        return fields;
    }

    fields.elapsed_tick = ReadU32Le(payload.subspan(4, 4));
    fields.motion_tag = ReadU16Le(payload.subspan(11, 2));
    fields.motion_event_field_0a = ReadU16Le(payload.subspan(13, 2));
    fields.action_field_0f = payload[15];
    fields.action_field_10 = payload[16];
    fields.motion_event_field_0c = ReadU32Le(payload.subspan(17, 4));
    fields.motion_event_field_10 = ReadU16Le(payload.subspan(21, 2));
    fields.motion_event_field_12 = 1;
    return fields;
}

std::uint32_t CombatHitChainKey(std::uint16_t source_object_id) {
    return source_object_id;
}

std::uint16_t NextNativeLikeHitIndex(
    PeerState& peer_state,
    std::uint16_t source_object_id,
    std::uint16_t target_object_id,
    std::uint32_t elapsed_tick) {
    auto& chain = peer_state.combat_hit_chains[CombatHitChainKey(source_object_id)];
    const auto elapsed_delta = elapsed_tick >= chain.elapsed_tick ? elapsed_tick - chain.elapsed_tick : 0U;
    const bool reset_chain = !chain.active || elapsed_tick < chain.elapsed_tick || elapsed_delta > kComboResetTicks;

    chain.source_object_id = source_object_id;
    chain.target_object_id = target_object_id;
    chain.elapsed_tick = elapsed_tick;
    chain.hit_index = reset_chain ? 1 : static_cast<std::uint16_t>(chain.hit_index + 1U);
    chain.active = true;
    return chain.hit_index;
}

std::uint16_t AddClamped(std::uint16_t value, std::uint16_t delta, std::uint16_t maximum) {
    return static_cast<std::uint16_t>(
        std::min<std::uint32_t>(maximum, static_cast<std::uint32_t>(value) + delta));
}

std::uint16_t TargetSpiritGainForDamage(std::uint16_t damage) {
    if (damage == 0) {
        return 0;
    }
    return static_cast<std::uint16_t>(
        std::max<std::uint16_t>(kMinTargetSpiritGainPerHit, static_cast<std::uint16_t>((damage / 2U) + 1U)));
}

bool StartsWith(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

bool IsBlendMotion(std::string_view motion_name) {
    return motion_name.find("_blend") != std::string_view::npos;
}

bool ShouldMirrorClientMotion(std::string_view motion_name) {
    return !motion_name.empty() && !IsBlendMotion(motion_name);
}

std::string_view UltimateAttackSuffix(std::string_view motion_name) {
    if (!StartsWith(motion_name, kUltimateAttackPrefix)) {
        return {};
    }
    const auto suffix = motion_name.substr(kUltimateAttackPrefix.size());
    const bool numeric = !suffix.empty() &&
                         std::all_of(suffix.begin(), suffix.end(), [](char ch) {
                             return ch >= '0' && ch <= '9';
                         });
    return numeric ? suffix : std::string_view{};
}

std::string SelectLiveEventMotion(std::string_view client_motion_name, std::string_view fallback_motion) {
    if (!ShouldMirrorClientMotion(client_motion_name)) {
        return std::string(fallback_motion);
    }
    return std::string(client_motion_name);
}

std::string SelectLiveTargetMotion(std::string_view client_motion_name, std::string_view fallback_motion) {
    const auto event_motion = SelectLiveEventMotion(client_motion_name, fallback_motion);
    if (event_motion == "down1") {
        return "down1#0";
    }
    if (event_motion == kUltimateStartMotion) {
        return std::string(kSpecialDamageMotion);
    }
    if (const auto suffix = UltimateAttackSuffix(event_motion); !suffix.empty()) {
        return "damage" + std::string(suffix);
    }
    return event_motion;
}

std::uint16_t ResponseMotionTag(
    const ClientActionFields& fields,
    bool target_is_dead,
    std::string_view target_motion) {
    return target_is_dead || target_motion == "down1#0" ? kDeathMotionTag : fields.motion_tag;
}

std::uint8_t HitRelationField08(std::string_view motion_name) {
    return motion_name == kGrabHitMotion ? 1 : 0;
}

std::string EntityLogLabel(const PeerState& peer_state, std::uint16_t object_id) {
    if (object_id == peer_state.player_object_id) {
        if (const auto label = peer_state.combat_entity_labels.find(object_id);
            label != peer_state.combat_entity_labels.end()) {
            return label->second;
        }
        return "player";
    }
    if (const auto label = peer_state.combat_entity_labels.find(object_id);
        label != peer_state.combat_entity_labels.end()) {
        return label->second;
    }
    return {};
}

std::string CombatActorLogFields(
    const PeerState& peer_state,
    std::uint16_t source_object_id,
    std::uint16_t target_object_id) {
    auto fields = " source=" + std::to_string(source_object_id);
    if (const auto label = EntityLogLabel(peer_state, source_object_id); !label.empty()) {
        fields += " source_entity='" + label + "'";
    }
    fields += " target=" + std::to_string(target_object_id);
    if (const auto label = EntityLogLabel(peer_state, target_object_id); !label.empty()) {
        fields += " target_entity='" + label + "'";
    }
    return fields;
}

void AddCombatStatUpdate(
    CombatEventResult& result,
    std::uint16_t object_id,
    const CombatEntityState& state,
    std::string description) {
    result.packets.push_back(OutgoingInnerPacket{
        OutgoingChannel::Reliable,
        0x10,
        BuildCombatStatUpdatePayload(
            object_id,
            state.life_force_current,
            state.life_force_max,
            state.spiritual_strength_current,
            100,
            0),
        std::move(description)});
}

void AddNativeLikeHitPackets(
    CombatEventResult& result,
    std::uint16_t source_object_id,
    std::uint16_t target_object_id,
    std::uint16_t hit_index,
    std::uint16_t damage,
    bool target_is_dead,
    std::string_view client_motion_name,
    std::string_view fallback_motion,
    std::string_view death_target_motion,
    std::string_view death_event_motion,
    const ClientActionFields& fields,
    std::string_view description_prefix) {
    const auto target_motion =
        target_is_dead ? std::string(death_target_motion) : SelectLiveTargetMotion(client_motion_name, fallback_motion);
    const auto event_motion =
        target_is_dead ? std::string(death_event_motion) : SelectLiveEventMotion(client_motion_name, fallback_motion);
    const auto motion_tag = ResponseMotionTag(fields, target_is_dead, target_motion);

    result.packets.push_back(OutgoingInnerPacket{
        OutgoingChannel::Sequenced,
        0x1e,
        BuildCombatDamageNoticePayload(source_object_id, damage),
        std::string(description_prefix) + " opcode=0x1E damage notice"});

    result.packets.push_back(OutgoingInnerPacket{
        OutgoingChannel::Sequenced,
        0x11,
        BuildCombatHitRelationPayload(
            source_object_id,
            target_object_id,
            hit_index,
            damage,
            HitRelationField08(event_motion)),
        std::string(description_prefix) + " opcode=0x11 hit relation"});

    result.packets.push_back(OutgoingInnerPacket{
        OutgoingChannel::Unsequenced,
        0x17,
        BuildCombatTargetMotionPayload(target_object_id, motion_tag, target_motion),
        std::string(description_prefix) + " opcode=0x17 target motion object_id=" +
            std::to_string(target_object_id) + " motion_tag=" + std::to_string(motion_tag) +
            " motion='" + target_motion + "'"});

    result.packets.push_back(OutgoingInnerPacket{
        OutgoingChannel::Sequenced,
        0x08,
        BuildCombatMotionEventPayload(
            source_object_id,
            target_object_id,
            fields.elapsed_tick,
            motion_tag,
            fields.motion_event_field_0a,
            fields.motion_event_field_0c,
            fields.motion_event_field_10,
            fields.motion_event_field_12,
            event_motion),
        std::string(description_prefix) + " opcode=0x08 motion event source=" +
            std::to_string(source_object_id) + " target=" + std::to_string(target_object_id) +
            " motion_tag=" + std::to_string(motion_tag) + " motion='" + event_motion + "'"});
}

std::string NativeLikeLogFields(const ClientActionFields& fields) {
    return " elapsed_tick=" + std::to_string(fields.elapsed_tick) +
           " motion_tag=" + std::to_string(fields.motion_tag) +
           " event_field_0a=" + std::to_string(fields.motion_event_field_0a) +
           " event_field_0c=" + std::to_string(fields.motion_event_field_0c) +
           " event_field_10=" + std::to_string(fields.motion_event_field_10) +
           " action_0f_10=" + std::to_string(fields.action_field_0f) + "," +
           std::to_string(fields.action_field_10);
}

}  // namespace

CombatEventResult HandleClientCombatEvent(
    PeerState& peer_state,
    const DecodedPacket& packet,
    bool experimental_game_udp_sync) {
    CombatEventResult result;
    if (!experimental_game_udp_sync) {
        return result;
    }

    if (packet.opcode == 0x07 && packet.payload.size() >= 2U) {
        const auto payload = std::span<const std::uint8_t>(packet.payload);
        const auto object_id = ReadU16Le(payload.subspan(0, 2));
        if (object_id == peer_state.player_object_id) {
            if (peer_state.player_combat_state.life_force_current == 0 &&
                peer_state.player_combat_state.spiritual_strength_current == 0) {
                return result;
            }
            peer_state.player_combat_state.life_force_current = 0;
            peer_state.player_combat_state.spiritual_strength_current = 0;
            AddCombatStatUpdate(
                result,
                object_id,
                peer_state.player_combat_state,
                "native-like opcode=0x07 clear player life/spirit opcode=0x10");
            result.log_line =
                "[game-udp][combat][opcode-0x07-clear]" +
                CombatActorLogFields(peer_state, object_id, object_id) +
                " life_force=0/" + std::to_string(peer_state.player_combat_state.life_force_max);
            return result;
        }

        auto target = peer_state.combat_entities.find(object_id);
        if (target == peer_state.combat_entities.end()) {
            return result;
        }
        if (target->second.life_force_current == 0 && target->second.spiritual_strength_current == 0) {
            return result;
        }
        target->second.life_force_current = 0;
        target->second.spiritual_strength_current = 0;
        peer_state.dead_combat_entities.insert(object_id);
        AddCombatStatUpdate(
            result,
            object_id,
            target->second,
            "native-like opcode=0x07 clear entity life/spirit opcode=0x10 object_id=" +
                std::to_string(object_id));
        result.log_line =
            "[game-udp][combat][opcode-0x07-clear]" +
            CombatActorLogFields(peer_state, object_id, object_id) +
            " life_force=0/" + std::to_string(target->second.life_force_max);
        return result;
    }

    if (packet.payload.size() < 4U) {
        return result;
    }

    if (packet.opcode == 0x3c) {
        const auto payload = std::span<const std::uint8_t>(packet.payload);
        const auto target_object_id = ReadU16Le(payload.subspan(0, 2));
        const auto player_object_id = ReadU16Le(payload.subspan(2, 2));
        if (player_object_id != peer_state.player_object_id) {
            result.log_line =
                "[game-udp][combat][native-like-ultimate-ignored] opcode=0x3C target=" +
                std::to_string(target_object_id) + " player=" + std::to_string(player_object_id) +
                " expected_player=" + std::to_string(peer_state.player_object_id);
            return result;
        }

        peer_state.player_combat_state.spiritual_strength_current = 0;
        AddCombatStatUpdate(
            result,
            peer_state.player_object_id,
            peer_state.player_combat_state,
            "native-like combat ultimate spiritual strength spend opcode=0x10 target=" +
                std::to_string(target_object_id));
        result.log_line =
            "[game-udp][combat][native-like-ultimate] opcode=0x3C target=" +
            std::to_string(target_object_id) + " player=" + std::to_string(player_object_id) +
            " spiritual_strength=0";
        return result;
    }

    if (packet.opcode != 0x03 || packet.payload.size() < 24U) {
        return result;
    }

    const auto payload = std::span<const std::uint8_t>(packet.payload);
    const auto source_object_id = ReadU16Le(payload.subspan(0, 2));
    const auto target_object_id = ReadU16Le(payload.subspan(2, 2));
    if (target_object_id == 0xffffU) {
        return result;
    }

    const auto fields = ExtractClientActionFields(payload);
    const auto client_motion_name = DecodeUtf16Tail(payload, 24);
    if (client_motion_name.empty()) {
        return result;
    }

    if (source_object_id == peer_state.player_object_id) {
        auto target = peer_state.combat_entities.find(target_object_id);
        if (target == peer_state.combat_entities.end() || target->second.life_force_current == 0) {
            return result;
        }
        if (target->second.category != kHostileCategory) {
            result.log_line =
                "[game-udp][combat][native-like-friendly-fire-blocked]" +
                CombatActorLogFields(peer_state, source_object_id, target_object_id) +
                " target_category=" + std::to_string(target->second.category);
            return result;
        }

        const auto observed_hit_index =
            NextNativeLikeHitIndex(peer_state, source_object_id, target_object_id, fields.elapsed_tick);
        const auto hit_index = observed_hit_index;
        const auto damage = static_cast<std::uint16_t>(
            std::min<std::uint16_t>(target->second.life_force_current, kDefaultPlayerAttackDamage));

        target->second.life_force_current = static_cast<std::uint16_t>(target->second.life_force_current - damage);
        target->second.spiritual_strength_current = AddClamped(
            target->second.spiritual_strength_current,
            TargetSpiritGainForDamage(damage),
            kMaxSpiritualStrength);
        peer_state.player_combat_state.spiritual_strength_current = AddClamped(
            peer_state.player_combat_state.spiritual_strength_current,
            kPlayerSpiritGainPerHit,
            kMaxSpiritualStrength);

        const bool target_is_dead = target->second.life_force_current == 0;
        if (target_is_dead) {
            peer_state.dead_combat_entities.insert(target_object_id);
        }

        AddCombatStatUpdate(
            result,
            target_object_id,
            target->second,
            "native-like combat monster stat opcode=0x10 object_id=" + std::to_string(target_object_id));
        AddCombatStatUpdate(
            result,
            peer_state.player_object_id,
            peer_state.player_combat_state,
            "native-like combat player stat opcode=0x10");
        AddNativeLikeHitPackets(
            result,
            source_object_id,
            target_object_id,
            hit_index,
            damage,
            target_is_dead,
            client_motion_name,
            "damage1",
            "death",
            "death",
            fields,
            "mirrored combat");

        result.log_line =
            "[game-udp][combat][mirrored]" +
            CombatActorLogFields(peer_state, source_object_id, target_object_id) +
            " client_motion='" + client_motion_name +
            "' damage=" + std::to_string(damage) + " hit_index=" + std::to_string(hit_index) +
            " target_spiritual_strength=" + std::to_string(target->second.spiritual_strength_current) +
            " player_spiritual_strength=" + std::to_string(peer_state.player_combat_state.spiritual_strength_current) +
            NativeLikeLogFields(fields) + " target_motion='" +
            (target_is_dead ? std::string{"death"} : SelectLiveTargetMotion(client_motion_name, "damage1")) +
            "' event_motion='" +
            (target_is_dead ? std::string{"death"} : SelectLiveEventMotion(client_motion_name, "damage1")) +
            "' life_force=" +
            std::to_string(target->second.life_force_current) + "/" +
            std::to_string(target->second.life_force_max);
        return result;
    }

    auto source = peer_state.combat_entities.find(source_object_id);
    if (source == peer_state.combat_entities.end() || source->second.life_force_current == 0) {
        return result;
    }

    source->second.spiritual_strength_current =
        AddClamped(source->second.spiritual_strength_current, kDefaultNpcSpiritGain, kMaxSpiritualStrength);
    AddCombatStatUpdate(
        result,
        source_object_id,
        source->second,
        "native-like combat npc stat opcode=0x10 object_id=" + std::to_string(source_object_id));

    const auto hit_index =
        NextNativeLikeHitIndex(peer_state, source_object_id, target_object_id, fields.elapsed_tick);

    if (target_object_id == peer_state.player_object_id) {
        if (peer_state.player_combat_state.life_force_current == 0) {
            return result;
        }
        const auto damage = static_cast<std::uint16_t>(
            std::min<std::uint16_t>(peer_state.player_combat_state.life_force_current, kDefaultNpcAttackDamage));
        peer_state.player_combat_state.life_force_current =
            static_cast<std::uint16_t>(peer_state.player_combat_state.life_force_current - damage);
        const bool player_is_dead = peer_state.player_combat_state.life_force_current == 0;

        AddCombatStatUpdate(
            result,
            peer_state.player_object_id,
            peer_state.player_combat_state,
            "native-like combat player life opcode=0x10");
        AddNativeLikeHitPackets(
            result,
            source_object_id,
            peer_state.player_object_id,
            hit_index,
            damage,
            player_is_dead,
            client_motion_name,
            "damage3",
            "down1#0",
            "down1",
            fields,
            "mirrored combat npc-vs-player");

        result.log_line =
            "[game-udp][combat][native-like-npc]" +
            CombatActorLogFields(peer_state, source_object_id, peer_state.player_object_id) +
            " client_motion='" +
            client_motion_name + "' damage=" + std::to_string(damage) +
            " hit_index=" + std::to_string(hit_index) + NativeLikeLogFields(fields) +
            " player_life_force=" + std::to_string(peer_state.player_combat_state.life_force_current) +
            "/" + std::to_string(peer_state.player_combat_state.life_force_max);
        return result;
    }

    auto target = peer_state.combat_entities.find(target_object_id);
    if (target == peer_state.combat_entities.end() || target->second.life_force_current == 0) {
        return result;
    }
    if (target->second.category != kObjectiveCategory) {
        result.log_line =
            "[game-udp][combat][native-like-npc-friendly-fire-blocked]" +
            CombatActorLogFields(peer_state, source_object_id, target_object_id) +
            " target_category=" + std::to_string(target->second.category);
        return result;
    }

    const auto damage =
        static_cast<std::uint16_t>(std::min<std::uint16_t>(target->second.life_force_current, kDefaultNpcAttackDamage));
    target->second.life_force_current = static_cast<std::uint16_t>(target->second.life_force_current - damage);
    const bool target_is_dead = target->second.life_force_current == 0;
    if (target_is_dead) {
        peer_state.dead_combat_entities.insert(target_object_id);
    }

    AddCombatStatUpdate(
        result,
        target_object_id,
        target->second,
        "native-like combat npc target stat opcode=0x10 object_id=" + std::to_string(target_object_id));
    AddNativeLikeHitPackets(
        result,
        source_object_id,
        target_object_id,
        hit_index,
        damage,
        target_is_dead,
        client_motion_name,
        "damage3",
        "death",
        "death",
        fields,
        "mirrored combat npc-vs-objective");

    result.log_line =
        "[game-udp][combat][native-like-npc]" +
        CombatActorLogFields(peer_state, source_object_id, target_object_id) +
        " client_motion='" + client_motion_name +
        "' damage=" + std::to_string(damage) + " hit_index=" + std::to_string(hit_index) +
        NativeLikeLogFields(fields) + " target_life_force=" +
        std::to_string(target->second.life_force_current) + "/" +
        std::to_string(target->second.life_force_max);
    return result;
}

}  // namespace cpp_server::udp
