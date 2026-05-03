#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "cpp_server/core/Logger.h"
#include "cpp_server/core/LogicalPacket.h"
#include "cpp_server/server/ClientSession.h"

namespace cpp_server::server {

struct PacketStreamTiming {
    int start_delay_ms{};
    int step_delay_ms{};
};

class PacketScheduler {
public:
    using SideEffectCallback = std::function<void(const ClientSessionPtr&, const core::LogicalPacketFrame&)>;

    void join_all();

    void start_delayed_action(
        ClientSessionPtr session,
        core::Logger& logger,
        const std::atomic<bool>& running,
        std::string description,
        int delay_ms,
        std::function<void()> action);

    [[nodiscard]] int schedule_packet_stream(
        ClientSessionPtr session,
        core::Logger& logger,
        const std::atomic<bool>& running,
        PacketStreamTiming timing,
        const std::vector<core::LogicalPacketFrame>& packets,
        std::string description_prefix,
        SideEffectCallback side_effects,
        std::optional<int> base_delay_ms = std::nullopt);

    void schedule_stream_with_commit(
        ClientSessionPtr session,
        core::Logger& logger,
        const std::atomic<bool>& running,
        PacketStreamTiming timing,
        const std::vector<core::LogicalPacketFrame>& packets,
        std::string description_prefix,
        const core::LogicalPacketFrame& commit_packet,
        std::string commit_description,
        SideEffectCallback side_effects,
        std::optional<int> base_delay_ms = std::nullopt);

private:
    std::mutex threads_mutex_{};
    std::vector<std::thread> threads_{};
};

}  // namespace cpp_server::server
