#include "cpp_server/server/TcpLzssServer.h"

#include <optional>

#include "cpp_server/packets/c2s/PacketCatalog.h"
#include "cpp_server/packets/s2c/NicknameHandshake_A6.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::server {

void TcpLzssServer::handle_character_name(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    std::optional<std::string> login_id;
    {
        std::scoped_lock lock(session->state_mutex);
        login_id = session->authenticated_login_id;
    }
    if (!login_id) {
        logger_.log("[auth] client=" + std::to_string(session->client_id) +
                    " sent 0xA6 before successful authentication");
        send_shared_error(session, 0x00A6, 7, "opcode=0xA6 -> 0xFF unauthenticated");
        return;
    }

    try {
        const auto packet = packets::c2s::NicknameHandshake_A6::deserialize(frame.payload);
        std::string effective_name;
        const auto result = account_database_.resolve_character_name(*login_id, packet.character_name, effective_name);

        if (result == core::ResolveCharacterNameResult::kInvalidName) {
            logger_.log("[auth] client=" + std::to_string(session->client_id) + " rejected nickname '" +
                        packet.character_name + "' for '" + *login_id + "'");
            send_shared_error(session, 0x00A6, 49, "opcode=0xA6 -> 0xFF invalid nickname");
            return;
        }

        if (result == core::ResolveCharacterNameResult::kNameInUse) {
            logger_.log("[auth] client=" + std::to_string(session->client_id) + " nickname already in use '" +
                        packet.character_name + "'");
            send_shared_error(session, 0x00A6, 10, "opcode=0xA6 -> 0xFF duplicate nickname");
            return;
        }

        if (result == core::ResolveCharacterNameResult::kAccountNotFound) {
            logger_.log("[auth] client=" + std::to_string(session->client_id) +
                        " nickname update failed because the authenticated account disappeared");
            send_shared_error(session, 0x00A6, 7, "opcode=0xA6 -> 0xFF missing account");
            return;
        }

        {
            std::scoped_lock lock(session->state_mutex);
            session->authenticated_nickname = effective_name;
        }

        logger_.log("[auth] client=" + std::to_string(session->client_id) + " nickname ready '" + effective_name +
                    "' for '" + *login_id + "'");

        const auto ack = packets::shared::ToFrame(packets::s2c::NicknameHandshake_A6{});
        start_delayed_action(
            session,
            "opcode=0xA6 -> empty 0xA6 nickname acknowledgement logical",
            config_.auto_delay_ms,
            [this, session, ack]() { session->send_logical_frame(ack, logger_); });
    } catch (const std::exception& ex) {
        logger_.log("[auth] client=" + std::to_string(session->client_id) + " invalid 0xA6 payload: " + ex.what());
        send_shared_error(session, 0x00A6, 49, "opcode=0xA6 -> 0xFF malformed nickname");
    }
}

}  // namespace cpp_server::server
