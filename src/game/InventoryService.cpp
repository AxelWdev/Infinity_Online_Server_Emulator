#include "cpp_server/game/InventoryService.h"

#include <algorithm>
#include <unordered_set>

namespace cpp_server::game {

namespace {

constexpr std::uint8_t kShopSelectionCash = 1;
constexpr std::uint8_t kShopSelectionLuna = 2;

template <typename Container>
bool ContainsItemId(const Container& values, std::uint32_t item_id) {
    return std::find(values.begin(), values.end(), item_id) != values.end();
}

}  // namespace

bool IsPlaceholderSkillList(const std::vector<packets::s2c::UpdateSkillList_73>& packets) {
    return packets.empty() ||
           std::all_of(
               packets.begin(),
               packets.end(),
               [](const auto& packet) { return packet.skill_item_id == 0; });
}

bool IsPlaceholderQuickslotList(const std::vector<packets::s2c::EnumQuickSlot_44>& packets) {
    return packets.empty() ||
           std::all_of(
               packets.begin(),
               packets.end(),
               [](const auto& packet) {
                   return packet.slot_state == 0 && packet.quickslot_entry_id == 0 && packet.display_lookup_key == 0;
               });
}

void NormalizeQuickslotList(std::vector<packets::s2c::EnumQuickSlot_44>& packets) {
    // LobbyScript_Inventory_Quickslot inserts quickslot buttons 1-5.
    // Slots 6-10 are only present in commented legacy UI script blocks.
    packets.erase(
        std::remove_if(
            packets.begin(),
            packets.end(),
            [](const auto& packet) { return packet.slot_index > kCurrentQuickslotCount; }),
        packets.end());

    if (packets.size() > kCurrentQuickslotCount) {
        packets.resize(kCurrentQuickslotCount);
    }
}

std::vector<packets::s2c::UpdateSkillList_73> BuildSkillListFromInventory(
    const core::GameDataCatalog& catalog,
    const core::AccountInventory& inventory) {
    std::vector<packets::s2c::UpdateSkillList_73> packets;
    packets.reserve(inventory.owned_skills.size());

    for (const auto& skill : inventory.owned_skills) {
        if (skill.skill_item_id == 0) {
            continue;
        }

        packets::s2c::UpdateSkillList_73 packet;
        packet.field_00 = skill.skill_item_id;
        // Confirmed client behavior: record +0x04 gates the training setup skill emission path.
        // The best supported catalog key for that grouping is the skill's owning character id.
        if (const auto catalog_skill = catalog.find_skill(skill.skill_item_id)) {
            packet.field_04 = catalog_skill->character_id;
        } else {
            packet.field_04 = skill.skill_item_id;
        }
        packet.skill_item_id = skill.skill_item_id;
        packet.duration_minutes = skill.duration_minutes;
        packet.update_existing_flag = 0;
        packets.push_back(packet);
    }

    std::sort(
        packets.begin(),
        packets.end(),
        [](const auto& left, const auto& right) { return left.skill_item_id < right.skill_item_id; });
    return packets;
}

void NormalizeSkillDurations(std::vector<packets::s2c::UpdateSkillList_73>& packets) {
    for (auto& packet : packets) {
        if (packet.skill_item_id != 0 && packet.field_04 == 0) {
            packet.field_04 = packet.skill_item_id;
        }
    }
}

void AppendCatalogItemPacket(
    std::vector<packets::s2c::UpdateItemList_3F>& packets,
    std::unordered_set<std::uint32_t>& seen_item_ids,
    std::uint32_t item_id,
    std::uint16_t owned_count) {
    if (item_id == 0 || !seen_item_ids.insert(item_id).second) {
        return;
    }

    packets::s2c::UpdateItemList_3F packet;
    packet.field_00 = item_id;
    // Inference: the item quickslot path copies the low 16 bits of field_04 into the
    // quickslot display key, so the conservative fallback reuses the real item id here
    // until the exact server-side meaning of field_04 is confirmed.
    packet.field_04 = item_id;
    // Confirmed local consumer behavior: the owned count is read from item record +0x2c.
    // The `0x3F` dequeue path zero-extends payload field_28 into that aligned slot.
    packet.field_28 = owned_count;
    packets.push_back(packet);
}

void AppendInventoryOwnedItemId(
    std::vector<packets::s2c::UpdateItemList_3F>& packets,
    std::unordered_set<std::uint32_t>& seen_item_ids,
    std::uint32_t item_id,
    std::uint16_t owned_count) {
    if (item_id == 0) {
        return;
    }
    AppendCatalogItemPacket(packets, seen_item_ids, item_id, owned_count);
}

bool IsBagLikeItem(const core::GameDataCatalog& catalog, std::uint32_t item_id) {
    const auto item = catalog.find_item(item_id);
    if (!item) {
        return false;
    }

    return item->item_type_name == "Bag" || item->equip_slot_name == "Bag";
}

std::vector<packets::s2c::UpdateCharacterList_6B> BuildCharacterListFromInventory(
    const core::AccountInventory& inventory) {
    std::vector<packets::s2c::UpdateCharacterList_6B> packets;
    packets.reserve(inventory.characters.size());

    for (const auto& character : inventory.characters) {
        packets::s2c::UpdateCharacterList_6B packet;
        packet.character_id = character.character_id;
        // Confirmed client-side mapping: weapon -> slot 3, accessories -> slots 4..6.
        // The cloth/dye bundle lives in slots 0..2; current inventory only drives main clothes in slot 1.
        packet.room_config_slot_0 = 0;
        packet.room_config_slot_1 = character.clothes_item_id;
        packet.room_config_slot_2 = 0;
        packet.room_config_slot_3 = character.equipped_weapon_item_id;
        packet.room_config_slot_4 = character.accessory_1_item_id;
        packet.room_config_slot_5 = character.accessory_2_item_id;
        packet.room_config_slot_6 = character.accessory_3_item_id;
        packets.push_back(packet);
    }

    std::sort(
        packets.begin(),
        packets.end(),
        [](const auto& left, const auto& right) { return left.character_id < right.character_id; });
    return packets;
}

std::vector<packets::s2c::UpdateItemList_3F> BuildItemListFromInventory(
    const core::AccountInventory& inventory) {
    std::vector<packets::s2c::UpdateItemList_3F> packets;
    std::unordered_set<std::uint32_t> seen_item_ids;

    packets.reserve(inventory.shared_item_stacks.size() + inventory.characters.size() * 8);

    for (const auto& stack : inventory.shared_item_stacks) {
        AppendInventoryOwnedItemId(packets, seen_item_ids, stack.item_id, stack.owned_count);
    }

    for (const auto& character : inventory.characters) {
        AppendInventoryOwnedItemId(packets, seen_item_ids, character.equipped_weapon_item_id, 1);
        AppendInventoryOwnedItemId(packets, seen_item_ids, character.clothes_item_id, 1);
        AppendInventoryOwnedItemId(packets, seen_item_ids, character.accessory_1_item_id, 1);
        AppendInventoryOwnedItemId(packets, seen_item_ids, character.accessory_2_item_id, 1);
        AppendInventoryOwnedItemId(packets, seen_item_ids, character.accessory_3_item_id, 1);

        for (const auto item_id : character.owned_weapon_item_ids) {
            AppendInventoryOwnedItemId(packets, seen_item_ids, item_id, 1);
        }
        for (const auto item_id : character.owned_clothes_item_ids) {
            AppendInventoryOwnedItemId(packets, seen_item_ids, item_id, 1);
        }
        for (const auto item_id : character.owned_accessory_1_item_ids) {
            AppendInventoryOwnedItemId(packets, seen_item_ids, item_id, 1);
        }
        for (const auto item_id : character.owned_accessory_2_item_ids) {
            AppendInventoryOwnedItemId(packets, seen_item_ids, item_id, 1);
        }
        for (const auto item_id : character.owned_accessory_3_item_ids) {
            AppendInventoryOwnedItemId(packets, seen_item_ids, item_id, 1);
        }
    }

    return packets;
}

std::vector<std::uint16_t> OwnedSkillIdsForCharacter(
    const core::GameDataCatalog& catalog,
    const core::AccountInventory& inventory,
    std::uint32_t character_id) {
    std::vector<std::uint16_t> skill_ids;

    for (const auto& skill : inventory.owned_skills) {
        if (skill.skill_item_id == 0 || skill.skill_item_id > 0xffffU) {
            continue;
        }

        const auto catalog_skill = catalog.find_skill(skill.skill_item_id);
        if (!catalog_skill || catalog_skill->character_id != character_id) {
            continue;
        }

        skill_ids.push_back(static_cast<std::uint16_t>(skill.skill_item_id));
    }

    std::sort(skill_ids.begin(), skill_ids.end());
    skill_ids.erase(std::unique(skill_ids.begin(), skill_ids.end()), skill_ids.end());
    return skill_ids;
}

std::uint16_t PurchaseCountFromBuyRequest(const packets::c2s::BuyItem_36& packet) {
    // The script-native buyitem(..., count, money_type) path uses `1` for a single purchase,
    // while the observed equipment-buy wire packet carries field_04 = 0. Treat the middle word
    // as a zero-based count so the etc/game-item shop can request stacked consumable buys.
    return static_cast<std::uint16_t>(packet.field_04 + 1u);
}

std::vector<packets::s2c::EnumQuickSlot_44> BuildQuickslotListFromInventoryItems(
    const core::GameDataCatalog& catalog,
    const core::AccountInventory& inventory) {
    constexpr std::uint8_t kBaseUnlockedSlots = 2;
    constexpr std::uint8_t kBagUnlockCap = 3;

    std::uint8_t bag_unlock_count = 0;

    for (const auto& stack : inventory.shared_item_stacks) {
        if (stack.item_id == 0 || stack.owned_count == 0) {
            continue;
        }

        if (IsBagLikeItem(catalog, stack.item_id)) {
            const auto next_count = static_cast<std::uint32_t>(bag_unlock_count) + stack.owned_count;
            bag_unlock_count = static_cast<std::uint8_t>(std::min<std::uint32_t>(next_count, kBagUnlockCap));
        }
    }

    const auto unlocked_slot_count =
        static_cast<std::uint8_t>(
            std::min<std::uint32_t>(kCurrentQuickslotCount, kBaseUnlockedSlots + bag_unlock_count));

    std::vector<packets::s2c::EnumQuickSlot_44> packets;
    packets.reserve(kCurrentQuickslotCount);
    for (std::uint8_t slot = 0; slot < kCurrentQuickslotCount; ++slot) {
        packets::s2c::EnumQuickSlot_44 packet;
        packet.slot_index = static_cast<std::uint8_t>(slot + 1);
        if (slot >= unlocked_slot_count) {
            packet.slot_state = 3;
        }
        packets.push_back(packet);
    }

    return packets;
}

bool HasSharedItem(const core::AccountInventory& inventory, std::uint32_t item_id) {
    return std::any_of(
        inventory.shared_item_stacks.begin(),
        inventory.shared_item_stacks.end(),
        [&](const auto& stack) { return stack.item_id == item_id && stack.owned_count != 0; });
}

bool MergeOwnedSkillIntoInventory(
    core::AccountInventory& inventory,
    std::uint32_t skill_item_id,
    std::uint32_t duration_minutes) {
    if (skill_item_id == 0) {
        return false;
    }

    for (auto& skill : inventory.owned_skills) {
        if (skill.skill_item_id != skill_item_id) {
            continue;
        }

        const bool changed = skill.duration_minutes != duration_minutes;
        skill.duration_minutes = duration_minutes;
        return changed;
    }

    inventory.owned_skills.push_back(core::OwnedSkill{skill_item_id, duration_minutes});
    std::sort(
        inventory.owned_skills.begin(),
        inventory.owned_skills.end(),
        [](const auto& left, const auto& right) { return left.skill_item_id < right.skill_item_id; });
    return true;
}

void NoteInventoryMatch(
    std::optional<InventoryEquipTarget>& target,
    bool& ambiguous,
    std::size_t character_index,
    std::uint32_t character_id,
    InventoryEquipSlot slot) {
    const InventoryEquipTarget candidate{character_index, character_id, slot};
    if (!target.has_value()) {
        target = candidate;
        return;
    }

    if (target->character_index != candidate.character_index || target->slot != candidate.slot) {
        ambiguous = true;
    }
}

std::optional<InventoryEquipTarget> FindInventoryEquipTarget(
    const core::AccountInventory& inventory,
    std::uint32_t item_id) {
    if (item_id == 0) {
        return std::nullopt;
    }

    std::optional<InventoryEquipTarget> target;
    bool ambiguous = false;

    for (std::size_t index = 0; index < inventory.characters.size(); ++index) {
        const auto& character = inventory.characters[index];
        if (character.equipped_weapon_item_id == item_id || ContainsItemId(character.owned_weapon_item_ids, item_id)) {
            NoteInventoryMatch(target, ambiguous, index, character.character_id, InventoryEquipSlot::kWeapon);
        }
        if (character.clothes_item_id == item_id || ContainsItemId(character.owned_clothes_item_ids, item_id)) {
            NoteInventoryMatch(target, ambiguous, index, character.character_id, InventoryEquipSlot::kClothes);
        }
        if (character.accessory_1_item_id == item_id || ContainsItemId(character.owned_accessory_1_item_ids, item_id)) {
            NoteInventoryMatch(target, ambiguous, index, character.character_id, InventoryEquipSlot::kAccessory1);
        }
        if (character.accessory_2_item_id == item_id || ContainsItemId(character.owned_accessory_2_item_ids, item_id)) {
            NoteInventoryMatch(target, ambiguous, index, character.character_id, InventoryEquipSlot::kAccessory2);
        }
        if (character.accessory_3_item_id == item_id || ContainsItemId(character.owned_accessory_3_item_ids, item_id)) {
            NoteInventoryMatch(target, ambiguous, index, character.character_id, InventoryEquipSlot::kAccessory3);
        }
    }

    if (ambiguous) {
        return std::nullopt;
    }
    return target;
}

bool MatchesKnownBundleSlot(
    const core::CharacterInventory& character,
    InventoryEquipSlot slot,
    std::uint32_t item_id) {
    if (item_id == 0) {
        return true;
    }

    switch (slot) {
    case InventoryEquipSlot::kWeapon:
        return character.equipped_weapon_item_id == item_id || ContainsItemId(character.owned_weapon_item_ids, item_id);
    case InventoryEquipSlot::kClothes:
        return character.clothes_item_id == item_id || ContainsItemId(character.owned_clothes_item_ids, item_id);
    case InventoryEquipSlot::kAccessory1:
        return character.accessory_1_item_id == item_id || ContainsItemId(character.owned_accessory_1_item_ids, item_id);
    case InventoryEquipSlot::kAccessory2:
        return character.accessory_2_item_id == item_id || ContainsItemId(character.owned_accessory_2_item_ids, item_id);
    case InventoryEquipSlot::kAccessory3:
        return character.accessory_3_item_id == item_id || ContainsItemId(character.owned_accessory_3_item_ids, item_id);
    }
    return false;
}

bool MatchesInventoryBundle(
    const core::CharacterInventory& character,
    const packets::c2s::EquipItem_48& packet) {
    return MatchesKnownBundleSlot(character, InventoryEquipSlot::kClothes, packet.field_04) &&
           MatchesKnownBundleSlot(character, InventoryEquipSlot::kWeapon, packet.field_0c) &&
           MatchesKnownBundleSlot(character, InventoryEquipSlot::kAccessory1, packet.field_10) &&
           MatchesKnownBundleSlot(character, InventoryEquipSlot::kAccessory2, packet.field_14) &&
           MatchesKnownBundleSlot(character, InventoryEquipSlot::kAccessory3, packet.field_18);
}

std::optional<InventoryEquipBundleTarget> FindInventoryBundleTarget(
    const core::AccountInventory& inventory,
    const packets::c2s::EquipItem_48& packet,
    std::optional<std::uint32_t> preferred_character_id) {
    if (preferred_character_id) {
        for (std::size_t index = 0; index < inventory.characters.size(); ++index) {
            const auto& character = inventory.characters[index];
            if (character.character_id == *preferred_character_id && MatchesInventoryBundle(character, packet)) {
                return InventoryEquipBundleTarget{index, character.character_id};
            }
        }
    }

    std::optional<InventoryEquipBundleTarget> target;
    bool ambiguous = false;
    for (std::size_t index = 0; index < inventory.characters.size(); ++index) {
        const auto& character = inventory.characters[index];
        if (!MatchesInventoryBundle(character, packet)) {
            continue;
        }

        const InventoryEquipBundleTarget candidate{index, character.character_id};
        if (!target) {
            target = candidate;
            continue;
        }

        if (target->character_index != candidate.character_index) {
            ambiguous = true;
        }
    }

    if (ambiguous) {
        return std::nullopt;
    }
    return target;
}

void ApplyEquippedItem(core::CharacterInventory& character, InventoryEquipSlot slot, std::uint32_t item_id) {
    switch (slot) {
    case InventoryEquipSlot::kWeapon: character.equipped_weapon_item_id = item_id; break;
    case InventoryEquipSlot::kClothes: character.clothes_item_id = item_id; break;
    case InventoryEquipSlot::kAccessory1: character.accessory_1_item_id = item_id; break;
    case InventoryEquipSlot::kAccessory2: character.accessory_2_item_id = item_id; break;
    case InventoryEquipSlot::kAccessory3: character.accessory_3_item_id = item_id; break;
    }
}

void ClearEquippedItem(core::CharacterInventory& character, InventoryEquipSlot slot) {
    ApplyEquippedItem(character, slot, 0);
}

bool ApplyKnownEquipmentBundle(core::CharacterInventory& character, const packets::c2s::EquipItem_48& packet) {
    const auto old_weapon = character.equipped_weapon_item_id;
    const auto old_clothes = character.clothes_item_id;
    const auto old_accessory_1 = character.accessory_1_item_id;
    const auto old_accessory_2 = character.accessory_2_item_id;
    const auto old_accessory_3 = character.accessory_3_item_id;

    character.clothes_item_id = packet.field_04;
    character.equipped_weapon_item_id = packet.field_0c;
    character.accessory_1_item_id = packet.field_10;
    character.accessory_2_item_id = packet.field_14;
    character.accessory_3_item_id = packet.field_18;

    return old_weapon != character.equipped_weapon_item_id || old_clothes != character.clothes_item_id ||
           old_accessory_1 != character.accessory_1_item_id || old_accessory_2 != character.accessory_2_item_id ||
           old_accessory_3 != character.accessory_3_item_id;
}

void ClearKnownEquipmentBundle(core::CharacterInventory& character) {
    character.equipped_weapon_item_id = 0;
    character.clothes_item_id = 0;
    character.accessory_1_item_id = 0;
    character.accessory_2_item_id = 0;
    character.accessory_3_item_id = 0;
}

std::string NormalizeCharacterInventoryKey(std::string_view text) {
    std::string normalized;
    normalized.reserve(text.size());
    for (const unsigned char ch : text) {
        if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z')) {
            normalized.push_back(static_cast<char>(ch));
        } else if (ch >= 'A' && ch <= 'Z') {
            normalized.push_back(static_cast<char>(ch - 'A' + 'a'));
        }
    }

    if (normalized == "kirious") {
        return "kirius";
    }
    return normalized;
}

bool AppendOwnedEquipmentItem(core::CharacterInventory& character, const core::ItemCatalogEntry& item) {
    auto append_unique = [&](std::vector<std::uint32_t>& values) {
        if (ContainsItemId(values, item.item_id)) {
            return false;
        }
        values.push_back(item.item_id);
        std::sort(values.begin(), values.end());
        return true;
    };

    if (item.item_type_name == "Weapon") {
        return append_unique(character.owned_weapon_item_ids);
    }
    if (item.item_type_name == "Main Clothes") {
        return append_unique(character.owned_clothes_item_ids);
    }
    if (item.item_type_name == "Accessories (1)") {
        return append_unique(character.owned_accessory_1_item_ids);
    }
    if (item.item_type_name == "Accessories (2)") {
        return append_unique(character.owned_accessory_2_item_ids);
    }
    if (item.item_type_name == "Accessories (3)") {
        return append_unique(character.owned_accessory_3_item_ids);
    }
    return false;
}

std::optional<std::size_t> FindInventoryCharacterIndexForItem(
    const core::GameDataCatalog& catalog,
    const core::AccountInventory& inventory,
    const core::ItemCatalogEntry& item) {
    const auto wanted_key = NormalizeCharacterInventoryKey(item.character_name);
    if (wanted_key.empty()) {
        return std::nullopt;
    }

    for (std::size_t index = 0; index < inventory.characters.size(); ++index) {
        const auto& character = inventory.characters[index];
        const auto character_name = catalog.character_name_for_id(character.character_id);
        if (!character_name) {
            continue;
        }
        if (NormalizeCharacterInventoryKey(*character_name) == wanted_key) {
            return index;
        }
    }
    return std::nullopt;
}

bool AppendSharedItemStack(core::AccountInventory& inventory, std::uint32_t item_id, std::uint16_t owned_count) {
    for (auto& stack : inventory.shared_item_stacks) {
        if (stack.item_id == item_id) {
            const auto next_count = static_cast<std::uint32_t>(stack.owned_count) + owned_count;
            stack.owned_count = static_cast<std::uint16_t>(std::min<std::uint32_t>(next_count, 0xffff));
            return true;
        }
    }

    inventory.shared_item_stacks.push_back(core::InventoryItemStack{item_id, owned_count});
    std::sort(
        inventory.shared_item_stacks.begin(),
        inventory.shared_item_stacks.end(),
        [](const auto& left, const auto& right) { return left.item_id < right.item_id; });
    return true;
}

bool ReplaceMisexpandedBaekhoSwimsuit(core::CharacterInventory& character) {
    if (character.character_id != 1) {
        return false;
    }

    const bool has_legacy_items =
        ContainsItemId(character.owned_clothes_item_ids, 8090) &&
        ContainsItemId(character.owned_accessory_1_item_ids, 8099) &&
        ContainsItemId(character.owned_accessory_2_item_ids, 8100) &&
        ContainsItemId(character.owned_accessory_3_item_ids, 8101);
    if (!has_legacy_items) {
        return false;
    }

    bool changed = false;
    auto erase_id = [](std::vector<std::uint32_t>& values, std::uint32_t item_id) {
        const auto original_size = values.size();
        values.erase(std::remove(values.begin(), values.end(), item_id), values.end());
        return values.size() != original_size;
    };
    changed = erase_id(character.owned_clothes_item_ids, 8090) || changed;
    changed = erase_id(character.owned_accessory_1_item_ids, 8099) || changed;
    changed = erase_id(character.owned_accessory_2_item_ids, 8100) || changed;
    changed = erase_id(character.owned_accessory_3_item_ids, 8101) || changed;

    auto append_unique_id = [](std::vector<std::uint32_t>& values, std::uint32_t item_id) {
        if (ContainsItemId(values, item_id)) {
            return false;
        }
        values.push_back(item_id);
        std::sort(values.begin(), values.end());
        return true;
    };

    changed = append_unique_id(character.owned_weapon_item_ids, 8007) || changed;
    changed = append_unique_id(character.owned_clothes_item_ids, 8000) || changed;
    changed = append_unique_id(character.owned_accessory_1_item_ids, 8004) || changed;
    changed = append_unique_id(character.owned_accessory_2_item_ids, 8005) || changed;
    changed = append_unique_id(character.owned_accessory_3_item_ids, 8006) || changed;
    return changed;
}

bool RepairMisexpandedPackages(core::AccountInventory& inventory) {
    bool changed = false;
    for (auto& character : inventory.characters) {
        changed = ReplaceMisexpandedBaekhoSwimsuit(character) || changed;
    }
    return changed;
}

bool EraseOwnedItemId(std::vector<std::uint32_t>& values, std::uint32_t item_id) {
    const auto original_size = values.size();
    values.erase(std::remove(values.begin(), values.end(), item_id), values.end());
    return values.size() != original_size;
}

bool RemoveSharedItemStack(core::AccountInventory& inventory, std::uint32_t item_id) {
    for (auto it = inventory.shared_item_stacks.begin(); it != inventory.shared_item_stacks.end(); ++it) {
        if (it->item_id != item_id) {
            continue;
        }

        if (it->owned_count > 1) {
            --it->owned_count;
        } else {
            inventory.shared_item_stacks.erase(it);
        }
        return true;
    }

    return false;
}

bool RemoveItemFromCharacterInventory(core::CharacterInventory& character, std::uint32_t item_id) {
    bool changed = false;

    if (character.equipped_weapon_item_id == item_id) {
        character.equipped_weapon_item_id = 0;
        changed = true;
    }
    if (character.clothes_item_id == item_id) {
        character.clothes_item_id = 0;
        changed = true;
    }
    if (character.accessory_1_item_id == item_id) {
        character.accessory_1_item_id = 0;
        changed = true;
    }
    if (character.accessory_2_item_id == item_id) {
        character.accessory_2_item_id = 0;
        changed = true;
    }
    if (character.accessory_3_item_id == item_id) {
        character.accessory_3_item_id = 0;
        changed = true;
    }

    changed = EraseOwnedItemId(character.owned_weapon_item_ids, item_id) || changed;
    changed = EraseOwnedItemId(character.owned_clothes_item_ids, item_id) || changed;
    changed = EraseOwnedItemId(character.owned_accessory_1_item_ids, item_id) || changed;
    changed = EraseOwnedItemId(character.owned_accessory_2_item_ids, item_id) || changed;
    changed = EraseOwnedItemId(character.owned_accessory_3_item_ids, item_id) || changed;

    return changed;
}

bool RemoveItemFromInventory(core::AccountInventory& inventory, std::uint32_t item_id) {
    bool changed = false;

    for (auto& character : inventory.characters) {
        changed = RemoveItemFromCharacterInventory(character, item_id) || changed;
    }

    if (changed) {
        return true;
    }

    return RemoveSharedItemStack(inventory, item_id);
}

bool ApplyPackageContentsToInventory(
    const core::GameDataCatalog& catalog,
    core::AccountInventory& inventory,
    std::uint32_t package_item_id,
    std::uint16_t package_count);

bool ApplyBoughtItemToInventory(
    const core::GameDataCatalog& catalog,
    core::AccountInventory& inventory,
    std::uint32_t item_id,
    std::uint16_t purchase_count) {
    if (!catalog.package_contents(item_id).empty()) {
        return ApplyPackageContentsToInventory(catalog, inventory, item_id, purchase_count);
    }

    const auto item = catalog.find_item(item_id);
    if (!item) {
        return AppendSharedItemStack(inventory, item_id, purchase_count);
    }

    if (!item->equipment_like) {
        return AppendSharedItemStack(inventory, item_id, purchase_count);
    }

    const auto character_index = FindInventoryCharacterIndexForItem(catalog, inventory, *item);
    if (!character_index) {
        return AppendSharedItemStack(inventory, item_id, purchase_count);
    }

    return AppendOwnedEquipmentItem(inventory.characters[*character_index], *item);
}

bool ApplyPackageContentsToInventory(
    const core::GameDataCatalog& catalog,
    core::AccountInventory& inventory,
    std::uint32_t package_item_id,
    std::uint16_t package_count) {
    bool changed = false;
    const auto contents = catalog.package_contents(package_item_id);
    const auto count = std::max<std::uint16_t>(package_count, 1);
    for (std::uint16_t index = 0; index < count; ++index) {
        for (const auto content_item_id : contents) {
            changed = ApplyBoughtItemToInventory(catalog, inventory, content_item_id, 1) || changed;
        }
    }
    return changed;
}

bool ExpandStoredPackageTokens(
    const core::GameDataCatalog& catalog,
    core::AccountInventory& inventory,
    std::vector<std::uint32_t>& expanded_package_ids) {
    bool changed = false;
    for (auto it = inventory.shared_item_stacks.begin(); it != inventory.shared_item_stacks.end();) {
        if (catalog.package_contents(it->item_id).empty()) {
            ++it;
            continue;
        }

        const auto package_item_id = it->item_id;
        const auto package_count = it->owned_count;
        it = inventory.shared_item_stacks.erase(it);
        expanded_package_ids.push_back(package_item_id);
        changed = ApplyPackageContentsToInventory(catalog, inventory, package_item_id, package_count) || changed;
        changed = true;
    }
    return changed;
}

bool ApplyShopPriceToAccountInfo(
    packets::s2c::UpdateAccountInfo_70& account_info,
    const core::ItemCatalogEntry& item,
    std::uint8_t buy_money_selection,
    std::uint16_t purchase_count) {
    const auto purchase_count_u64 = static_cast<std::uint64_t>(std::max<std::uint16_t>(purchase_count, 1));
    const auto pay_luna = [&]() {
        if (item.luna_price == 0) {
            return false;
        }
        const auto total_price = static_cast<std::uint64_t>(item.luna_price) * purchase_count_u64;
        if (account_info.luna < total_price) {
            return false;
        }
        account_info.luna -= static_cast<std::uint32_t>(total_price);
        return true;
    };
    const auto pay_cash = [&]() {
        if (item.cash_price == 0) {
            return false;
        }
        const auto total_price = static_cast<std::uint64_t>(item.cash_price) * purchase_count_u64;
        if (account_info.cash < total_price) {
            return false;
        }
        account_info.cash -= static_cast<std::uint32_t>(total_price);
        return true;
    };

    if (item.luna_price != 0 && item.cash_price == 0) {
        return pay_luna();
    }
    if (item.cash_price != 0 && item.luna_price == 0) {
        return pay_cash();
    }

    switch (buy_money_selection) {
    case kShopSelectionLuna: return pay_luna();
    case kShopSelectionCash: return pay_cash();
    default: return false;
    }
}

bool ApplyShopPriceToAccountInfo(
    packets::s2c::UpdateAccountInfo_70& account_info,
    const core::SkillCatalogEntry& skill,
    std::uint8_t buy_money_selection) {
    const auto pay_luna = [&]() {
        if (skill.luna_price == 0 || account_info.luna < skill.luna_price) {
            return false;
        }
        account_info.luna -= skill.luna_price;
        return true;
    };
    const auto pay_cash = [&]() {
        if (skill.cash_price == 0 || account_info.cash < skill.cash_price) {
            return false;
        }
        account_info.cash -= skill.cash_price;
        return true;
    };

    if (skill.luna_price != 0 && skill.cash_price == 0) {
        return pay_luna();
    }
    if (skill.cash_price != 0 && skill.luna_price == 0) {
        return pay_cash();
    }

    switch (buy_money_selection) {
    case kShopSelectionLuna: return pay_luna();
    case kShopSelectionCash: return pay_cash();
    default: return false;
    }
}

std::string_view EquipSlotName(InventoryEquipSlot slot) {
    switch (slot) {
    case InventoryEquipSlot::kWeapon: return "weapon";
    case InventoryEquipSlot::kClothes: return "clothes";
    case InventoryEquipSlot::kAccessory1: return "accessory_1";
    case InventoryEquipSlot::kAccessory2: return "accessory_2";
    case InventoryEquipSlot::kAccessory3: return "accessory_3";
    }
    return "unknown";
}

}  // namespace cpp_server::game
