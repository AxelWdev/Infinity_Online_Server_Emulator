#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "cpp_server/core/AccountDatabase.h"
#include "cpp_server/core/GameDataCatalog.h"
#include "cpp_server/packets/c2s/BuyItem_36.h"
#include "cpp_server/packets/c2s/EquipItem_48.h"
#include "cpp_server/packets/s2c/EnumQuickSlot_44.h"
#include "cpp_server/packets/s2c/UpdateAccountInfo_70.h"
#include "cpp_server/packets/s2c/UpdateCharacterList_6B.h"
#include "cpp_server/packets/s2c/UpdateItemList_3F.h"
#include "cpp_server/packets/s2c/UpdateSkillList_73.h"

namespace cpp_server::game {

inline constexpr std::uint8_t kCurrentQuickslotCount = 5;

enum class InventoryEquipSlot {
    kWeapon,
    kClothes,
    kAccessory1,
    kAccessory2,
    kAccessory3,
};

struct InventoryEquipTarget {
    std::size_t character_index{};
    std::uint32_t character_id{};
    InventoryEquipSlot slot{};
};

struct InventoryEquipBundleTarget {
    std::size_t character_index{};
    std::uint32_t character_id{};
};

[[nodiscard]] bool IsPlaceholderSkillList(const std::vector<packets::s2c::UpdateSkillList_73>& packets);
[[nodiscard]] bool IsPlaceholderQuickslotList(const std::vector<packets::s2c::EnumQuickSlot_44>& packets);
void NormalizeQuickslotList(std::vector<packets::s2c::EnumQuickSlot_44>& packets);
void NormalizeSkillDurations(std::vector<packets::s2c::UpdateSkillList_73>& packets);

[[nodiscard]] std::vector<packets::s2c::UpdateSkillList_73> BuildSkillListFromInventory(
    const core::GameDataCatalog& catalog,
    const core::AccountInventory& inventory);
[[nodiscard]] std::vector<packets::s2c::UpdateCharacterList_6B> BuildCharacterListFromInventory(
    const core::AccountInventory& inventory);
[[nodiscard]] std::vector<packets::s2c::UpdateItemList_3F> BuildItemListFromInventory(
    const core::AccountInventory& inventory);
[[nodiscard]] std::vector<std::uint16_t> OwnedSkillIdsForCharacter(
    const core::GameDataCatalog& catalog,
    const core::AccountInventory& inventory,
    std::uint32_t character_id);

[[nodiscard]] std::uint16_t PurchaseCountFromBuyRequest(const packets::c2s::BuyItem_36& packet);
[[nodiscard]] std::vector<packets::s2c::EnumQuickSlot_44> BuildQuickslotListFromInventoryItems(
    const core::GameDataCatalog& catalog,
    const core::AccountInventory& inventory);

[[nodiscard]] bool HasSharedItem(const core::AccountInventory& inventory, std::uint32_t item_id);
[[nodiscard]] bool MergeOwnedSkillIntoInventory(
    core::AccountInventory& inventory,
    std::uint32_t skill_item_id,
    std::uint32_t duration_minutes);
[[nodiscard]] std::optional<InventoryEquipTarget> FindInventoryEquipTarget(
    const core::AccountInventory& inventory,
    std::uint32_t item_id);
[[nodiscard]] std::optional<InventoryEquipBundleTarget> FindInventoryBundleTarget(
    const core::AccountInventory& inventory,
    const packets::c2s::EquipItem_48& packet,
    std::optional<std::uint32_t> preferred_character_id);
void ApplyEquippedItem(core::CharacterInventory& character, InventoryEquipSlot slot, std::uint32_t item_id);
void ClearEquippedItem(core::CharacterInventory& character, InventoryEquipSlot slot);
[[nodiscard]] bool ApplyKnownEquipmentBundle(
    core::CharacterInventory& character,
    const packets::c2s::EquipItem_48& packet);
void ClearKnownEquipmentBundle(core::CharacterInventory& character);
[[nodiscard]] bool RepairMisexpandedPackages(core::AccountInventory& inventory);
[[nodiscard]] bool RemoveItemFromInventory(core::AccountInventory& inventory, std::uint32_t item_id);
[[nodiscard]] bool ApplyBoughtItemToInventory(
    const core::GameDataCatalog& catalog,
    core::AccountInventory& inventory,
    std::uint32_t item_id,
    std::uint16_t purchase_count);
[[nodiscard]] bool ExpandStoredPackageTokens(
    const core::GameDataCatalog& catalog,
    core::AccountInventory& inventory,
    std::vector<std::uint32_t>& expanded_package_ids);
[[nodiscard]] bool ApplyShopPriceToAccountInfo(
    packets::s2c::UpdateAccountInfo_70& account_info,
    const core::ItemCatalogEntry& item,
    std::uint8_t buy_money_selection,
    std::uint16_t purchase_count);
[[nodiscard]] bool ApplyShopPriceToAccountInfo(
    packets::s2c::UpdateAccountInfo_70& account_info,
    const core::SkillCatalogEntry& skill,
    std::uint8_t buy_money_selection);
[[nodiscard]] std::string_view EquipSlotName(InventoryEquipSlot slot);

}  // namespace cpp_server::game
