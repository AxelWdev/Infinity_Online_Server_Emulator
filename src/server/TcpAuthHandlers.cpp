#include "cpp_server/server/TcpLzssServer.h"

#include "cpp_server/packets/c2s/PacketCatalog.h"
#include "cpp_server/packets/shared/PacketSupport.h"
#include "cpp_server/server/RoomStatePackets.h"

namespace cpp_server::server {

void TcpLzssServer::handle_connect_lobby(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    try {
        const auto packet = packets::c2s::ConnectLobby_01::deserialize(frame.payload);
        if (const auto account = account_database_.find_by_login(packet.login_id)) {
            {
                std::scoped_lock lock(session->state_mutex);
                session->authenticated_login_id = account->login_id;
                session->authenticated_nickname = account->nickname;
            }

            logger_.log("[auth] client=" + std::to_string(session->client_id) + " reconnect-bound to '" +
                        account->login_id + "' via 0x01");
        } else {
            logger_.log("[auth] client=" + std::to_string(session->client_id) + " 0x01 login lookup missed for '" +
                        packet.login_id + "'");
        }
    } catch (const std::exception& ex) {
        logger_.log("[auth] client=" + std::to_string(session->client_id) + " invalid 0x01 payload: " + ex.what());
    }

    auto_send_empty_completion(session, frame);
}

void TcpLzssServer::handle_login_challenge(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    try {
        const auto packet = packets::c2s::LoginChallenge_9F::deserialize(frame.payload);
        const auto account = account_database_.authenticate(packet.login_id, packet.password_value);
        if (!account) {
            logger_.log("[auth] client=" + std::to_string(session->client_id) + " login failed for '" + packet.login_id + "'");
            send_shared_error(session, 0x009F, 8, "opcode=0x9F -> 0xFF login failure");
            return;
        }

        if (const auto existing = find_authenticated_session_by_login(packet.login_id, session.get())) {
            if (!packet.force_disconnect_flag) {
                logger_.log("[auth] client=" + std::to_string(session->client_id) + " reconnect required for '" +
                            packet.login_id + "'");
                send_shared_error(session, 0x009F, 4, "opcode=0x9F -> 0xFF reconnect-required");
                return;
            }

            logger_.log("[auth] client=" + std::to_string(session->client_id) + " forcing previous login for '" +
                        packet.login_id + "'");
            existing->close();
        }

        {
            std::scoped_lock lock(session->state_mutex);
            session->authenticated_login_id = account->login_id;
            session->authenticated_nickname = account->nickname;
        }

        const auto reply = packets::shared::ToFrame(BuildLoginSuccessPacket(*account));
        logger_.log("[auth] client=" + std::to_string(session->client_id) + " login accepted for '" + account->login_id +
                    "'" + (account->nickname && !account->nickname->empty()
                               ? " with persisted nickname '" + *account->nickname + "'"
                               : " without a persisted nickname"));
        start_delayed_action(
            session,
            "opcode=0x9F -> payload-bearing 0x9F login continuation logical",
            config_.auto_delay_ms,
            [this, session, reply]() { session->send_logical_frame(reply, logger_); });
    } catch (const std::exception& ex) {
        logger_.log("[auth] client=" + std::to_string(session->client_id) + " invalid 0x9F payload: " + ex.what());
        send_shared_error(session, 0x009F, 7, "opcode=0x9F -> 0xFF malformed login");
    }
}

}  // namespace cpp_server::server
