#include "cpp_server/server/TcpLzssServer.h"

#include <functional>
#include <optional>
#include <string_view>

#include "cpp_server/packets/shared/PacketSupport.h"
#include "cpp_server/server/SessionPacketEffects.h"

namespace cpp_server::server {

void TcpLzssServer::send_shared_error(
    ClientSessionPtr session,
    std::uint16_t selector_opcode,
    std::uint16_t error_code,
    std::string description) {
    packets::s2c::SharedError_FF packet;
    packet.selector_opcode = selector_opcode;
    packet.error_code = error_code;
    const auto frame = packets::shared::ToFrame(packet);

    start_delayed_action(
        session,
        std::move(description),
        config_.auto_delay_ms,
        [this, session, frame]() { session->send_logical_frame(frame, logger_); });
}

ClientSessionPtr TcpLzssServer::find_authenticated_session_by_login(
    std::string_view login_id,
    const ClientSession* exclude_session) {
    std::scoped_lock lock(sessions_mutex_);
    for (const auto& [client_id, session] : sessions_) {
        (void)client_id;
        if (session.get() == exclude_session || session->closed) {
            continue;
        }

        std::scoped_lock session_lock(session->state_mutex);
        if (session->authenticated_login_id && *session->authenticated_login_id == login_id) {
            return session;
        }
    }
    return nullptr;
}


void TcpLzssServer::start_delayed_action(ClientSessionPtr session, std::string description, int delay_ms, std::function<void()> action) {
    packet_scheduler_.start_delayed_action(
        std::move(session), logger_, running_, std::move(description), delay_ms, std::move(action));
}

int TcpLzssServer::schedule_packet_stream(
    ClientSessionPtr session,
    const std::vector<core::LogicalPacketFrame>& packets,
    std::string description_prefix,
    std::optional<int> base_delay_ms) {
    return packet_scheduler_.schedule_packet_stream(
        std::move(session),
        logger_,
        running_,
        PacketStreamTiming{config_.list_stream_start_delay_ms, config_.list_stream_step_ms},
        packets,
        std::move(description_prefix),
        [this](const ClientSessionPtr& scheduled_session, const core::LogicalPacketFrame& packet) {
            ApplySentPacketSideEffects(scheduled_session, packet);
        },
        base_delay_ms);
}

void TcpLzssServer::schedule_stream_with_commit(
    ClientSessionPtr session,
    const std::vector<core::LogicalPacketFrame>& packets,
    std::string description_prefix,
    const core::LogicalPacketFrame& commit_packet,
    std::string commit_description,
    std::optional<int> base_delay_ms) {
    packet_scheduler_.schedule_stream_with_commit(
        std::move(session),
        logger_,
        running_,
        PacketStreamTiming{config_.list_stream_start_delay_ms, config_.list_stream_step_ms},
        packets,
        std::move(description_prefix),
        commit_packet,
        std::move(commit_description),
        [this](const ClientSessionPtr& scheduled_session, const core::LogicalPacketFrame& packet) {
            ApplySentPacketSideEffects(scheduled_session, packet);
        },
        base_delay_ms);
}


}  // namespace cpp_server::server
