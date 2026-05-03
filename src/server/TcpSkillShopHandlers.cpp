#include "cpp_server/server/TcpLzssServer.h"

#include <algorithm>
#include <optional>
#include <sstream>

#include "cpp_server/game/InventoryService.h"
#include "cpp_server/packets/c2s/PacketCatalog.h"
#include "cpp_server/packets/s2c/BuySkill_75.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::server {

void TcpLzssServer::handle_buy_skill(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    std::optional<std::string> login_id;
    {
        std::scoped_lock lock(session->state_mutex);
        login_id = session->authenticated_login_id;
    }
    if (!login_id) {
        logger_.log("[shop] client=" + std::to_string(session->client_id) +
                    " sent 0x75 before successful authentication");
        send_shared_error(session, 0x0075, 7, "opcode=0x75 -> 0xFF unauthenticated");
        return;
    }

    try {
        const auto packet = packets::c2s::BuySkill_75::deserialize(frame.payload);
        const auto skill = game_data_catalog_.find_skill(packet.skill_shop_id);
        if (!skill) {
            logger_.log("[shop] client=" + std::to_string(session->client_id) +
                        " 0x75 rejected unknown skill_shop_id=" + std::to_string(packet.skill_shop_id) +
                        " for '" + *login_id + "'");
            send_shared_error(session, 0x0075, 7, "opcode=0x75 -> 0xFF unknown shop skill");
            return;
        }

        const auto profile = account_database_.find_packet_profile(*login_id);
        if (!profile) {
            logger_.log("[shop] client=" + std::to_string(session->client_id) + " 0x75 failed because '" +
                        *login_id + "' has no persisted account profile");
            send_shared_error(session, 0x0075, 7, "opcode=0x75 -> 0xFF missing account profile");
            return;
        }
        if (!profile->account_info) {
            logger_.log("[shop] client=" + std::to_string(session->client_id) + " 0x75 failed because '" +
                        *login_id + "' has no persisted packet_70 account info; refusing options-file fallback");
            send_shared_error(session, 0x0075, 7, "opcode=0x75 -> 0xFF missing account info");
            return;
        }
        auto updated_profile = *profile;
        auto updated_account_info = *updated_profile.account_info;
        if (!updated_profile.inventory) {
            logger_.log("[shop] client=" + std::to_string(session->client_id) + " 0x75 failed because '" +
                        *login_id + "' has no persisted inventory");
            send_shared_error(session, 0x0075, 7, "opcode=0x75 -> 0xFF missing inventory");
            return;
        }

        if (!game::ApplyShopPriceToAccountInfo(updated_account_info, *skill, packet.buy_money_selection)) {
            logger_.log("[shop] client=" + std::to_string(session->client_id) +
                        " 0x75 rejected skill_shop_id=" + std::to_string(packet.skill_shop_id) +
                        " for '" + *login_id + "' because selector=" +
                        std::to_string(packet.buy_money_selection) + " could not pay price luna=" +
                        std::to_string(skill->luna_price) + " cash=" + std::to_string(skill->cash_price) +
                        " current_luna=" + std::to_string(updated_account_info.luna) +
                        " current_cash=" + std::to_string(updated_account_info.cash));
            send_shared_error(session, 0x0075, 7, "opcode=0x75 -> 0xFF insufficient or unsupported funds");
            return;
        }

        auto updated_inventory = *updated_profile.inventory;
        [[maybe_unused]] const auto skill_inventory_changed =
            game::MergeOwnedSkillIntoInventory(updated_inventory, skill->base_skill_id, skill->duration_minutes);
        updated_profile.account_info = updated_account_info;
        updated_profile.inventory = updated_inventory;
        updated_profile.skill_list.reset();

        if (!account_database_.set_packet_profile(*login_id, updated_profile)) {
            logger_.log("[shop] client=" + std::to_string(session->client_id) +
                        " 0x75 failed to persist bought skill inventory and account balance for '" + *login_id + "'");
            send_shared_error(session, 0x0075, 7, "opcode=0x75 -> 0xFF persist failure");
            return;
        }

        const auto owned_skill_it = std::find_if(
            updated_inventory.owned_skills.begin(),
            updated_inventory.owned_skills.end(),
            [&](const auto& owned_skill) { return owned_skill.skill_item_id == skill->base_skill_id; });
        std::ostringstream shop_log;
        shop_log << "[shop] client=" << session->client_id << " accepted 0x75 buy for skill_shop_id="
                 << packet.skill_shop_id << " owned_skill_id=" << skill->base_skill_id
                 << " buy_money_selection=" << static_cast<std::uint32_t>(packet.buy_money_selection)
                 << " duration_minutes=" << skill->duration_minutes;
        if (owned_skill_it != updated_inventory.owned_skills.end()) {
            shop_log << " total_owned_duration_minutes=" << owned_skill_it->duration_minutes;
        }
        shop_log << " on '" << *login_id
                 << "' remaining_luna=" << updated_account_info.luna
                 << " remaining_cash=" << updated_account_info.cash;
        logger_.log(shop_log.str());

        const auto ack = packets::shared::ToFrame(packets::s2c::BuySkill_75{});
        start_delayed_action(
            session,
            "opcode=0x75 -> empty 0x75 shop-skill acknowledgement logical",
            config_.auto_delay_ms,
            [this, session, ack]() { session->send_logical_frame(ack, logger_); });
    } catch (const std::exception& ex) {
        logger_.log("[shop] client=" + std::to_string(session->client_id) + " invalid 0x75 payload: " + ex.what());
        send_shared_error(session, 0x0075, 7, "opcode=0x75 -> 0xFF malformed shop-skill request");
    }
}

}  // namespace cpp_server::server
