#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include "cpp_server/core/AccountDatabase.h"
#include "cpp_server/core/GameDataCatalog.h"
#include "cpp_server/core/Logger.h"
#include "cpp_server/core/OptionsStore.h"
#include "cpp_server/core/SocketPlatform.h"
#include "cpp_server/server/ClientSession.h"
#include "cpp_server/server/PacketScheduler.h"
#include "cpp_server/server/RoomRegistry.h"
#include "cpp_server/udp/UdpGameServer.h"
#include "cpp_server/udp/UdpInitialSyncContext.h"

namespace cpp_server::server {

struct ServerConfiguration {
    std::string host{"0.0.0.0"};
    std::uint16_t port{8080};
    std::filesystem::path options_path{};
    std::filesystem::path account_database_path{};
    std::optional<std::filesystem::path> debug_log_path{};
    int auto_delay_ms{250};
    int list_stream_start_delay_ms{0};
    int list_stream_step_ms{0};
    std::unordered_map<std::uint8_t, int> auto_echo_delays_ms{};
    std::uint32_t auto_enum_channel_type_id{1};
    std::uint16_t auto_enum_channel_selector_id{1};
    std::string auto_enum_ipv4{"127.0.0.1"};
    std::uint16_t auto_enum_port{8080};
    std::string auto_enum_name{"Lobby"};
    std::uint16_t game_udp_port{0};
    bool experimental_game_udp_sync{};
};

class TcpLzssServer {
public:
    explicit TcpLzssServer(ServerConfiguration config);
    ~TcpLzssServer();

    void start();
    void stop();

    void list_clients();
    void set_active(int client_id);
    void send_logical_to_active(std::span<const std::uint8_t> packet);
    void send_frame_to_active(std::uint8_t opcode, std::span<const std::uint8_t> payload);
    void send_raw_lzss_to_active(std::span<const std::uint8_t> packet);
    void send_enumchannel_ex_to_active(
        std::uint32_t channel_type_id,
        std::uint16_t channel_selector_id,
        std::uint32_t packed_ipv4,
        std::uint16_t tcp_port,
        std::string name);
    void send_enumdone_to_active();
    void close_active();

    [[nodiscard]] core::Logger& logger() { return logger_; }
    [[nodiscard]] std::string debug_log_path_text() const { return logger_.debug_log_path_text(); }

private:
    // Session and option access.
    [[nodiscard]] ClientSessionPtr active_session();
    [[nodiscard]] core::ServerOptions options();
    [[nodiscard]] core::ServerOptions effective_options_for_session(const ClientSessionPtr& session);

    // Room and account projection.
    [[nodiscard]] std::vector<packets::s2c::EnumGameRoom_0E> created_room_entries();
    [[nodiscard]] std::optional<packets::s2c::EnumGameRoom_0E> runtime_room_entry_for_session(
        const ClientSessionPtr& session);
    [[nodiscard]] packets::s2c::RoomInfo_0D room_info_for_session(
        const ClientSessionPtr& session,
        const core::ServerOptions& effective_options);
    [[nodiscard]] std::string character_list_source_for_session(const ClientSessionPtr& session);
    [[nodiscard]] std::string item_list_source_for_session(const ClientSessionPtr& session);
    [[nodiscard]] std::vector<packets::s2c::FullRoomStateSlot_85> room_slot_entries_for_session(
        const ClientSessionPtr& session,
        const core::ServerOptions& effective_options);
    [[nodiscard]] std::vector<packets::s2c::StateUpdate_9E> room_user_state_entries_for_session(
        const ClientSessionPtr& session,
        const core::ServerOptions& effective_options);
    [[nodiscard]] std::optional<packets::s2c::RoomPlayerState_16> room_player_state_for_session(
        const ClientSessionPtr& session);
    [[nodiscard]] std::optional<udp::InitialSyncContext> game_udp_initial_sync_context();
    void send_mission_result_from_udp(udp::MissionResultEvent event);

    // Connection lifecycle.
    void accept_loop();
    void client_loop(ClientSessionPtr session);
    void drain_logical_packets(ClientSessionPtr session);

    // Runtime room registry state.
    void upsert_created_room_from_session(const ClientSessionPtr& session);
    void clear_created_room_state_for_session(const ClientSessionPtr& session);
    void remove_created_rooms_for_session(const ClientSessionPtr& session);

    // Dispatch and observed client state.
    [[nodiscard]] bool maybe_schedule_auto_action(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void capture_dynamic_room_state(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void note_packet_context(ClientSessionPtr session, std::uint8_t opcode);
    [[nodiscard]] bool is_full_room_state_upload_active(const ClientSessionPtr& session);

    // Domain packet handlers.
    void handle_buy_item(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void handle_buy_skill(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void handle_connect_lobby(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void handle_assign_quickslot(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void handle_equip_item(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void handle_remove_item(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void handle_login_challenge(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void handle_character_name(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void handle_state_update_upload(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void send_shared_error(
        ClientSessionPtr session,
        std::uint16_t selector_opcode,
        std::uint16_t error_code,
        std::string description);
    [[nodiscard]] ClientSessionPtr find_authenticated_session_by_login(
        std::string_view login_id,
        const ClientSession* exclude_session = nullptr);

    // Scheduled automatic replies.
    void start_delayed_action(ClientSessionPtr session, std::string description, int delay_ms, std::function<void()> action);
    void auto_echo_frame(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void auto_send_empty_completion(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void auto_send_paired_completion(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void auto_send_account_info(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void auto_send_full_room_state_commit(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void handle_room_leave_request(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void auto_send_room_create_completion(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void auto_send_room_character_select_state(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void handle_room_start_request(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void auto_send_room_info(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void auto_send_item_list(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void auto_send_character_list(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void auto_send_skill_list(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void auto_send_guard_list(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void auto_send_quickslot_list(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void auto_send_room_list(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void auto_send_packet_52_list(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void handle_observed_no_response_packet(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void auto_send_hakan_training_info(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void auto_send_training_start_guard_state(ClientSessionPtr session, const core::LogicalPacketFrame& frame);
    void auto_send_enumchannel_reply(ClientSessionPtr session, const core::LogicalPacketFrame& frame);

    // Stream scheduling.
    [[nodiscard]] int schedule_packet_stream(
        ClientSessionPtr session,
        const std::vector<core::LogicalPacketFrame>& packets,
        std::string description_prefix,
        std::optional<int> base_delay_ms = std::nullopt);
    void schedule_stream_with_commit(
        ClientSessionPtr session,
        const std::vector<core::LogicalPacketFrame>& packets,
        std::string description_prefix,
        const core::LogicalPacketFrame& commit_packet,
        std::string commit_description,
        std::optional<int> base_delay_ms = std::nullopt);

    ServerConfiguration config_{};
    core::SocketRuntime socket_runtime_{};
    core::SocketHandle listener_{core::kInvalidSocket};
    std::atomic<bool> running_{false};

    mutable std::mutex sessions_mutex_{};
    std::map<int, ClientSessionPtr> sessions_{};
    int next_client_id_{1};
    std::optional<int> active_client_id_{};

    std::mutex client_threads_mutex_{};
    std::thread accept_thread_{};
    std::vector<std::thread> client_threads_{};
    PacketScheduler packet_scheduler_{};
    RoomRegistry room_registry_{};
    std::atomic<int> game_udp_pending_client_id_{0};
    udp::GameServer game_udp_server_{};

    core::Logger logger_{};
    core::OptionsStore options_store_;
    core::AccountDatabase account_database_;
    core::GameDataCatalog game_data_catalog_;
    std::uint32_t auto_enum_packed_ipv4_{};
};

}  // namespace cpp_server::server
