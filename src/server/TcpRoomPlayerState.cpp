#include "cpp_server/server/TcpLzssServer.h"

#include <algorithm>
#include <optional>

#include "cpp_server/game/InventoryService.h"
#include "cpp_server/server/RoomStatePackets.h"

namespace cpp_server::server {

std::vector<packets::s2c::FullRoomStateSlot_85> TcpLzssServer::room_slot_entries_for_session(
    const ClientSessionPtr& session,
    const core::ServerOptions& effective_options) {
    if (!effective_options.packet_85_entries.empty()) {
        return effective_options.packet_85_entries;
    }

    std::optional<std::string> authenticated_login_id;
    std::optional<std::uint32_t> selected_character_id;
    {
        std::scoped_lock lock(session->state_mutex);
        authenticated_login_id = session->authenticated_login_id;
        selected_character_id = session->observed_equipment_character_id;
    }
    if (!authenticated_login_id || !selected_character_id || *selected_character_id > 0xFF) {
        return {};
    }

    const auto inventory = account_database_.find_inventory(*authenticated_login_id);
    if (!inventory) {
        return {};
    }

    const auto character_it = std::find_if(
        inventory->characters.begin(),
        inventory->characters.end(),
        [&](const auto& character) { return character.character_id == *selected_character_id; });
    if (character_it == inventory->characters.end()) {
        logger_.log("[auto] client=" + std::to_string(session->client_id) +
                    " no 0x85 room slot inventory row for character_id=" +
                    std::to_string(*selected_character_id));
        return {};
    }

    packets::s2c::FullRoomStateSlot_85 packet;
    packet.lookup_key_00 = static_cast<std::uint8_t>(*selected_character_id);
    packet.field_01 = 0;
    packet.field_03 = static_cast<std::uint16_t>(character_it->clothes_item_id);
    packet.field_05 = 0;
    packet.field_09 = character_it->equipped_weapon_item_id;
    packet.field_0d = character_it->accessory_1_item_id;
    packet.field_11 = static_cast<std::uint16_t>(character_it->accessory_2_item_id);
    packet.field_13 = static_cast<std::uint16_t>(character_it->accessory_3_item_id);
    packet.field_15 = 0;

    logger_.log("[auto] client=" + std::to_string(session->client_id) +
                " synthesized 0x85 room slot for character_id=" + std::to_string(*selected_character_id) +
                " clothes=" + std::to_string(character_it->clothes_item_id) +
                " weapon=" + std::to_string(character_it->equipped_weapon_item_id) +
                " acc1=" + std::to_string(character_it->accessory_1_item_id) +
                " acc2=" + std::to_string(character_it->accessory_2_item_id) +
                " acc3=" + std::to_string(character_it->accessory_3_item_id));
    return {packet};
}

std::vector<packets::s2c::StateUpdate_9E> TcpLzssServer::room_user_state_entries_for_session(
    const ClientSessionPtr& session,
    const core::ServerOptions& effective_options) {
    (void)effective_options;
    std::optional<std::uint32_t> selected_character_id;
    {
        std::scoped_lock lock(session->state_mutex);
        selected_character_id = session->observed_equipment_character_id;
    }
    if (!selected_character_id || *selected_character_id > 0xFF) {
        return {};
    }

    logger_.log("[auto] client=" + std::to_string(session->client_id) +
                " not synthesizing room 0x9E for character_id=" + std::to_string(*selected_character_id) +
                "; 0x9E is the +0x4594 mission/state list, not character selection state");
    return {};
}

std::optional<packets::s2c::RoomPlayerState_16> TcpLzssServer::room_player_state_for_session(
    const ClientSessionPtr& session) {
    std::optional<std::string> authenticated_login_id;
    std::optional<std::string> authenticated_nickname;
    std::optional<std::uint32_t> selected_character_id;
    {
        std::scoped_lock lock(session->state_mutex);
        authenticated_login_id = session->authenticated_login_id;
        authenticated_nickname = session->authenticated_nickname;
        selected_character_id = session->observed_equipment_character_id;
    }

    if (!selected_character_id || *selected_character_id > 0xFF) {
        return std::nullopt;
    }

    const auto player_name = authenticated_nickname && !authenticated_nickname->empty()
                                 ? *authenticated_nickname
                                 : authenticated_login_id.value_or("Player");

    packets::s2c::RoomPlayerState_16 packet;
    packet.player_name = player_name;
    packet.field_0d = *selected_character_id;
    packet.field_31 = *selected_character_id;
    packet.flags = 0x08;
    packet.player_id = authenticated_login_id
                           ? StableReconnectLoginDword(*authenticated_login_id)
                           : *selected_character_id;

    if (authenticated_login_id) {
        if (const auto account_info = account_database_.find_account_info(*authenticated_login_id)) {
            const auto level_raw = std::max<std::uint8_t>(1, account_info->level_raw);
            packet.field_06 = static_cast<std::uint8_t>(*selected_character_id);
            packet.field_08 = level_raw;
        } else {
            packet.field_06 = static_cast<std::uint8_t>(*selected_character_id);
            packet.field_08 = 1;
        }

        const auto inventory = account_database_.find_inventory(*authenticated_login_id);
        if (inventory) {
            const auto character_it = std::find_if(
                inventory->characters.begin(),
                inventory->characters.end(),
                [&](const auto& character) { return character.character_id == *selected_character_id; });
            if (character_it != inventory->characters.end()) {
                packet.equipment_fields[0] = 0;
                packet.equipment_fields[1] = character_it->clothes_item_id;
                packet.equipment_fields[2] = 0;
                packet.equipment_fields[3] = character_it->equipped_weapon_item_id;
                packet.equipment_fields[4] = character_it->accessory_1_item_id;
                packet.equipment_fields[5] = character_it->accessory_2_item_id;
                packet.equipment_fields[6] = character_it->accessory_3_item_id;
            } else {
                logger_.log("[auto] client=" + std::to_string(session->client_id) +
                            " no 0x16 equipment inventory row for character_id=" +
                            std::to_string(*selected_character_id));
            }
        }
    } else {
        packet.field_06 = static_cast<std::uint8_t>(*selected_character_id);
        packet.field_08 = 1;
    }

    logger_.log("[auto] client=" + std::to_string(session->client_id) +
                " synthesized 0x16 room player state name='" + player_name +
                "' character_id=" + std::to_string(*selected_character_id) +
                " hero_id_field_06=" + std::to_string(packet.field_06) +
                " win_count_field_07=" + std::to_string(packet.field_07) +
                " level_candidate_field_08=" + std::to_string(packet.field_08) +
                " player_id=" + std::to_string(packet.player_id) +
                " slot1_clothes=" + std::to_string(packet.equipment_fields[1]) +
                " slot3_weapon=" + std::to_string(packet.equipment_fields[3]) +
                " slot4_acc1=" + std::to_string(packet.equipment_fields[4]) +
                " slot5_acc2=" + std::to_string(packet.equipment_fields[5]) +
                " slot6_acc3=" + std::to_string(packet.equipment_fields[6]));
    return packet;
}


}  // namespace cpp_server::server
