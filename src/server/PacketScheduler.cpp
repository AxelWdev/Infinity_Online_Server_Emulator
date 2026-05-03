#include "cpp_server/server/PacketScheduler.h"

#include <chrono>

namespace cpp_server::server {

void PacketScheduler::join_all() {
    std::scoped_lock lock(threads_mutex_);
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    threads_.clear();
}

void PacketScheduler::start_delayed_action(
    ClientSessionPtr session,
    core::Logger& logger,
    const std::atomic<bool>& running,
    std::string description,
    int delay_ms,
    std::function<void()> action) {
    logger.log("[auto] client=" + std::to_string(session->client_id) + " " + description + " after " +
               std::to_string(delay_ms) + " ms");
    std::scoped_lock lock(threads_mutex_);
    threads_.emplace_back([session,
                           &logger,
                           &running,
                           description = std::move(description),
                           delay_ms,
                           action = std::move(action)]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        if (!running || session->closed) {
            return;
        }
        try {
            action();
        } catch (const std::exception& ex) {
            logger.log("[auto] client=" + std::to_string(session->client_id) + " " + description +
                       " failed: " + ex.what());
        }
    });
}

int PacketScheduler::schedule_packet_stream(
    ClientSessionPtr session,
    core::Logger& logger,
    const std::atomic<bool>& running,
    PacketStreamTiming timing,
    const std::vector<core::LogicalPacketFrame>& packets,
    std::string description_prefix,
    SideEffectCallback side_effects,
    std::optional<int> base_delay_ms) {
    const int start_delay = base_delay_ms.value_or(timing.start_delay_ms);
    const int step_delay = timing.step_delay_ms;
    if (start_delay <= 0 && step_delay <= 0) {
        for (const auto& packet : packets) {
            side_effects(session, packet);
            session->send_logical_frame(packet, logger);
        }
        return 0;
    }

    for (std::size_t index = 0; index < packets.size(); ++index) {
        const auto packet = packets[index];
        start_delayed_action(
            session,
            logger,
            running,
            description_prefix + " " + std::to_string(index) + " logical",
            start_delay + static_cast<int>(index) * step_delay,
            [session, &logger, packet, side_effects]() {
                side_effects(session, packet);
                session->send_logical_frame(packet, logger);
            });
    }
    return start_delay + static_cast<int>(packets.size()) * step_delay;
}

void PacketScheduler::schedule_stream_with_commit(
    ClientSessionPtr session,
    core::Logger& logger,
    const std::atomic<bool>& running,
    PacketStreamTiming timing,
    const std::vector<core::LogicalPacketFrame>& packets,
    std::string description_prefix,
    const core::LogicalPacketFrame& commit_packet,
    std::string commit_description,
    SideEffectCallback side_effects,
    std::optional<int> base_delay_ms) {
    const auto commit_delay = schedule_packet_stream(
        session, logger, running, timing, packets, std::move(description_prefix), side_effects, base_delay_ms);
    if (commit_delay <= 0) {
        side_effects(session, commit_packet);
        session->send_logical_frame(commit_packet, logger);
        return;
    }

    start_delayed_action(
        session,
        logger,
        running,
        std::move(commit_description),
        commit_delay,
        [session, &logger, commit_packet, side_effects]() {
            side_effects(session, commit_packet);
            session->send_logical_frame(commit_packet, logger);
        });
}

}  // namespace cpp_server::server
