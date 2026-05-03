#include "cpp_server/server/TcpLzssServer.h"

#include <optional>

#include "cpp_server/game/InventoryService.h"
#include "cpp_server/packets/c2s/PacketCatalog.h"
#include "cpp_server/packets/s2c/RemoveItem_39.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::server {

void TcpLzssServer::handle_remove_item(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    std::optional<std::string> login_id;
    {
        std::scoped_lock lock(session->state_mutex);
        login_id = session->authenticated_login_id;
    }
    if (!login_id) {
        logger_.log("[inventory] client=" + std::to_string(session->client_id) +
                    " sent 0x39 before successful authentication");
        send_shared_error(session, 0x0039, 7, "opcode=0x39 -> 0xFF unauthenticated");
        return;
    }

    try {
        const auto packet = packets::c2s::RemoveItem_39::deserialize(frame.payload);
        const auto inventory = account_database_.find_inventory(*login_id);
        if (!inventory) {
            logger_.log("[inventory] client=" + std::to_string(session->client_id) + " 0x39 failed because '" +
                        *login_id + "' has no persisted inventory");
            send_shared_error(session, 0x0039, 7, "opcode=0x39 -> 0xFF missing inventory");
            return;
        }

        auto updated_inventory = *inventory;
        if (!game::RemoveItemFromInventory(updated_inventory, packet.item_id)) {
            logger_.log("[inventory] client=" + std::to_string(session->client_id) +
                        " 0x39 item_id=" + std::to_string(packet.item_id) +
                        " was not present for '" + *login_id + "'");
            send_shared_error(session, 0x0039, 7, "opcode=0x39 -> 0xFF missing removable item");
            return;
        }

        if (!account_database_.set_inventory(*login_id, updated_inventory)) {
            logger_.log("[inventory] client=" + std::to_string(session->client_id) +
                        " 0x39 failed to persist inventory for '" + *login_id + "'");
            send_shared_error(session, 0x0039, 7, "opcode=0x39 -> 0xFF persist failure");
            return;
        }

        logger_.log("[inventory] client=" + std::to_string(session->client_id) +
                    " removed item_id=" + std::to_string(packet.item_id) +
                    " from '" + *login_id + "' via 0x39");

        const auto ack = packets::shared::ToFrame(packets::s2c::RemoveItem_39{});
        start_delayed_action(
            session,
            "opcode=0x39 -> empty 0x39 inventory-delete acknowledgement logical",
            config_.auto_delay_ms,
            [this, session, ack]() { session->send_logical_frame(ack, logger_); });
    } catch (const std::exception& ex) {
        logger_.log("[inventory] client=" + std::to_string(session->client_id) + " invalid 0x39 payload: " + ex.what());
        send_shared_error(session, 0x0039, 7, "opcode=0x39 -> 0xFF malformed removeitem request");
    }
}

}  // namespace cpp_server::server
