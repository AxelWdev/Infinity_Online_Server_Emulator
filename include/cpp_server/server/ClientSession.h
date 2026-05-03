#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "cpp_server/core/Logger.h"
#include "cpp_server/core/LogicalPacket.h"
#include "cpp_server/core/LzssCodec.h"
#include "cpp_server/core/SocketPlatform.h"

namespace cpp_server::server {

struct ClientSession {
    int client_id{};
    core::SocketHandle socket{core::kInvalidSocket};
    std::string address_ip{};
    std::uint16_t address_port{};

    core::LzssDecoder decoder{};
    core::LzssEncoder encoder{};
    core::ByteVector logical_buffer{};

    std::mutex send_mutex{};
    std::mutex state_mutex{};
    std::atomic<bool> closed{false};

    std::optional<std::uint8_t> last_opcode{};
    std::chrono::steady_clock::time_point last_packet_monotonic{};
    std::chrono::steady_clock::time_point full_room_state_upload_deadline{};
    bool pending_room_leave_request{};
    std::optional<std::string> authenticated_login_id{};
    std::optional<std::string> authenticated_nickname{};
    std::optional<std::uint32_t> observed_equipment_character_id{};

    std::optional<std::string> created_room_name{};
    std::optional<std::string> created_mission_title{};
    std::optional<std::uint16_t> created_mission_rule_id{};
    std::optional<std::uint8_t> created_room_max_players{};
    bool awaiting_room_create_completion{};
    bool awaiting_room_enter_token{};

    void send_logical_frame(const core::LogicalPacketFrame& packet, core::Logger& logger);
    void send_logical_bytes(std::span<const std::uint8_t> packet, core::Logger& logger);
    void send_raw_lzss(std::span<const std::uint8_t> packet, core::Logger& logger);
    void close();
};

using ClientSessionPtr = std::shared_ptr<ClientSession>;

}  // namespace cpp_server::server
