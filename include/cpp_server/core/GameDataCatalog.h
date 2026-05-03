#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace cpp_server::core {

struct SkillCatalogEntry {
    std::uint32_t skill_id{};
    std::uint32_t base_skill_id{};
    std::uint32_t character_id{};
    std::optional<std::uint32_t> skill_order{};
    std::uint16_t icon_id{};
    std::uint32_t luna_price{};
    std::uint32_t cash_price{};
    bool equipable{};
    std::uint32_t cooltime_ms{};
    std::uint32_t duration_minutes{};
    std::size_t source_index{};
};

struct ItemCatalogEntry {
    std::uint32_t item_id{};
    std::string display_name{};
    std::string character_name{};
    std::string item_type_name{};
    std::string equip_slot_name{};
    std::uint32_t luna_price{};
    std::uint32_t cash_price{};
    bool equipment_like{};
    std::size_t source_index{};
};

struct MissionSpawnPosition {
    float x{};
    float y{};
    float z{};
};

struct MissionEntityDefinition {
    std::string role{};
    std::uint16_t entity_object_id{};
    std::uint8_t field_06{};
    std::optional<std::uint8_t> category{};
    std::array<std::uint8_t, 4> state_bytes{};
    std::array<std::uint8_t, 6> appearance_bytes{};
    MissionSpawnPosition position{};
    std::string resource_key{};
    std::string display_name{};
    std::string group_name{};
    std::size_t source_index{};
};

struct MissionCatalogEntry {
    std::uint32_t mission_rule_id{};
    std::string mission_ui_key{};
    std::string scene_key{};
    std::optional<MissionSpawnPosition> player_spawn_position{};
    std::string english_room_title{};
    std::uint8_t max_players{};
    std::uint32_t time_limit_seconds{};
    std::vector<MissionEntityDefinition> entity_definitions{};
    std::size_t source_index{};
};

class GameDataCatalog {
public:
    explicit GameDataCatalog(std::filesystem::path anchor_path);

    [[nodiscard]] bool available() const;
    [[nodiscard]] std::vector<SkillCatalogEntry> skills_for_character(std::uint32_t character_id) const;
    [[nodiscard]] std::optional<SkillCatalogEntry> find_skill(std::uint32_t skill_id) const;
    [[nodiscard]] std::vector<ItemCatalogEntry> starter_items() const;
    [[nodiscard]] std::optional<ItemCatalogEntry> find_item(std::uint32_t item_id) const;
    [[nodiscard]] std::vector<std::uint32_t> package_contents(std::uint32_t package_item_id) const;
    [[nodiscard]] std::optional<std::string> character_name_for_id(std::uint32_t character_id) const;
    [[nodiscard]] std::optional<std::string> character_asset_key_for_id(std::uint32_t character_id) const;
    [[nodiscard]] std::vector<ItemCatalogEntry> equipment_items_for_character(std::string_view character_name) const;
    [[nodiscard]] std::optional<MissionCatalogEntry> find_mission(std::uint32_t mission_rule_id) const;
    [[nodiscard]] const std::filesystem::path& skill_catalog_path() const;

private:
    void ensure_loaded() const;
    void load() const;

    [[nodiscard]] static std::filesystem::path resolve_skill_catalog_path(const std::filesystem::path& anchor_path);
    [[nodiscard]] static std::filesystem::path resolve_item_catalog_path(const std::filesystem::path& anchor_path);
    [[nodiscard]] static std::filesystem::path resolve_item_metadata_path(const std::filesystem::path& anchor_path);
    [[nodiscard]] static std::filesystem::path resolve_character_metadata_path(const std::filesystem::path& anchor_path);
    [[nodiscard]] static std::filesystem::path resolve_package_contents_path(const std::filesystem::path& anchor_path);
    [[nodiscard]] static std::filesystem::path resolve_mission_catalog_path(const std::filesystem::path& anchor_path);

    std::filesystem::path anchor_path_{};
    mutable std::mutex mutex_{};
    mutable bool loaded_{false};
    mutable std::filesystem::path skill_catalog_path_{};
    mutable std::unordered_map<std::uint32_t, std::vector<SkillCatalogEntry>> skills_by_character_{};
    mutable std::unordered_map<std::uint32_t, SkillCatalogEntry> skills_by_id_{};
    mutable std::filesystem::path item_catalog_path_{};
    mutable std::vector<ItemCatalogEntry> starter_items_{};
    mutable std::filesystem::path item_metadata_path_{};
    mutable std::unordered_map<std::uint32_t, ItemCatalogEntry> items_by_id_{};
    mutable std::filesystem::path package_contents_path_{};
    mutable std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> package_contents_by_id_{};
    mutable std::filesystem::path character_metadata_path_{};
    mutable std::unordered_map<std::uint32_t, std::string> character_names_by_id_{};
    mutable std::unordered_map<std::uint32_t, std::string> character_asset_keys_by_id_{};
    mutable std::unordered_map<std::string, std::vector<ItemCatalogEntry>> equipment_items_by_character_{};
    mutable std::filesystem::path mission_catalog_path_{};
    mutable std::unordered_map<std::uint32_t, MissionCatalogEntry> missions_by_rule_id_{};
};

}  // namespace cpp_server::core
