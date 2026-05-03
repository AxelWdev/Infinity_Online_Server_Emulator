#include "cpp_server/udp/UdpGameServer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "cpp_server/core/PacketLog.h"
#include "cpp_server/udp/UdpCombat.h"
#include "cpp_server/udp/UdpGamePackets.h"
#include "cpp_server/udp/UdpProtocol.h"

namespace cpp_server::udp {

namespace {

constexpr std::uint16_t kMission06RuleField = 10005;
constexpr std::uint8_t kObjectiveCategory = 2;
constexpr std::uint8_t kMissionResultFailure = 0;
constexpr std::uint8_t kMissionResultSuccess = 1;
constexpr std::uint8_t kNoNextMission = 0;
constexpr int kMissionResultSuccessDelayMs = 500;
constexpr int kMissionResultFailureDelayMs = 2000;

std::string_view NativeSessionStateName(NativeSessionState state) {
    switch (state) {
    case NativeSessionState::kInitial: return "initial";
    case NativeSessionState::kBootstrapAccepted: return "bootstrap-accepted";
    case NativeSessionState::kInitialSyncSent: return "initial-sync-sent";
    case NativeSessionState::kClientReady: return "client-ready";
    case NativeSessionState::kElapsedSent: return "elapsed-sent";
    case NativeSessionState::kStarted: return "started";
    case NativeSessionState::kInactive: return "inactive";
    case NativeSessionState::kDisconnected: return "disconnected";
    }
    return "unknown";
}

std::uint8_t NativeSessionStateValue(NativeSessionState state) {
    return static_cast<std::uint8_t>(state);
}

std::string DescribeOutgoingInnerPacket(
    std::string_view channel,
    std::uint8_t opcode,
    std::span<const std::uint8_t> payload,
    std::string_view description) {
    const auto decoded = DescribeInnerPayload(opcode, payload);
    std::string summary = std::string(description) + " channel=" + std::string(channel) +
                          " opcode=0x" + core::OpcodeHex(opcode) +
                          " payload_len=" + std::to_string(payload.size()) + decoded;
    if (decoded.empty()) {
        summary += " payload_hex=" + core::HexBytes(payload);
    }
    return summary;
}

bool IsMission06(const PeerState& peer_state) {
    return peer_state.active_rule_field == kMission06RuleField || peer_state.active_scene_key == "mission_0_6";
}

bool HasActiveMissionContext(const PeerState& peer_state) {
    return peer_state.active_rule_field != 0 || !peer_state.active_scene_key.empty();
}

std::string EntityResultLabel(const PeerState& peer_state, std::uint16_t object_id) {
    if (const auto it = peer_state.combat_entity_labels.find(object_id); it != peer_state.combat_entity_labels.end()) {
        return it->second;
    }
    if (object_id == peer_state.player_object_id) {
        return "player";
    }
    return "object_id=" + std::to_string(object_id);
}

}  // namespace

GameServer::~GameServer() {
    stop();
}

void GameServer::start(
    GameServerConfig config,
    core::Logger& logger,
    const core::GameDataCatalog& game_data_catalog,
    InitialSyncProvider initial_sync_provider,
    MissionResultCallback mission_result_callback) {
    if (running_.exchange(true)) {
        return;
    }

    config_ = std::move(config);
    logger_ = &logger;
    game_data_catalog_ = &game_data_catalog;
    initial_sync_provider_ = std::move(initial_sync_provider);
    mission_result_callback_ = std::move(mission_result_callback);

    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (!core::IsValidSocket(socket_)) {
        running_ = false;
        throw std::runtime_error("failed to create game UDP socket: " + core::LastSocketErrorText());
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(config_.port);
    if (inet_pton(AF_INET, config_.host.c_str(), &address.sin_addr) != 1) {
        running_ = false;
        core::CloseSocket(socket_);
        socket_ = core::kInvalidSocket;
        throw std::runtime_error("invalid game UDP bind address: " + config_.host);
    }

    if (bind(socket_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
        running_ = false;
        core::CloseSocket(socket_);
        socket_ = core::kInvalidSocket;
        throw std::runtime_error("game UDP bind failed: " + core::LastSocketErrorText());
    }

    sockaddr_in bound_address{};
#ifdef _WIN32
    int bound_address_len = sizeof(bound_address);
#else
    socklen_t bound_address_len = sizeof(bound_address);
#endif
    if (getsockname(socket_, reinterpret_cast<sockaddr*>(&bound_address), &bound_address_len) != 0) {
        running_ = false;
        core::CloseSocket(socket_);
        socket_ = core::kInvalidSocket;
        throw std::runtime_error("game UDP getsockname failed: " + core::LastSocketErrorText());
    }

    config_.port = ntohs(bound_address.sin_port);
    logger_->log("[game-udp] listening on " + config_.host + ":" + std::to_string(config_.port));
    thread_ = std::thread(&GameServer::loop, this);
}

void GameServer::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    core::CloseSocket(socket_);
    socket_ = core::kInvalidSocket;

    if (thread_.joinable()) {
        thread_.join();
    }

    peers_.clear();
}

std::uint16_t GameServer::bound_port() const {
    return config_.port == 0 ? config_.fallback_port : config_.port;
}

void GameServer::loop() {
    std::array<std::uint8_t, 4096> buffer{};
    while (running_) {
        sockaddr_in sender{};
#ifdef _WIN32
        int sender_len = sizeof(sender);
        const int received = recvfrom(
            socket_,
            reinterpret_cast<char*>(buffer.data()),
            static_cast<int>(buffer.size()),
            0,
            reinterpret_cast<sockaddr*>(&sender),
            &sender_len);
#else
        socklen_t sender_len = sizeof(sender);
        const auto received = recvfrom(
            socket_,
            buffer.data(),
            buffer.size(),
            0,
            reinterpret_cast<sockaddr*>(&sender),
            &sender_len);
#endif
        if (received <= 0) {
            if (running_) {
                logger_->log("[game-udp] recvfrom failed: " + core::LastSocketErrorText());
            }
            continue;
        }

        char sender_ip[INET_ADDRSTRLEN]{};
        if (inet_ntop(AF_INET, &sender.sin_addr, sender_ip, sizeof(sender_ip)) == nullptr) {
            std::snprintf(sender_ip, sizeof(sender_ip), "unknown");
        }
        auto& peer_state = peers_[EndpointKey(sender)];
        const auto payload = std::span<const std::uint8_t>(
            buffer.data(),
            static_cast<std::size_t>(received));
        logger_->log("[game-udp][recv] from=" + std::string(sender_ip) + ":" + std::to_string(ntohs(sender.sin_port)) +
                     " transport_len=" + std::to_string(received));

        if (!TransportChecksumOk(payload)) {
            logger_->log("[game-udp][drop] from=" + std::string(sender_ip) + ":" +
                         std::to_string(ntohs(sender.sin_port)) + " reason=transport checksum mismatch");
            continue;
        }

        const auto transport_word = ReadU16Le(payload);
        const auto transport_kind = static_cast<std::uint8_t>(transport_word & 7U);
        const auto sequence_base = static_cast<std::uint16_t>(transport_word & 0xfff8U);
        logger_->log("[game-udp][frame] from=" + std::string(sender_ip) + ":" +
                     std::to_string(ntohs(sender.sin_port)) + " kind=" + std::to_string(transport_kind) +
                     " sequence_base=" + std::to_string(sequence_base));

        auto send_udp_reply = [&](const core::ByteVector& reply, std::string_view description) {
#ifdef _WIN32
            const int sent = sendto(
                socket_,
                reinterpret_cast<const char*>(reply.data()),
                static_cast<int>(reply.size()),
                0,
                reinterpret_cast<const sockaddr*>(&sender),
                sizeof(sender));
#else
            const auto sent = sendto(
                socket_,
                reply.data(),
                reply.size(),
                0,
                reinterpret_cast<const sockaddr*>(&sender),
                sizeof(sender));
#endif
            if (sent <= 0) {
                logger_->log("[game-udp][send-failed] to=" + std::string(sender_ip) + ":" +
                             std::to_string(ntohs(sender.sin_port)) + " " + std::string(description) +
                             " error=" + core::LastSocketErrorText());
                return;
            }
            logger_->log("[game-udp][send] to=" + std::string(sender_ip) + ":" +
                          std::to_string(ntohs(sender.sin_port)) + " " + std::string(description) +
                          " transport_len=" + std::to_string(reply.size()));
        };

        auto send_udp_reliable_inner = [&](PeerState& peer,
                                           std::uint8_t inner_opcode,
                                           std::span<const std::uint8_t> inner_payload,
                                           std::string_view description) {
            try {
                const auto inner = EncodeInnerPacket(inner_opcode, inner_payload);
                const auto frame = BuildReliableFrame(peer.next_server_reliable_sequence_base, inner);
                send_udp_reply(frame, DescribeOutgoingInnerPacket("reliable", inner_opcode, inner_payload, description));
                peer.next_server_reliable_sequence_base =
                    static_cast<std::uint16_t>((peer.next_server_reliable_sequence_base + 8U) & 0xfff8U);
            } catch (const std::exception& ex) {
                logger_->log("[game-udp][send-failed] to=" + std::string(sender_ip) + ":" +
                             std::to_string(ntohs(sender.sin_port)) + " " + std::string(description) +
                             " error=" + ex.what());
            }
        };

        auto send_udp_sequenced_inner = [&](PeerState& peer,
                                            std::uint8_t inner_opcode,
                                            std::span<const std::uint8_t> inner_payload,
                                            std::string_view description) {
            try {
                const auto inner = EncodeInnerPacket(inner_opcode, inner_payload);
                const auto frame = BuildSequencedFrame(peer.next_server_sequenced_sequence_base, inner);
                send_udp_reply(frame, DescribeOutgoingInnerPacket("sequenced", inner_opcode, inner_payload, description));
                peer.next_server_sequenced_sequence_base =
                    static_cast<std::uint16_t>((peer.next_server_sequenced_sequence_base + 8U) & 0xfff8U);
            } catch (const std::exception& ex) {
                logger_->log("[game-udp][send-failed] to=" + std::string(sender_ip) + ":" +
                             std::to_string(ntohs(sender.sin_port)) + " " + std::string(description) +
                             " error=" + ex.what());
            }
        };

        auto send_udp_unsequenced_inner = [&](std::uint8_t inner_opcode,
                                              std::span<const std::uint8_t> inner_payload,
                                              std::string_view description) {
            try {
                const auto inner = EncodeInnerPacket(inner_opcode, inner_payload);
                const auto frame = BuildUnsequencedFrame(inner);
                send_udp_reply(frame, DescribeOutgoingInnerPacket("unsequenced", inner_opcode, inner_payload, description));
            } catch (const std::exception& ex) {
                logger_->log("[game-udp][send-failed] to=" + std::string(sender_ip) + ":" +
                             std::to_string(ntohs(sender.sin_port)) + " " + std::string(description) +
                             " error=" + ex.what());
            }
        };

        auto game_elapsed_ms = [&]() -> std::uint32_t {
            if (peer_state.initial_sync_sent_at.time_since_epoch().count() == 0) {
                return 0;
            }
            const auto now = std::chrono::steady_clock::now();
            return static_cast<std::uint32_t>(std::min<std::int64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(now - peer_state.initial_sync_sent_at).count(),
                0xffffffffLL));
        };

        auto send_combat_stat_update = [&](std::uint16_t object_id,
                                           const CombatEntityState& state,
                                           std::string_view description) {
            const auto stat_payload = BuildCombatStatUpdatePayload(
                object_id,
                state.life_force_current,
                state.life_force_max,
                state.spiritual_strength_current,
                100,
                0);
            send_udp_reliable_inner(peer_state, 0x10, stat_payload, description);
        };

        auto send_initial_combat_stats = [&]() {
            if (peer_state.sent_initial_combat_stats) {
                return;
            }
            send_combat_stat_update(
                peer_state.player_object_id,
                peer_state.player_combat_state,
                "experimental combat initial player 0x10");
            for (const auto& [object_id, state] : peer_state.combat_entities) {
                send_combat_stat_update(
                    object_id,
                    state,
                    "experimental combat initial entity 0x10 object_id=" + std::to_string(object_id) +
                        " category=" + std::to_string(state.category));
            }
            peer_state.sent_initial_combat_stats = true;
        };

        auto send_combat_result = [&](const CombatEventResult& combat_result) {
            for (const auto& outgoing : combat_result.packets) {
                switch (outgoing.channel) {
                case OutgoingChannel::Reliable:
                    send_udp_reliable_inner(peer_state, outgoing.opcode, outgoing.payload, outgoing.description);
                    break;
                case OutgoingChannel::Sequenced:
                    send_udp_sequenced_inner(peer_state, outgoing.opcode, outgoing.payload, outgoing.description);
                    break;
                case OutgoingChannel::Unsequenced:
                    send_udp_unsequenced_inner(outgoing.opcode, outgoing.payload, outgoing.description);
                    break;
                }
            }
            if (combat_result.log_line) {
                logger_->log(*combat_result.log_line);
            }
        };

        auto queue_mission_result = [&](std::uint8_t mission_result, std::string reason, int delay_ms) {
            if (peer_state.mission_result_sent) {
                return;
            }

            peer_state.mission_result_sent = true;
            const auto result_text = mission_result == kMissionResultSuccess ? "success" : "failure";
            logger_->log("[game-udp][mission-result] client=" + std::to_string(peer_state.client_id) +
                         " rule=" + std::to_string(peer_state.active_rule_field) +
                         " scene='" + peer_state.active_scene_key + "' result=" + result_text +
                         " reason='" + reason + "' scheduling tcp opcode=0x29 delay_ms=" +
                         std::to_string(delay_ms));
            if (!mission_result_callback_) {
                logger_->log("[game-udp][mission-result] no TCP mission-result callback is registered");
                return;
            }

            MissionResultEvent event;
            event.client_id = peer_state.client_id;
            event.rule_field = peer_state.active_rule_field;
            event.scene_key = peer_state.active_scene_key;
            event.next_mission = kNoNextMission;
            event.mission_result = mission_result;
            event.delay_ms = delay_ms;
            event.reason = std::move(reason);
            mission_result_callback_(std::move(event));
        };

        auto maybe_queue_terminal_mission_result = [&](std::string_view trigger, bool allow_time_success) {
            if (!config_.experimental_gameplay_sync || peer_state.mission_result_sent || !HasActiveMissionContext(peer_state)) {
                return;
            }
            if (peer_state.client_id <= 0) {
                logger_->log("[game-udp][mission-result] terminal check skipped; UDP peer has no TCP client id");
                return;
            }
            if (!IsMission06(peer_state)) {
                logger_->log("[game-udp][mission-result] terminal check using active non-06 mission context rule=" +
                             std::to_string(peer_state.active_rule_field) + " scene='" +
                             peer_state.active_scene_key + "'");
            }

            if (allow_time_success && peer_state.active_world_duration_ms != 0 &&
                peer_state.initial_sync_sent_at.time_since_epoch().count() != 0) {
                const auto elapsed_ms = game_elapsed_ms();
                if (elapsed_ms >= peer_state.active_world_duration_ms) {
                    queue_mission_result(
                        kMissionResultSuccess,
                        std::string(trigger) + ": mission timer elapsed elapsed_ms=" + std::to_string(elapsed_ms) +
                            " duration_ms=" + std::to_string(peer_state.active_world_duration_ms),
                        kMissionResultSuccessDelayMs);
                    return;
                }
            }

            if (peer_state.player_combat_state.life_force_max != 0 &&
                peer_state.player_combat_state.life_force_current == 0) {
                queue_mission_result(
                    kMissionResultFailure,
                    std::string(trigger) + ": " + EntityResultLabel(peer_state, peer_state.player_object_id) +
                        " life_force=0",
                    kMissionResultFailureDelayMs);
                return;
            }

            for (const auto& [object_id, state] : peer_state.combat_entities) {
                if (state.category == kObjectiveCategory && state.life_force_max != 0 && state.life_force_current == 0) {
                    queue_mission_result(
                        kMissionResultFailure,
                        std::string(trigger) + ": objective " + EntityResultLabel(peer_state, object_id) +
                            " life_force=0",
                        kMissionResultFailureDelayMs);
                    return;
                }
            }
        };

        auto handle_client_combat_event = [&](const DecodedPacket& packet) {
            send_combat_result(HandleClientCombatEvent(peer_state, packet, config_.experimental_gameplay_sync));
            maybe_queue_terminal_mission_result("combat opcode=0x" + core::OpcodeHex(packet.opcode), false);
        };

        auto maybe_send_game_state_tick = [&]() {
            if (!config_.experimental_gameplay_sync || !peer_state.sent_start_elapsed || peer_state.mission_result_sent) {
                return;
            }

            const auto now = std::chrono::steady_clock::now();
            if (peer_state.last_game_state_tick_sent_at.time_since_epoch().count() != 0 &&
                now - peer_state.last_game_state_tick_sent_at < std::chrono::seconds(3)) {
                return;
            }

            const auto elapsed_ms = game_elapsed_ms();
            const auto remaining_ms = peer_state.active_world_duration_ms > elapsed_ms
                                          ? peer_state.active_world_duration_ms - elapsed_ms
                                          : 0U;
            core::ByteVector tick_payload;
            AppendU32Le(tick_payload, elapsed_ms);
            AppendU32Le(tick_payload, remaining_ms);
            send_udp_unsequenced_inner(
                0x07,
                tick_payload,
                "experimental native-style opcode=0x07 room game-state tick elapsed_ms=" +
                    std::to_string(elapsed_ms) + " remaining_ms=" + std::to_string(remaining_ms));
            peer_state.last_game_state_tick_sent_at = now;
            maybe_queue_terminal_mission_result("timer", remaining_ms == 0 && elapsed_ms >= peer_state.active_world_duration_ms);
        };

        bool should_send_initial_room_sync = false;
        bool should_send_start_elapsed = false;
        if (transport_kind == 0) {
            const auto decoded_packets = DecodeInnerStream(payload.subspan(3));
            logger_->log("[game-udp][unsequenced] from=" + std::string(sender_ip) + ":" +
                         std::to_string(ntohs(sender.sin_port)) +
                         DescribeInnerStreamDecode(payload.subspan(3)));
            if (config_.experimental_gameplay_sync) {
                for (const auto& decoded_packet : decoded_packets) {
                    if (decoded_packet.opcode == 0x02 && decoded_packet.payload.size() == 4U) {
                        const auto request_value =
                            ReadU32Le(std::span<const std::uint8_t>(decoded_packet.payload));
                        core::ByteVector current_packet_payload;
                        AppendU32Le(current_packet_payload, 0);
                        send_udp_unsequenced_inner(
                            0x06,
                            current_packet_payload,
                            "experimental native-style opcode=0x06 current-packet reply to client opcode=0x02 "
                            "request_value=" +
                                std::to_string(request_value) + " reply_value=0");
                    }
                }
            }
        } else if (transport_kind == 1) {
            const auto decoded_packets = DecodeInnerStream(payload.subspan(3));
            logger_->log("[game-udp][sequenced-kind-1] from=" + std::string(sender_ip) + ":" +
                         std::to_string(ntohs(sender.sin_port)) +
                         " sequence_base=" + std::to_string(sequence_base) +
                         DescribeInnerStreamDecode(payload.subspan(3)));
            for (const auto& decoded_packet : decoded_packets) {
                handle_client_combat_event(decoded_packet);
            }
            send_udp_reply(BuildAckList(sequence_base, 4), "sequenced-kind-1 ack-list kind=4");
        } else if (transport_kind == 2) {
            if (const auto decoded = DecodeInnerPacket(payload.subspan(3))) {
                peer_state.last_client_reliable_sequence_base = sequence_base;
                if (!decoded->checksum_ok) {
                    logger_->log("[game-udp][session] from=" + std::string(sender_ip) + ":" +
                                 std::to_string(ntohs(sender.sin_port)) +
                                 " ignoring opcode=0x" + core::OpcodeHex(decoded->opcode) +
                                 " for native session state because inner checksum failed");
                } else if (decoded->opcode == 0x00 && decoded->payload.size() == 10) {
                    peer_state.client_handshake_token =
                        ReadU32Le(std::span<const std::uint8_t>(decoded->payload).subspan(0, 4));
                    peer_state.client_advertised_ipv4 =
                        ReadU32Le(std::span<const std::uint8_t>(decoded->payload).subspan(4, 4));
                    peer_state.client_advertised_port =
                        ReadU16Le(std::span<const std::uint8_t>(decoded->payload).subspan(8, 2));
                    peer_state.has_client_handshake = true;
                    peer_state.native_session_state = NativeSessionState::kBootstrapAccepted;
                    should_send_initial_room_sync = !peer_state.sent_initial_room_sync;
                } else if (decoded->opcode == 0x01 && decoded->payload.empty()) {
                    peer_state.native_session_state = NativeSessionState::kClientReady;
                    should_send_start_elapsed =
                        config_.experimental_gameplay_sync &&
                        peer_state.sent_initial_room_sync &&
                        !peer_state.sent_start_elapsed;
                } else if (decoded->opcode == 0x02) {
                    logger_->log("[game-udp][session] from=" + std::string(sender_ip) + ":" +
                                 std::to_string(ntohs(sender.sin_port)) +
                                 " opcode=0x02 native current-packet resend request state=" +
                                 std::string(NativeSessionStateName(peer_state.native_session_state)) +
                                 "(" + std::to_string(NativeSessionStateValue(peer_state.native_session_state)) +
                                 "); no current-packet replay buffer implemented yet");
                } else if (decoded->opcode == 0x0a) {
                    peer_state.native_session_state = NativeSessionState::kStarted;
                    logger_->log("[game-udp][session] from=" + std::string(sender_ip) + ":" +
                                 std::to_string(ntohs(sender.sin_port)) +
                                 " opcode=0x0A marked native session state=" +
                                 std::string(NativeSessionStateName(peer_state.native_session_state)) +
                                 "(" + std::to_string(NativeSessionStateValue(peer_state.native_session_state)) + ")");
                }
                const auto decoded_description = DescribeInnerPacket(*decoded);
                logger_->log("[game-udp][decoded] from=" + std::string(sender_ip) + ":" +
                             std::to_string(ntohs(sender.sin_port)) + " opcode=0x" +
                             core::OpcodeHex(decoded->opcode) + " payload_len=" +
                             std::to_string(decoded->payload.size()) + " checksum_ok=" +
                             std::string(decoded->checksum_ok ? "true" : "false") +
                             " native_session_state=" + std::string(NativeSessionStateName(peer_state.native_session_state)) +
                             "(" + std::to_string(NativeSessionStateValue(peer_state.native_session_state)) + ")" +
                             decoded_description +
                             (decoded_description.empty() || !decoded->checksum_ok
                                  ? " payload_hex=" + core::HexBytes(decoded->payload)
                                  : ""));
                if (decoded->checksum_ok) {
                    handle_client_combat_event(*decoded);
                }
            } else {
                logger_->log("[game-udp][decoded] from=" + std::string(sender_ip) + ":" +
                             std::to_string(ntohs(sender.sin_port)) + " reason=inner packet decode failed");
            }

            send_udp_reply(BuildAckList(sequence_base, 5), "reliable-kind-2 ack-list kind=5");
            if (should_send_initial_room_sync) {
                if (config_.experimental_gameplay_sync) {
                    if (initial_sync_provider_) {
                        if (const auto context = initial_sync_provider_()) {
                            try {
                                const auto world = BuildWorldSnapshot(
                                    context->room,
                                    context->player_state,
                                    peer_state.client_handshake_token != 0 ? peer_state.client_handshake_token
                                                                            : context->player_state.player_id,
                                    context->scene_key,
                                    context->player_spawn_position,
                                    context->player_skill_ids,
                                    *game_data_catalog_);
                                const auto announced_entities = static_cast<std::uint8_t>(
                                    std::min<std::size_t>(world.entities.size(), 0xffU));
                                const auto room_header =
                                    BuildRoomHeaderPayload(world.rule_field, world.scene_key, announced_entities);
                                peer_state.client_id = context->client_id;
                                peer_state.active_rule_field = world.rule_field;
                                peer_state.active_scene_key = world.scene_key;
                                peer_state.mission_result_sent = false;

                                logger_->log("[game-udp][sync][experimental] client=" +
                                             std::to_string(context->client_id) +
                                             " sending native-order initial sync room_id=" +
                                             std::to_string(context->room.room_id) +
                                             " udp_rule_field=" + std::to_string(world.rule_field) +
                                             " scene_key='" + world.scene_key + "' player_id=" +
                                             std::to_string(context->player_state.player_id) +
                                             " character_id=" + std::to_string(context->player_state.field_0d) +
                                             " entity_count=" + std::to_string(world.entities.size()) +
                                             " control_0x23_count=" + std::to_string(world.control_records.size()) +
                                             " player_skill_count=" + std::to_string(context->player_skill_ids.size()) +
                                             " duration_ms=" + std::to_string(world.duration_ms) +
                                             " spawn=(" +
                                             std::to_string(
                                                 context->player_spawn_position ? context->player_spawn_position->x : 0.0F) +
                                             "," +
                                             std::to_string(
                                                 context->player_spawn_position ? context->player_spawn_position->y : 0.0F) +
                                             "," +
                                             std::to_string(
                                                 context->player_spawn_position ? context->player_spawn_position->z : 0.0F) +
                                             ")");
                                send_udp_reliable_inner(
                                    peer_state,
                                    0x02,
                                    room_header,
                                    "experimental initial-sync opcode=0x02 room scene header");
                                for (std::size_t index = 0; index < world.control_records.size(); ++index) {
                                    const auto control_payload = BuildControl23Payload(world.control_records[index]);
                                    send_udp_reliable_inner(
                                        peer_state,
                                        0x23,
                                        control_payload,
                                        "experimental initial-sync opcode=0x23 native training control record index=" +
                                            std::to_string(index));
                                }
                                for (std::size_t index = 0; index < world.entities.size(); ++index) {
                                    const auto entity_payload = BuildEntityPayload(world.entities[index]);
                                    send_udp_reliable_inner(
                                        peer_state,
                                        0x03,
                                        entity_payload,
                                        "experimental initial-sync opcode=0x03 entity index=" +
                                            std::to_string(index) + " entity_object_id=" +
                                            std::to_string(world.entities[index].entity_object_id) + " resource_key='" +
                                            world.entities[index].resource_key + "'");
                                }
                                peer_state.combat_entities.clear();
                                peer_state.combat_entity_labels.clear();
                                peer_state.dead_combat_entities.clear();
                                peer_state.player_object_id =
                                    world.entities.empty() ? 0x0040 : world.entities.front().entity_object_id;
                                peer_state.player_combat_state = CombatEntityState{600, 600, 0, 1};
                                peer_state.combat_hit_chains.clear();
                                peer_state.sent_initial_combat_stats = false;
                                for (const auto& entity : world.entities) {
                                    std::string label = entity.resource_key;
                                    if (!entity.display_name.empty()) {
                                        label += label.empty() ? entity.display_name : ":" + entity.display_name;
                                    }
                                    if (!entity.group_name.empty()) {
                                        label += label.empty() ? entity.group_name : ":" + entity.group_name;
                                    }
                                    if (!label.empty()) {
                                        peer_state.combat_entity_labels[entity.entity_object_id] = std::move(label);
                                    }
                                    if (entity.category == 2) {
                                        peer_state.combat_entities[entity.entity_object_id] =
                                            CombatEntityState{1200, 1200, 0, entity.category};
                                    } else if (entity.category == 3) {
                                        peer_state.combat_entities[entity.entity_object_id] =
                                            CombatEntityState{400, 400, 0, entity.category};
                                    }
                                }
                                send_udp_reliable_inner(
                                    peer_state,
                                    0x04,
                                    {},
                                    "experimental initial-sync opcode=0x04 completion");
                                peer_state.initial_sync_sent_at = std::chrono::steady_clock::now();
                                peer_state.active_world_duration_ms = world.duration_ms;
                            } catch (const std::exception& ex) {
                                logger_->log("[game-udp][sync][experimental] failed to build initial sync: " +
                                             std::string(ex.what()));
                            }
                        }
                    }
                } else {
                    logger_->log("[game-udp][sync] from=" + std::string(sender_ip) + ":" +
                                 std::to_string(ntohs(sender.sin_port)) +
                                 " withholding gameplay inner UDP sync; transport ACK/probe is stable. Run with "
                                 "--experimental-game-udp-sync to test the native-order 0x02/0x03/0x04 sequence");
                }
                peer_state.sent_initial_room_sync = true;
                if (peer_state.native_session_state == NativeSessionState::kBootstrapAccepted) {
                    peer_state.native_session_state = NativeSessionState::kInitialSyncSent;
                }
            }
            if (should_send_start_elapsed) {
                const auto now = std::chrono::steady_clock::now();
                const auto elapsed_ms =
                    peer_state.initial_sync_sent_at.time_since_epoch().count() == 0
                        ? 0U
                        : static_cast<std::uint32_t>(std::min<std::int64_t>(
                              std::chrono::duration_cast<std::chrono::milliseconds>(
                                  now - peer_state.initial_sync_sent_at)
                                  .count(),
                              0xffffffffLL));
                core::ByteVector elapsed_payload;
                AppendU32Le(elapsed_payload, elapsed_ms);
                logger_->log("[game-udp][sync][experimental] client-ready opcode=0x01; sending native opcode=0x05 "
                             "elapsed_ms=" + std::to_string(elapsed_ms));
                send_initial_combat_stats();
                send_udp_reliable_inner(
                    peer_state,
                    0x05,
                    elapsed_payload,
                    "experimental post-ready opcode=0x05 elapsed time");
                peer_state.sent_start_elapsed = true;
                peer_state.native_session_state = NativeSessionState::kElapsedSent;
                peer_state.last_game_state_tick_sent_at = {};
            }
        } else if (transport_kind == 3) {
            send_udp_reply(BuildProbeReply(transport_word), "probe kind=3 reply kind=6");
        } else if (transport_kind == 4 || transport_kind == 5) {
            logger_->log("[game-udp][ack-list] from=" + std::string(sender_ip) + ":" +
                         std::to_string(ntohs(sender.sin_port)) + " kind=" +
                         std::to_string(transport_kind) +
                         DescribeAckList(payload, static_cast<std::uint16_t>(transport_word >> 3U)));
        } else {
            logger_->log("[game-udp][frame] from=" + std::string(sender_ip) + ":" +
                         std::to_string(ntohs(sender.sin_port)) + " kind=" +
                         std::to_string(transport_kind) + " no response implemented");
        }

        maybe_send_game_state_tick();
    }
}

}  // namespace cpp_server::udp
