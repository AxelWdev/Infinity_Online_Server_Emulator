#include "cpp_server/server/TcpLzssServer.h"

#include <optional>
#include <sstream>

#include "cpp_server/game/InventoryService.h"
#include "cpp_server/packets/c2s/PacketCatalog.h"
#include "cpp_server/packets/s2c/BuyItem_36.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::server {

void TcpLzssServer::handle_buy_item(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    std::optional<std::string> login_id;
    {
        std::scoped_lock lock(session->state_mutex);
        login_id = session->authenticated_login_id;
    }
    if (!login_id) {
        logger_.log("[shop] client=" + std::to_string(session->client_id) +
                    " sent 0x36 before successful authentication");
        send_shared_error(session, 0x0036, 7, "opcode=0x36 -> 0xFF unauthenticated");
        return;
    }

    try {
        const auto packet = packets::c2s::BuyItem_36::deserialize(frame.payload);
        const auto purchase_count = game::PurchaseCountFromBuyRequest(packet);
        const auto profile = account_database_.find_packet_profile(*login_id);
        if (!profile || !profile->inventory) {
            logger_.log("[shop] client=" + std::to_string(session->client_id) + " 0x36 failed because '" +
                        *login_id + "' has no persisted inventory");
            send_shared_error(session, 0x0036, 7, "opcode=0x36 -> 0xFF missing inventory");
            return;
        }
        if (!profile->account_info) {
            logger_.log("[shop] client=" + std::to_string(session->client_id) + " 0x36 failed because '" +
                        *login_id + "' has no persisted packet_70 account info");
            send_shared_error(session, 0x0036, 7, "opcode=0x36 -> 0xFF missing account info");
            return;
        }

        const auto item = game_data_catalog_.find_item(packet.item_id);
        if (!item) {
            logger_.log("[shop] client=" + std::to_string(session->client_id) +
                        " 0x36 rejected unknown item_id=" + std::to_string(packet.item_id) +
                        " for '" + *login_id + "'");
            send_shared_error(session, 0x0036, 7, "opcode=0x36 -> 0xFF unknown shop item");
            return;
        }

        auto updated_profile = *profile;
        auto updated_inventory = *updated_profile.inventory;
        const auto repaired_misexpanded_packages = game::RepairMisexpandedPackages(updated_inventory);
        if (repaired_misexpanded_packages) {
            logger_.log("[shop] client=" + std::to_string(session->client_id) +
                        " repaired previously misexpanded Baekho swimsuit package items for '" + *login_id + "'");
        }
        std::vector<std::uint32_t> expanded_stored_package_ids;
        const auto expanded_stored_packages =
            game::ExpandStoredPackageTokens(game_data_catalog_, updated_inventory, expanded_stored_package_ids);
        if (expanded_stored_packages) {
            std::ostringstream package_log;
            package_log << "[shop] client=" << session->client_id
                        << " expanded stored package token(s) for '" << *login_id << "':";
            for (const auto package_item_id : expanded_stored_package_ids) {
                package_log << " " << package_item_id;
            }
            logger_.log(package_log.str());
        }

        const auto purchase_changed =
            game::ApplyBoughtItemToInventory(game_data_catalog_, updated_inventory, packet.item_id, purchase_count);
        if (!purchase_changed) {
            logger_.log("[shop] client=" + std::to_string(session->client_id) +
                        " 0x36 item_id=" + std::to_string(packet.item_id) +
                        " purchase_count=" + std::to_string(purchase_count) +
                        " was already present for '" + *login_id + "'");
        } else {
            auto updated_account_info = *updated_profile.account_info;
            if (!game::ApplyShopPriceToAccountInfo(
                    updated_account_info, *item, packet.buy_money_selection, purchase_count)) {
                logger_.log("[shop] client=" + std::to_string(session->client_id) +
                            " 0x36 rejected item_id=" + std::to_string(packet.item_id) +
                            " purchase_count=" + std::to_string(purchase_count) +
                            " for '" + *login_id + "' because selector=" +
                            std::to_string(packet.buy_money_selection) + " could not pay price luna=" +
                            std::to_string(item->luna_price) + " cash=" + std::to_string(item->cash_price) +
                            " current_luna=" + std::to_string(updated_account_info.luna) +
                            " current_cash=" + std::to_string(updated_account_info.cash));
                send_shared_error(session, 0x0036, 7, "opcode=0x36 -> 0xFF insufficient or unsupported funds");
                return;
            }

            updated_profile.account_info = updated_account_info;
            updated_profile.inventory = updated_inventory;
        }

        if (!purchase_changed && (expanded_stored_packages || repaired_misexpanded_packages)) {
            updated_profile.inventory = updated_inventory;
        }

        if ((purchase_changed || expanded_stored_packages || repaired_misexpanded_packages) &&
            !account_database_.set_packet_profile(*login_id, updated_profile)) {
            logger_.log("[shop] client=" + std::to_string(session->client_id) +
                        " 0x36 failed to persist bought item and account balance for '" + *login_id + "'");
            send_shared_error(session, 0x0036, 7, "opcode=0x36 -> 0xFF persist failure");
            return;
        }

        std::ostringstream shop_log;
        shop_log << "[shop] client=" << session->client_id << " accepted 0x36 buy for item_id=" << packet.item_id
                 << " purchase_count=" << purchase_count
                 << " buy_money_selection=" << static_cast<std::uint32_t>(packet.buy_money_selection)
                 << " field_04=" << packet.field_04 << " on '" << *login_id << "'";
        if (const auto contents = game_data_catalog_.package_contents(packet.item_id); !contents.empty()) {
            shop_log << " expanded_package_contents=";
            for (std::size_t index = 0; index < contents.size(); ++index) {
                if (index != 0) {
                    shop_log << ",";
                }
                shop_log << contents[index];
            }
        }
        if (purchase_changed) {
            const auto& updated_account_info = *updated_profile.account_info;
            shop_log << " remaining_luna=" << updated_account_info.luna
                     << " remaining_cash=" << updated_account_info.cash;
        }
        logger_.log(shop_log.str());

        const auto ack = packets::shared::ToFrame(packets::s2c::BuyItem_36{});
        start_delayed_action(
            session,
            "opcode=0x36 -> empty 0x36 shop-buy acknowledgement logical",
            config_.auto_delay_ms,
            [this, session, ack]() { session->send_logical_frame(ack, logger_); });
    } catch (const std::exception& ex) {
        logger_.log("[shop] client=" + std::to_string(session->client_id) + " invalid 0x36 payload: " + ex.what());
        send_shared_error(session, 0x0036, 7, "opcode=0x36 -> 0xFF malformed shop-buy request");
    }
}

}  // namespace cpp_server::server
