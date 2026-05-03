#include "cpp_server/server/TcpLzssServer.h"

#include <algorithm>
#include <optional>

#include "cpp_server/game/InventoryService.h"
#include "cpp_server/packets/c2s/PacketCatalog.h"
#include "cpp_server/packets/s2c/AssignQuickSlot_47.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::server {

void TcpLzssServer::handle_assign_quickslot(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    std::optional<std::string> login_id;
    {
        std::scoped_lock lock(session->state_mutex);
        login_id = session->authenticated_login_id;
    }
    if (!login_id) {
        logger_.log("[quickslot] client=" + std::to_string(session->client_id) +
                    " sent 0x47 before successful authentication");
        send_shared_error(session, 0x0047, 7, "opcode=0x47 -> 0xFF unauthenticated");
        return;
    }

    try {
        const auto packet = packets::c2s::AssignQuickSlot_47::deserialize(frame.payload);
        if (packet.slot_index == 0 || packet.slot_index > game::kCurrentQuickslotCount) {
            logger_.log("[quickslot] client=" + std::to_string(session->client_id) +
                        " 0x47 rejected slot_index=" + std::to_string(packet.slot_index) +
                        " for '" + *login_id + "'");
            send_shared_error(session, 0x0047, 7, "opcode=0x47 -> 0xFF invalid quickslot index");
            return;
        }

        // Observed script paths:
        // - InsertQuickSlotbyItem sends kind=1 and item_or_skill_id as the item id.
        // - RemoveItemFromQuickSlot sends kind=0 and item_or_skill_id=0 for the selected slot.
        constexpr std::uint8_t kObservedItemQuickslotKind = 1;
        constexpr std::uint8_t kObservedClearQuickslotKind = 0;
        const bool clears_slot =
            packet.quickslot_item_kind == kObservedClearQuickslotKind && packet.item_or_skill_id == 0;
        if (!clears_slot && packet.quickslot_item_kind != kObservedItemQuickslotKind) {
            logger_.log("[quickslot] client=" + std::to_string(session->client_id) +
                        " 0x47 has unimplemented quickslot_item_kind=" +
                        std::to_string(static_cast<unsigned int>(packet.quickslot_item_kind)) +
                        " item_or_skill_id=" + std::to_string(packet.item_or_skill_id) +
                        " for '" + *login_id + "'");
            send_shared_error(session, 0x0047, 7, "opcode=0x47 -> 0xFF unsupported quickslot item kind");
            return;
        }

        const auto inventory = account_database_.find_inventory(*login_id);
        if (!inventory) {
            logger_.log("[quickslot] client=" + std::to_string(session->client_id) + " 0x47 failed because '" +
                        *login_id + "' has no persisted inventory");
            send_shared_error(session, 0x0047, 7, "opcode=0x47 -> 0xFF missing inventory");
            return;
        }

        const auto item_id = static_cast<std::uint32_t>(packet.item_or_skill_id);
        if (!clears_slot && !game::HasSharedItem(*inventory, item_id)) {
            logger_.log("[quickslot] client=" + std::to_string(session->client_id) +
                        " 0x47 rejected unowned item_id=" + std::to_string(item_id) +
                        " for '" + *login_id + "'");
            send_shared_error(session, 0x0047, 7, "opcode=0x47 -> 0xFF item not owned");
            return;
        }

        auto quickslots = effective_options_for_session(session).packet_44_entries;
        game::NormalizeQuickslotList(quickslots);
        if (game::IsPlaceholderQuickslotList(quickslots)) {
            quickslots = game::BuildQuickslotListFromInventoryItems(game_data_catalog_, *inventory);
        }

        auto target_it = std::find_if(
            quickslots.begin(),
            quickslots.end(),
            [&](const auto& entry) { return entry.slot_index == packet.slot_index; });
        if (target_it == quickslots.end()) {
            packets::s2c::EnumQuickSlot_44 entry;
            entry.slot_index = static_cast<std::uint8_t>(packet.slot_index);
            quickslots.push_back(entry);
            target_it = std::prev(quickslots.end());
        }

        target_it->entry_key = clears_slot ? 0 : item_id;
        target_it->quickslot_entry_id = clears_slot ? 0 : item_id;
        target_it->slot_state = clears_slot ? 0 : packet.quickslot_item_kind;
        target_it->display_lookup_key = clears_slot ? 0 : packet.item_or_skill_id;
        target_it->duration_minutes = 0;
        game::NormalizeQuickslotList(quickslots);

        if (!account_database_.set_quickslot_list(*login_id, quickslots)) {
            logger_.log("[quickslot] client=" + std::to_string(session->client_id) +
                        " 0x47 failed to persist quickslot list for '" + *login_id + "'");
            send_shared_error(session, 0x0047, 7, "opcode=0x47 -> 0xFF persist failure");
            return;
        }

        logger_.log("[quickslot] client=" + std::to_string(session->client_id) +
                    (clears_slot ? " cleared" : " assigned item_id=" + std::to_string(item_id) + " to") +
                    " slot_index=" + std::to_string(packet.slot_index) +
                    " current_entry_key=" + std::to_string(packet.current_entry_key) +
                    " kind=" + std::to_string(static_cast<unsigned int>(packet.quickslot_item_kind)) +
                    " field_08=" + std::to_string(packet.field_08) +
                    " for '" + *login_id + "' via 0x47");

        const auto ack = packets::shared::ToFrame(packets::s2c::AssignQuickSlot_47{});
        start_delayed_action(
            session,
            "opcode=0x47 -> empty 0x47 quickslot assignment acknowledgement logical",
            config_.auto_delay_ms,
            [this, session, ack]() { session->send_logical_frame(ack, logger_); });
    } catch (const std::exception& ex) {
        logger_.log("[quickslot] client=" + std::to_string(session->client_id) + " invalid 0x47 payload: " + ex.what());
        send_shared_error(session, 0x0047, 7, "opcode=0x47 -> 0xFF malformed quickslot assignment");
    }
}

}  // namespace cpp_server::server
