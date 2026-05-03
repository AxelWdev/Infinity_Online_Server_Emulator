#include "cpp_server/server/TcpLzssServer.h"

#include <array>
#include <optional>
#include <span>

#include "cpp_server/core/LogicalPacket.h"
#include "cpp_server/core/PacketLog.h"

namespace cpp_server::server {

void TcpLzssServer::accept_loop() {
    while (running_) {
        sockaddr_in client_address{};
        socklen_t address_len = sizeof(client_address);
        const auto client_socket = accept(listener_, reinterpret_cast<sockaddr*>(&client_address), &address_len);
        if (!core::IsValidSocket(client_socket)) {
            if (running_) {
                logger_.log("[server] accept failed: " + core::LastSocketErrorText());
            }
            break;
        }

        int tcp_no_delay = 1;
        if (setsockopt(
                client_socket,
                IPPROTO_TCP,
                TCP_NODELAY,
                reinterpret_cast<const char*>(&tcp_no_delay),
                sizeof(tcp_no_delay)) != 0) {
            logger_.log("[server] client socket TCP_NODELAY failed: " + core::LastSocketErrorText());
        }

        char ip_buffer[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &client_address.sin_addr, ip_buffer, sizeof(ip_buffer));

        auto session = std::make_shared<ClientSession>();
        {
            std::scoped_lock lock(sessions_mutex_);
            session->client_id = next_client_id_++;
            session->socket = client_socket;
            session->address_ip = ip_buffer;
            session->address_port = ntohs(client_address.sin_port);
            sessions_.emplace(session->client_id, session);
            active_client_id_ = session->client_id;
        }

        logger_.log("[server] client " + std::to_string(session->client_id) + " connected from " + session->address_ip +
                    ":" + std::to_string(session->address_port));

        std::scoped_lock threads_lock(client_threads_mutex_);
        client_threads_.emplace_back(&TcpLzssServer::client_loop, this, session);
    }
}

void TcpLzssServer::client_loop(ClientSessionPtr session) {
    try {
        std::array<std::uint8_t, 4096> buffer{};
        while (running_ && !session->closed) {
#ifdef _WIN32
            const int received = recv(session->socket, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0);
#else
            const auto received = recv(session->socket, buffer.data(), buffer.size(), 0);
#endif
            if (received <= 0) {
                break;
            }

            const auto packet = std::span<const std::uint8_t>(buffer.data(), static_cast<std::size_t>(received));
            logger_.debug("[packet][C->S][lzss] client=" + std::to_string(session->client_id) +
                          " bytes=" + std::to_string(packet.size()) + " data=" + core::HexBytes(packet));
            const auto decoded = session->decoder.feed(packet);
            if (!decoded.empty()) {
                logger_.debug("[packet][C->S][logical-stream] client=" + std::to_string(session->client_id) +
                              " bytes=" + std::to_string(decoded.size()) + " data=" + core::HexBytes(decoded));
                session->logical_buffer.insert(session->logical_buffer.end(), decoded.begin(), decoded.end());
                drain_logical_packets(session);
            }
        }
    } catch (const std::exception& ex) {
        logger_.log("[server] client " + std::to_string(session->client_id) + " socket error: " + ex.what());
    }

    session->close();
    remove_created_rooms_for_session(session);
    {
        std::scoped_lock lock(sessions_mutex_);
        sessions_.erase(session->client_id);
        if (active_client_id_ && *active_client_id_ == session->client_id) {
            active_client_id_ = sessions_.empty() ? std::optional<int>{} : std::optional<int>{sessions_.begin()->first};
        }
    }
    logger_.log("[server] client " + std::to_string(session->client_id) + " disconnected");
}

void TcpLzssServer::drain_logical_packets(ClientSessionPtr session) {
    while (true) {
        auto frame = core::TryPopLogicalPacket(session->logical_buffer);
        if (!frame) {
            return;
        }

        logger_.log(core::FormatPacketLogLine("C->S", session->client_id, *frame, "RECV"));
        if (!frame->payload.empty()) {
            logger_.log(core::FormatPacketPayloadLine("C->S", session->client_id, *frame));
        }

        capture_dynamic_room_state(session, *frame);
        note_packet_context(session, frame->opcode);
        if (!maybe_schedule_auto_action(session, *frame)) {
            logger_.log(core::FormatUnhandledPacketLine(
                session->client_id,
                *frame,
                "no server handler or auto-reply registered for this opcode yet"));
        }
    }
}

}  // namespace cpp_server::server
