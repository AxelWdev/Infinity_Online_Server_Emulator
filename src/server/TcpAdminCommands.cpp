#include "cpp_server/server/TcpLzssServer.h"

#include <span>

#include "cpp_server/core/LogicalPacket.h"
#include "cpp_server/core/PacketLog.h"
#include "cpp_server/packets/s2c/EnumChannel_A3.h"
#include "cpp_server/packets/s2c/EnumChannelDone_A4.h"
#include "cpp_server/packets/shared/PacketSupport.h"
#include "cpp_server/server/SessionPacketEffects.h"

namespace cpp_server::server {

void TcpLzssServer::list_clients() {
    std::scoped_lock lock(sessions_mutex_);
    if (sessions_.empty()) {
        logger_.log("[server] no connected clients");
        return;
    }

    for (const auto& [client_id, session] : sessions_) {
        const char marker = (active_client_id_ && *active_client_id_ == client_id) ? '*' : ' ';
        logger_.log(std::string(1, marker) + " client=" + std::to_string(client_id) + " addr=" +
                    session->address_ip + ":" + std::to_string(session->address_port));
    }
}

void TcpLzssServer::set_active(int client_id) {
    std::scoped_lock lock(sessions_mutex_);
    if (!sessions_.contains(client_id)) {
        throw std::runtime_error("unknown client id " + std::to_string(client_id));
    }
    active_client_id_ = client_id;
    logger_.log("[server] active client set to " + std::to_string(client_id));
}

ClientSessionPtr TcpLzssServer::active_session() {
    std::scoped_lock lock(sessions_mutex_);
    if (!active_client_id_ || !sessions_.contains(*active_client_id_)) {
        throw std::runtime_error("no active client");
    }
    return sessions_.at(*active_client_id_);
}

void TcpLzssServer::send_logical_to_active(std::span<const std::uint8_t> packet) {
    auto session = active_session();
    if (const auto frame = core::TryParseSerializedLogicalPacket(packet)) {
        ApplySentPacketSideEffects(session, *frame);
    }
    session->send_logical_bytes(packet, logger_);
}

void TcpLzssServer::send_frame_to_active(std::uint8_t opcode, std::span<const std::uint8_t> payload) {
    send_logical_to_active(core::BuildLogicalPacket(opcode, payload).serialize());
}

void TcpLzssServer::send_raw_lzss_to_active(std::span<const std::uint8_t> packet) {
    active_session()->send_raw_lzss(packet, logger_);
}

void TcpLzssServer::send_enumchannel_ex_to_active(
    std::uint32_t channel_type_id,
    std::uint16_t channel_selector_id,
    std::uint32_t packed_ipv4,
    std::uint16_t tcp_port,
    std::string name) {
    packets::s2c::EnumChannel_A3 packet;
    packet.channel_type_id = channel_type_id;
    packet.channel_selector_id = channel_selector_id;
    packet.packed_ipv4 = packed_ipv4;
    packet.tcp_port = tcp_port;
    packet.name = std::move(name);
    const auto frame = packets::shared::ToFrame(packet).serialize();
    send_logical_to_active(frame);
}

void TcpLzssServer::send_enumdone_to_active() {
    send_logical_to_active(core::BuildLogicalPacket(packets::s2c::EnumChannelDone_A4::kOpcode).serialize());
}

void TcpLzssServer::close_active() {
    active_session()->close();
}

}  // namespace cpp_server::server
