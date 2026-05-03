#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <unordered_map>

namespace cpp_server::udp {

struct CombatEntityState {
    std::uint16_t life_force_current{};
    std::uint16_t life_force_max{};
    std::uint16_t spiritual_strength_current{};
    std::uint8_t category{};
};

struct CombatHitChainState {
    std::uint16_t source_object_id{};
    std::uint16_t target_object_id{};
    std::uint32_t elapsed_tick{};
    std::uint16_t hit_index{};
    bool active{};
};

enum class NativeSessionState : std::uint8_t {
    kInitial = 0,
    kBootstrapAccepted = 1,
    kInitialSyncSent = 2,
    kClientReady = 3,
    kElapsedSent = 4,
    kStarted = 6,
    kInactive = 7,
    kDisconnected = 8,
};

struct PeerState {
    std::uint16_t last_client_reliable_sequence_base{};
    std::uint16_t next_server_reliable_sequence_base{};
    std::uint16_t next_server_sequenced_sequence_base{};
    NativeSessionState native_session_state{NativeSessionState::kInitial};
    std::uint32_t client_handshake_token{};
    std::uint32_t client_advertised_ipv4{};
    std::uint16_t client_advertised_port{};
    std::chrono::steady_clock::time_point initial_sync_sent_at{};
    std::chrono::steady_clock::time_point last_game_state_tick_sent_at{};
    std::uint32_t active_world_duration_ms{};
    int client_id{};
    std::uint16_t active_rule_field{};
    std::string active_scene_key{};
    std::unordered_map<std::uint16_t, CombatEntityState> combat_entities{};
    std::unordered_map<std::uint16_t, std::string> combat_entity_labels{};
    std::unordered_set<std::uint16_t> dead_combat_entities{};
    CombatEntityState player_combat_state{};
    std::uint16_t player_object_id{0x0040};
    std::unordered_map<std::uint32_t, CombatHitChainState> combat_hit_chains{};
    bool has_client_handshake{};
    bool sent_initial_room_sync{};
    bool sent_start_elapsed{};
    bool sent_initial_combat_stats{};
    bool mission_result_sent{};
};

}  // namespace cpp_server::udp
