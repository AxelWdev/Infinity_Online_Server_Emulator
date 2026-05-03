#include "cpp_server/server/TcpLzssServer.h"

#include <algorithm>
#include <optional>

#include "cpp_server/game/InventoryService.h"
#include "cpp_server/packets/c2s/PacketCatalog.h"
#include "cpp_server/packets/s2c/EquipItem_48.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::server {

void TcpLzssServer::handle_equip_item(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    std::optional<std::string> login_id;
    std::optional<std::uint32_t> preferred_character_id;
    {
        std::scoped_lock lock(session->state_mutex);
        login_id = session->authenticated_login_id;
        preferred_character_id = session->observed_equipment_character_id;
    }
    if (!login_id) {
        logger_.log("[inventory] client=" + std::to_string(session->client_id) +
                    " sent 0x48 before successful authentication");
        send_shared_error(session, 0x0048, 7, "opcode=0x48 -> 0xFF unauthenticated");
        return;
    }

    try {
        const auto packet = packets::c2s::EquipItem_48::deserialize(frame.payload);
        const auto inventory = account_database_.find_inventory(*login_id);
        if (!inventory) {
            logger_.log("[inventory] client=" + std::to_string(session->client_id) + " 0x48 failed because '" +
                        *login_id + "' has no persisted inventory");
            send_shared_error(session, 0x0048, 7, "opcode=0x48 -> 0xFF missing inventory");
            return;
        }

        auto updated_inventory = *inventory;
        // Observed 0x48 shapes so far:
        // - weapon only: slot3/field_0c = weapon item id
        // - clothes+weapon: slot1/field_04 = clothes item id, slot3/field_0c = weapon item id
        // - clear: all managed fields zero for the currently selected character
        const bool is_all_zero = packet.field_00 == 0 && packet.field_04 == 0 && packet.field_08 == 0 &&
                                 packet.field_0c == 0 && packet.field_10 == 0 && packet.field_14 == 0 &&
                                 packet.field_18 == 0;

        std::string inventory_log;
        bool inventory_changed = false;

        if (is_all_zero) {
            if (!preferred_character_id) {
                logger_.log("[inventory] client=" + std::to_string(session->client_id) +
                            " all-zero 0x48 had no remembered character context for '" + *login_id + "'");
                send_shared_error(session, 0x0048, 7, "opcode=0x48 -> 0xFF missing equip character context");
                return;
            }

            auto character_it = std::find_if(
                updated_inventory.characters.begin(),
                updated_inventory.characters.end(),
                [&](const auto& character) { return character.character_id == *preferred_character_id; });
            if (character_it == updated_inventory.characters.end()) {
                logger_.log("[inventory] client=" + std::to_string(session->client_id) +
                            " all-zero 0x48 referenced missing character_id=" +
                            std::to_string(*preferred_character_id) + " for '" + *login_id + "'");
                send_shared_error(session, 0x0048, 7, "opcode=0x48 -> 0xFF invalid equip character context");
                return;
            }

            inventory_changed = character_it->equipped_weapon_item_id != 0 || character_it->clothes_item_id != 0 ||
                                character_it->accessory_1_item_id != 0 || character_it->accessory_2_item_id != 0 ||
                                character_it->accessory_3_item_id != 0;
            game::ClearKnownEquipmentBundle(*character_it);
            inventory_log = "[inventory] client=" + std::to_string(session->client_id) +
                            " cleared managed equipment bundle for character_id=" +
                            std::to_string(character_it->character_id) + " on '" + *login_id + "'";
        } else {
            const auto target = game::FindInventoryBundleTarget(updated_inventory, packet, preferred_character_id);
            if (!target) {
                logger_.log("[inventory] client=" + std::to_string(session->client_id) +
                            " 0x48 bundle did not map to a unique owned character for '" + *login_id +
                            "': field_00=" + std::to_string(packet.field_00) +
                            " field_04=" + std::to_string(packet.field_04) +
                            " field_08=" + std::to_string(packet.field_08) +
                            " field_0c=" + std::to_string(packet.field_0c) +
                            " field_10=" + std::to_string(packet.field_10) +
                            " field_14=" + std::to_string(packet.field_14) +
                            " field_18=" + std::to_string(packet.field_18));
                send_shared_error(session, 0x0048, 7, "opcode=0x48 -> 0xFF invalid equip bundle");
                return;
            }

            auto& character = updated_inventory.characters[target->character_index];
            inventory_changed = game::ApplyKnownEquipmentBundle(character, packet);
            {
                std::scoped_lock lock(session->state_mutex);
                session->observed_equipment_character_id = target->character_id;
            }
            inventory_log = "[inventory] client=" + std::to_string(session->client_id) +
                            " applied 0x48 bundle to character_id=" + std::to_string(target->character_id) +
                            " on '" + *login_id + "' slot1_clothes=" + std::to_string(packet.field_04) +
                            " slot3_weapon=" + std::to_string(packet.field_0c) +
                            " slot4_acc1=" + std::to_string(packet.field_10) +
                            " slot5_acc2=" + std::to_string(packet.field_14) +
                            " slot6_acc3=" + std::to_string(packet.field_18);
            if (packet.field_00 != 0 || packet.field_08 != 0) {
                inventory_log += " unresolved_slot0=" + std::to_string(packet.field_00) +
                                 " unresolved_slot2=" + std::to_string(packet.field_08);
            }
        }

        if (inventory_changed) {
            if (!account_database_.set_inventory(*login_id, updated_inventory)) {
                logger_.log("[inventory] client=" + std::to_string(session->client_id) +
                            " 0x48 failed to persist inventory for '" + *login_id + "'");
                send_shared_error(session, 0x0048, 7, "opcode=0x48 -> 0xFF persist failure");
                return;
            }
        }

        logger_.log(inventory_log);

        const auto ack = packets::shared::ToFrame(packets::s2c::EquipItem_48{});
        start_delayed_action(
            session,
            "opcode=0x48 -> empty 0x48 equip acknowledgement logical",
            config_.auto_delay_ms,
            [this, session, ack]() { session->send_logical_frame(ack, logger_); });
    } catch (const std::exception& ex) {
        logger_.log("[inventory] client=" + std::to_string(session->client_id) + " invalid 0x48 payload: " + ex.what());
        send_shared_error(session, 0x0048, 7, "opcode=0x48 -> 0xFF malformed equip request");
    }
}


}  // namespace cpp_server::server
