#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cpp_server/packets/s2c/EnumQuickSlot_44.h"
#include "cpp_server/packets/s2c/UpdateItemList_3F.h"
#include "cpp_server/packets/s2c/UpdateAccountInfo_70.h"
#include "cpp_server/packets/s2c/UpdateCharacterList_6B.h"
#include "cpp_server/packets/s2c/UpdateGuardList_6E.h"
#include "cpp_server/packets/s2c/UpdateSkillList_73.h"

namespace cpp_server::core {

struct InventoryItemStack {
    std::uint32_t item_id{};
    std::uint16_t owned_count{1};
};

struct OwnedSkill {
    std::uint32_t skill_item_id{};
    std::uint32_t duration_minutes{};
};

struct CharacterInventory {
    std::uint32_t character_id{};
    std::uint32_t equipped_weapon_item_id{};
    std::uint32_t clothes_item_id{};
    std::uint32_t accessory_1_item_id{};
    std::uint32_t accessory_2_item_id{};
    std::uint32_t accessory_3_item_id{};
    std::vector<std::uint32_t> owned_weapon_item_ids{};
    std::vector<std::uint32_t> owned_clothes_item_ids{};
    std::vector<std::uint32_t> owned_accessory_1_item_ids{};
    std::vector<std::uint32_t> owned_accessory_2_item_ids{};
    std::vector<std::uint32_t> owned_accessory_3_item_ids{};
};

struct AccountInventory {
    std::vector<InventoryItemStack> shared_item_stacks{};
    std::vector<OwnedSkill> owned_skills{};
    std::vector<CharacterInventory> characters{};
};

struct AccountPacketProfile {
    std::optional<packets::s2c::UpdateAccountInfo_70> account_info{};
    std::optional<std::vector<packets::s2c::UpdateCharacterList_6B>> character_list{};
    std::optional<std::vector<packets::s2c::UpdateItemList_3F>> item_list{};
    std::optional<std::vector<packets::s2c::EnumQuickSlot_44>> quickslot_list{};
    std::optional<std::vector<packets::s2c::UpdateSkillList_73>> skill_list{};
    std::optional<std::vector<packets::s2c::UpdateGuardList_6E>> guard_list{};
    std::optional<AccountInventory> inventory{};
};

struct AccountRecord {
    std::string login_id{};
    std::string password{};
    std::optional<std::string> nickname{};
    AccountPacketProfile packet_profile{};
};

enum class ResolveCharacterNameResult {
    kResolved,
    kAccountNotFound,
    kInvalidName,
    kNameInUse,
};

class AccountDatabase {
public:
    explicit AccountDatabase(std::filesystem::path path);

    [[nodiscard]] std::optional<AccountRecord> authenticate(
        std::string_view login_id,
        std::string_view password) const;
    [[nodiscard]] std::optional<AccountRecord> find_by_login(std::string_view login_id) const;
    [[nodiscard]] ResolveCharacterNameResult resolve_character_name(
        std::string_view login_id,
        std::string_view requested_name,
        std::string& effective_name);
    [[nodiscard]] std::optional<AccountPacketProfile> find_packet_profile(std::string_view login_id) const;
    [[nodiscard]] std::optional<packets::s2c::UpdateAccountInfo_70> find_account_info(std::string_view login_id) const;
    [[nodiscard]] std::optional<std::vector<packets::s2c::UpdateCharacterList_6B>> find_character_list(
        std::string_view login_id) const;
    [[nodiscard]] std::optional<std::vector<packets::s2c::UpdateItemList_3F>> find_item_list(
        std::string_view login_id) const;
    [[nodiscard]] std::optional<std::vector<packets::s2c::EnumQuickSlot_44>> find_quickslot_list(
        std::string_view login_id) const;
    [[nodiscard]] std::optional<std::vector<packets::s2c::UpdateSkillList_73>> find_skill_list(
        std::string_view login_id) const;
    [[nodiscard]] std::optional<std::vector<packets::s2c::UpdateGuardList_6E>> find_guard_list(
        std::string_view login_id) const;
    [[nodiscard]] std::optional<AccountInventory> find_inventory(std::string_view login_id) const;

    bool set_packet_profile(std::string_view login_id, AccountPacketProfile profile);
    bool set_account_info(
        std::string_view login_id,
        std::optional<packets::s2c::UpdateAccountInfo_70> account_info);
    bool set_character_list(
        std::string_view login_id,
        std::optional<std::vector<packets::s2c::UpdateCharacterList_6B>> character_list);
    bool set_item_list(
        std::string_view login_id,
        std::optional<std::vector<packets::s2c::UpdateItemList_3F>> item_list);
    bool set_quickslot_list(
        std::string_view login_id,
        std::optional<std::vector<packets::s2c::EnumQuickSlot_44>> quickslot_list);
    bool set_skill_list(
        std::string_view login_id,
        std::optional<std::vector<packets::s2c::UpdateSkillList_73>> skill_list);
    bool set_guard_list(
        std::string_view login_id,
        std::optional<std::vector<packets::s2c::UpdateGuardList_6E>> guard_list);
    bool set_inventory(
        std::string_view login_id,
        std::optional<AccountInventory> inventory);

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    void ensure_loaded_locked() const;
    void load_locked() const;
    void store_locked() const;

    [[nodiscard]] static bool is_valid_character_name(std::string_view name);

    std::filesystem::path path_{};
    mutable std::mutex mutex_{};
    mutable bool loaded_{false};
    mutable std::optional<std::filesystem::file_time_type> cached_mtime_{};
    mutable std::vector<AccountRecord> accounts_{};
};

}  // namespace cpp_server::core
