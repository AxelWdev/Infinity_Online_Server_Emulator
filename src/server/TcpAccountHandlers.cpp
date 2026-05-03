#include "cpp_server/server/TcpLzssServer.h"

#include <optional>

#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::server {

void TcpLzssServer::auto_send_account_info(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    (void)frame;
    std::optional<std::string> login_id;
    {
        std::scoped_lock lock(session->state_mutex);
        login_id = session->authenticated_login_id;
    }
    if (!login_id) {
        logger_.log("[account] client=" + std::to_string(session->client_id) +
                    " sent 0x70 before successful authentication; refusing options-file fallback");
        send_shared_error(session, 0x0070, 7, "opcode=0x70 -> 0xFF unauthenticated");
        return;
    }

    const auto account_info = account_database_.find_account_info(*login_id);
    if (!account_info) {
        logger_.log("[account] client=" + std::to_string(session->client_id) +
                    " sent 0x70 but account '" + *login_id +
                    "' has no persisted packet_70 account info; refusing options-file fallback");
        send_shared_error(session, 0x0070, 7, "opcode=0x70 -> 0xFF missing account info");
        return;
    }

    logger_.log("[account] client=" + std::to_string(session->client_id) + " opcode=0x70 using account_database for '" +
                *login_id + "' level_raw=" + std::to_string(account_info->level_raw) +
                " luna=" + std::to_string(account_info->luna) +
                " cash=" + std::to_string(account_info->cash));
    const auto packet = packets::shared::ToFrame(*account_info);
    if (is_full_room_state_upload_active(session)) {
        start_delayed_action(
            session,
            "opcode=0x70 -> deferred payload-bearing 0x70 reply until full-room-state upload settles",
            config_.auto_delay_ms + 750,
            [this, session, packet]() {
                session->send_logical_frame(packet, logger_);
            });
        return;
    }
    start_delayed_action(session, "opcode=0x70 -> payload-bearing 0x70 reply logical", config_.auto_delay_ms,
                         [this, session, packet]() { session->send_logical_frame(packet, logger_); });
}

}  // namespace cpp_server::server
