#include "cpp_server/core/GameDataCatalog.h"

#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>

namespace cpp_server::core {

namespace {

bool is_decimal_number(std::string_view text) {
    if (text.empty()) {
        return false;
    }

    for (const unsigned char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
    }
    return true;
}

std::optional<std::uint32_t> parse_optional_u32(std::string_view text) {
    if (!is_decimal_number(text)) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(std::stoul(std::string(text), nullptr, 10));
}

std::uint32_t parse_preferred_price(
    const std::vector<std::string_view>& columns,
    std::initializer_list<std::size_t> candidate_indexes) {
    for (const auto index : candidate_indexes) {
        if (index >= columns.size()) {
            continue;
        }

        const auto parsed = parse_optional_u32(columns[index]).value_or(0);
        if (parsed != 0) {
            return parsed;
        }
    }

    return 0;
}

std::uint32_t canonical_skill_id(std::uint32_t skill_id) {
    // Observed skill shop rows use ids like 110314 / 120314 / 130314 / 140314
    // for the same owned skill family as base row 314.
    return skill_id >= 100000 ? (skill_id % 10000) : skill_id;
}

std::string ascii_lowercase(std::string_view text) {
    std::string lowered;
    lowered.reserve(text.size());
    for (const unsigned char ch : text) {
        if (ch >= 'A' && ch <= 'Z') {
            lowered.push_back(static_cast<char>(ch - 'A' + 'a'));
        } else {
            lowered.push_back(static_cast<char>(ch));
        }
    }
    return lowered;
}

std::uint32_t parse_duration_minutes_from_text(std::string_view text) {
    if (text.empty()) {
        return 0;
    }

    const auto lowered = ascii_lowercase(text);
    if (lowered.find("permanent") != std::string::npos || text.find("영구") != std::string_view::npos) {
        return 0;
    }

    std::string digits;
    for (const unsigned char ch : text) {
        if (ch >= '0' && ch <= '9') {
            digits.push_back(static_cast<char>(ch));
        }
    }

    if (digits.empty()) {
        return 0;
    }

    const auto value = static_cast<std::uint32_t>(std::stoul(digits, nullptr, 10));
    if (lowered.find("day") != std::string::npos || text.find("일") != std::string_view::npos) {
        return value * 24u * 60u;
    }

    return 0;
}

std::uint32_t parse_skill_duration_minutes(const std::vector<std::string_view>& columns) {
    if (columns.size() > 51) {
        const auto parsed = parse_duration_minutes_from_text(columns[51]);
        if (parsed != 0 || ascii_lowercase(columns[51]).find("permanent") != std::string::npos) {
            return parsed;
        }
    }

    if (columns.size() > 41) {
        return parse_duration_minutes_from_text(columns[41]);
    }

    return 0;
}

std::optional<std::uint8_t> parse_optional_u8(std::string_view text) {
    const auto parsed = parse_optional_u32(text);
    if (!parsed || *parsed > 0xff) {
        return std::nullopt;
    }
    return static_cast<std::uint8_t>(*parsed);
}

std::optional<std::uint16_t> parse_optional_u16(std::string_view text) {
    const auto parsed = parse_optional_u32(text);
    if (!parsed || *parsed > 0xffff) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(*parsed);
}

std::optional<float> parse_optional_f32(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }

    try {
        std::size_t consumed = 0;
        const auto value = std::stof(std::string(text), &consumed);
        return consumed == text.size() ? std::optional<float>{value} : std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

std::string_view trim_ascii_whitespace(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) {
        text.remove_suffix(1);
    }
    return text;
}

std::vector<std::string_view> split_tab_separated_line(std::string_view line) {
    std::vector<std::string_view> columns;
    std::size_t start = 0;
    while (start <= line.size()) {
        const auto next = line.find('\t', start);
        if (next == std::string_view::npos) {
            columns.push_back(line.substr(start));
            break;
        }
        columns.push_back(line.substr(start, next - start));
        start = next + 1;
    }
    return columns;
}

std::vector<std::string_view> split_comma_separated_line(std::string_view line) {
    std::vector<std::string_view> columns;
    std::size_t start = 0;
    while (start <= line.size()) {
        const auto next = line.find(',', start);
        if (next == std::string_view::npos) {
            columns.push_back(line.substr(start));
            break;
        }
        columns.push_back(line.substr(start, next - start));
        start = next + 1;
    }
    return columns;
}

std::filesystem::path resolve_preferred_path(
    const std::filesystem::path& anchor_path,
    const std::filesystem::path& local_relative,
    const std::filesystem::path& legacy_relative);

std::optional<std::string> read_text_file(const std::filesystem::path& path);

std::filesystem::path resolve_mission_layout_path(
    const std::filesystem::path& anchor_path,
    std::string_view scene_key) {
    if (scene_key.empty()) {
        return {};
    }

    const auto relative = std::filesystem::path("data/missions") / (std::string(scene_key) + ".csv");
    return resolve_preferred_path(anchor_path, relative, std::filesystem::path("CPP_Server") / relative);
}

std::vector<MissionEntityDefinition> load_mission_entity_definitions(
    const std::filesystem::path& anchor_path,
    std::string_view scene_key) {
    std::vector<MissionEntityDefinition> entities;

    const auto path = resolve_mission_layout_path(anchor_path, scene_key);
    if (path.empty()) {
        return entities;
    }

    const auto content = read_text_file(path);
    if (!content) {
        return entities;
    }

    std::size_t line_start = 0;
    bool skipped_header = false;
    std::size_t source_index = 0;
    while (line_start <= content->size()) {
        const auto line_end = content->find('\n', line_start);
        std::string_view line(
            content->data() + line_start,
            (line_end == std::string::npos ? content->size() : line_end) - line_start);
        line = trim_ascii_whitespace(line);

        if (line.empty() || line.front() == '#') {
            // Keep comments in the data files for evidence without making them part of the parser contract.
        } else if (!skipped_header) {
            skipped_header = true;
        } else {
            const auto columns = split_comma_separated_line(line);
            if (columns.size() >= 18) {
                MissionEntityDefinition entry;
                entry.role = std::string(trim_ascii_whitespace(columns[0]));
                entry.entity_object_id = parse_optional_u16(trim_ascii_whitespace(columns[1])).value_or(0);
                entry.field_06 = parse_optional_u8(trim_ascii_whitespace(columns[2])).value_or(0);
                entry.category = parse_optional_u8(trim_ascii_whitespace(columns[3]));
                entry.state_bytes = {
                    parse_optional_u8(trim_ascii_whitespace(columns[4])).value_or(0),
                    parse_optional_u8(trim_ascii_whitespace(columns[5])).value_or(0),
                    parse_optional_u8(trim_ascii_whitespace(columns[6])).value_or(0),
                    parse_optional_u8(trim_ascii_whitespace(columns[7])).value_or(0)};
                entry.appearance_bytes = {
                    parse_optional_u8(trim_ascii_whitespace(columns[8])).value_or(0),
                    parse_optional_u8(trim_ascii_whitespace(columns[9])).value_or(0),
                    parse_optional_u8(trim_ascii_whitespace(columns[10])).value_or(0),
                    parse_optional_u8(trim_ascii_whitespace(columns[11])).value_or(0),
                    parse_optional_u8(trim_ascii_whitespace(columns[12])).value_or(0),
                    parse_optional_u8(trim_ascii_whitespace(columns[13])).value_or(0)};

                const auto x = parse_optional_f32(trim_ascii_whitespace(columns[14]));
                const auto y = parse_optional_f32(trim_ascii_whitespace(columns[15]));
                const auto z = parse_optional_f32(trim_ascii_whitespace(columns[16]));
                entry.resource_key = std::string(trim_ascii_whitespace(columns[17]));
                if (columns.size() > 18) {
                    entry.display_name = std::string(trim_ascii_whitespace(columns[18]));
                }
                if (columns.size() > 19) {
                    entry.group_name = std::string(trim_ascii_whitespace(columns[19]));
                }

                if (entry.entity_object_id != 0 && x && y && z && !entry.resource_key.empty()) {
                    entry.position = MissionSpawnPosition{*x, *y, *z};
                    entry.source_index = source_index++;
                    entities.push_back(std::move(entry));
                }
            }
        }

        if (line_end == std::string::npos) {
            break;
        }
        line_start = line_end + 1;
    }

    return entities;
}

std::optional<std::size_t> find_header_column(
    const std::vector<std::string_view>& columns,
    std::string_view header_name) {
    for (std::size_t index = 0; index < columns.size(); ++index) {
        if (columns[index] == header_name) {
            return index;
        }
    }
    return std::nullopt;
}

std::string confirmed_mission_scene_key(std::uint32_t mission_rule_id) {
    // Confirmed by setting/mission.csv, setting/infinity.xml, and script/game.script.txt.
    // Keep this table evidence-based; it is intentionally not derived from display text.
    switch (mission_rule_id) {
    case 10000: return "mission_hakansexam";
    case 10001: return "mission_0_2";
    case 10002: return "mission_0_3";
    case 10004: return "mission_0_5";
    case 10005: return "mission_0_6";
    case 10006: return "mission_0_7";
    case 10007: return "mission_0_8";
    case 100: return "mission01";
    default: return {};
    }
}

std::optional<MissionSpawnPosition> confirmed_mission_spawn_position(std::uint32_t mission_rule_id) {
    // Confirmed by setting/infinity.xml map spawn rows. These positions are the closest
    // current evidence for the server-authored UDP 0x03 entity coordinates.
    switch (mission_rule_id) {
    case 10005: return MissionSpawnPosition{397.8F, 0.0F, -126.9F};
    default: return std::nullopt;
    }
}

std::string normalize_character_key(std::string_view text) {
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

bool is_equipment_category(std::string_view english_type_name) {
    return english_type_name == "Weapon" || english_type_name == "Main Clothes" ||
           english_type_name == "Accessories (1)" || english_type_name == "Accessories (2)" ||
           english_type_name == "Accessories (3)";
}

std::string decode_text_bytes(const std::string& raw_bytes) {
    if (raw_bytes.size() >= 2) {
        const auto bom0 = static_cast<unsigned char>(raw_bytes[0]);
        const auto bom1 = static_cast<unsigned char>(raw_bytes[1]);

        if (bom0 == 0xFF && bom1 == 0xFE) {
            std::string decoded;
            decoded.reserve(raw_bytes.size() / 2);
            for (std::size_t i = 2; i + 1 < raw_bytes.size(); i += 2) {
                const std::uint16_t code_unit = static_cast<std::uint16_t>(
                    static_cast<unsigned char>(raw_bytes[i]) |
                    (static_cast<std::uint16_t>(static_cast<unsigned char>(raw_bytes[i + 1])) << 8U));
                decoded.push_back(code_unit <= 0x7f ? static_cast<char>(code_unit) : '?');
            }
            return decoded;
        }

        if (bom0 == 0xFE && bom1 == 0xFF) {
            std::string decoded;
            decoded.reserve(raw_bytes.size() / 2);
            for (std::size_t i = 2; i + 1 < raw_bytes.size(); i += 2) {
                const std::uint16_t code_unit = static_cast<std::uint16_t>(
                    (static_cast<std::uint16_t>(static_cast<unsigned char>(raw_bytes[i])) << 8U) |
                    static_cast<unsigned char>(raw_bytes[i + 1]));
                decoded.push_back(code_unit <= 0x7f ? static_cast<char>(code_unit) : '?');
            }
            return decoded;
        }
    }

    return raw_bytes;
}

std::optional<std::string> read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }

    std::ostringstream raw;
    raw << input.rdbuf();
    return decode_text_bytes(raw.str());
}

std::filesystem::path normalize_anchor_directory(const std::filesystem::path& anchor_path) {
    if (anchor_path.empty()) {
        return std::filesystem::current_path();
    }
    if (std::filesystem::is_directory(anchor_path)) {
        return anchor_path;
    }
    return anchor_path.parent_path();
}

std::filesystem::path resolve_preferred_path(
    const std::filesystem::path& anchor_path,
    const std::filesystem::path& local_relative,
    const std::filesystem::path& legacy_relative) {
    auto current = normalize_anchor_directory(anchor_path);

    for (int depth = 0; depth < 10; ++depth) {
        const auto local_candidate = current / local_relative;
        if (std::filesystem::exists(local_candidate)) {
            return std::filesystem::weakly_canonical(local_candidate);
        }

        const auto legacy_candidate = current / legacy_relative;
        if (std::filesystem::exists(legacy_candidate)) {
            return std::filesystem::weakly_canonical(legacy_candidate);
        }

        if (!current.has_parent_path()) {
            break;
        }
        current = current.parent_path();
    }

    return {};
}

}  // namespace

GameDataCatalog::GameDataCatalog(std::filesystem::path anchor_path) : anchor_path_(std::move(anchor_path)) {}

bool GameDataCatalog::available() const {
    std::scoped_lock lock(mutex_);
    ensure_loaded();
    return !skill_catalog_path_.empty() || !item_catalog_path_.empty() || !item_metadata_path_.empty();
}

std::vector<SkillCatalogEntry> GameDataCatalog::skills_for_character(std::uint32_t character_id) const {
    std::scoped_lock lock(mutex_);
    ensure_loaded();

    const auto it = skills_by_character_.find(character_id);
    if (it == skills_by_character_.end()) {
        return {};
    }
    return it->second;
}

std::optional<SkillCatalogEntry> GameDataCatalog::find_skill(std::uint32_t skill_id) const {
    std::scoped_lock lock(mutex_);
    ensure_loaded();

    const auto it = skills_by_id_.find(skill_id);
    if (it == skills_by_id_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<ItemCatalogEntry> GameDataCatalog::starter_items() const {
    std::scoped_lock lock(mutex_);
    ensure_loaded();
    return starter_items_;
}

std::optional<ItemCatalogEntry> GameDataCatalog::find_item(std::uint32_t item_id) const {
    std::scoped_lock lock(mutex_);
    ensure_loaded();

    const auto it = items_by_id_.find(item_id);
    if (it == items_by_id_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<std::uint32_t> GameDataCatalog::package_contents(std::uint32_t package_item_id) const {
    std::scoped_lock lock(mutex_);
    ensure_loaded();

    const auto it = package_contents_by_id_.find(package_item_id);
    if (it == package_contents_by_id_.end()) {
        return {};
    }
    return it->second;
}

std::optional<std::string> GameDataCatalog::character_name_for_id(std::uint32_t character_id) const {
    std::scoped_lock lock(mutex_);
    ensure_loaded();

    const auto it = character_names_by_id_.find(character_id);
    if (it == character_names_by_id_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<std::string> GameDataCatalog::character_asset_key_for_id(std::uint32_t character_id) const {
    std::scoped_lock lock(mutex_);
    ensure_loaded();

    const auto it = character_asset_keys_by_id_.find(character_id);
    if (it == character_asset_keys_by_id_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<MissionCatalogEntry> GameDataCatalog::find_mission(std::uint32_t mission_rule_id) const {
    std::scoped_lock lock(mutex_);
    ensure_loaded();

    const auto it = missions_by_rule_id_.find(mission_rule_id);
    if (it == missions_by_rule_id_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<ItemCatalogEntry> GameDataCatalog::equipment_items_for_character(std::string_view character_name) const {
    std::scoped_lock lock(mutex_);
    ensure_loaded();

    const auto it = equipment_items_by_character_.find(normalize_character_key(character_name));
    if (it == equipment_items_by_character_.end()) {
        return {};
    }
    return it->second;
}

const std::filesystem::path& GameDataCatalog::skill_catalog_path() const {
    std::scoped_lock lock(mutex_);
    ensure_loaded();
    return skill_catalog_path_;
}

void GameDataCatalog::ensure_loaded() const {
    if (!loaded_) {
        load();
        loaded_ = true;
    }
}

void GameDataCatalog::load() const {
    skills_by_character_.clear();
    skills_by_id_.clear();
    starter_items_.clear();
    items_by_id_.clear();
    package_contents_by_id_.clear();
    character_names_by_id_.clear();
    character_asset_keys_by_id_.clear();
    equipment_items_by_character_.clear();
    missions_by_rule_id_.clear();
    skill_catalog_path_ = resolve_skill_catalog_path(anchor_path_);
    item_catalog_path_ = resolve_item_catalog_path(anchor_path_);
    item_metadata_path_ = resolve_item_metadata_path(anchor_path_);
    package_contents_path_ = resolve_package_contents_path(anchor_path_);
    character_metadata_path_ = resolve_character_metadata_path(anchor_path_);
    mission_catalog_path_ = resolve_mission_catalog_path(anchor_path_);

    if (!skill_catalog_path_.empty()) {
        const auto content = read_text_file(skill_catalog_path_);
        if (!content) {
            skill_catalog_path_.clear();
        } else {
            std::unordered_map<std::uint32_t, std::unordered_set<std::uint32_t>> seen_skill_ids;
            std::size_t source_index = 0;
            std::size_t line_start = 0;
            bool skipped_header = false;

            while (line_start <= content->size()) {
                const auto line_end = content->find('\n', line_start);
                std::string_view line(
                    content->data() + line_start,
                    (line_end == std::string::npos ? content->size() : line_end) - line_start);

                if (!line.empty() && line.back() == '\r') {
                    line.remove_suffix(1);
                }

                if (!skipped_header) {
                    skipped_header = true;
                } else if (!line.empty()) {
                    const auto columns = split_tab_separated_line(line);
                    if (columns.size() >= 38 && is_decimal_number(columns[0]) && is_decimal_number(columns[2])) {
                        const auto skill_id =
                            static_cast<std::uint32_t>(std::stoul(std::string(columns[0]), nullptr, 10));
                        const auto character_id =
                            static_cast<std::uint32_t>(std::stoul(std::string(columns[2]), nullptr, 10));

                        SkillCatalogEntry entry;
                        entry.skill_id = skill_id;
                        entry.base_skill_id = canonical_skill_id(skill_id);
                        entry.character_id = character_id;
                        entry.skill_order = parse_optional_u32(columns[16]);
                        entry.icon_id = static_cast<std::uint16_t>(parse_optional_u32(columns[5]).value_or(0));
                        entry.cash_price = parse_preferred_price(columns, {3, 29, 31, 33});
                        entry.luna_price = parse_preferred_price(columns, {4, 30, 32, 34});
                        entry.equipable = parse_optional_u32(columns[36]).value_or(0) != 0;
                        entry.cooltime_ms = parse_optional_u32(columns[37]).value_or(0);
                        entry.duration_minutes = parse_skill_duration_minutes(columns);
                        entry.source_index = source_index++;

                        skills_by_id_[entry.skill_id] = entry;

                        // The shipped tables include rental/variant rows like 110001/120001 in addition to the base ids.
                        if (entry.base_skill_id == entry.skill_id &&
                            seen_skill_ids[character_id].insert(entry.base_skill_id).second) {
                            skills_by_character_[character_id].push_back(entry);
                        }
                    }
                }

                if (line_end == std::string::npos) {
                    break;
                }
                line_start = line_end + 1;
            }
        }
    }

    if (!item_catalog_path_.empty()) {
        const auto content = read_text_file(item_catalog_path_);
        if (!content) {
            item_catalog_path_.clear();
        } else {
            std::size_t line_start = 0;
            bool skipped_header = false;
            while (line_start <= content->size()) {
                const auto line_end = content->find('\n', line_start);
                std::string_view line(
                    content->data() + line_start,
                    (line_end == std::string::npos ? content->size() : line_end) - line_start);

                if (!line.empty() && line.back() == '\r') {
                    line.remove_suffix(1);
                }

                if (!skipped_header) {
                    skipped_header = true;
                } else if (!line.empty()) {
                    const auto columns = split_tab_separated_line(line);
                    if (!columns.empty() && is_decimal_number(columns[0])) {
                        ItemCatalogEntry entry;
                        entry.item_id = static_cast<std::uint32_t>(std::stoul(std::string(columns[0]), nullptr, 10));
                        entry.source_index = starter_items_.size();
                        starter_items_.push_back(entry);
                    }
                }

                if (line_end == std::string::npos) {
                    break;
                }
                line_start = line_end + 1;
            }
        }
    }

    if (!item_metadata_path_.empty()) {
        const auto content = read_text_file(item_metadata_path_);
        if (!content) {
            item_metadata_path_.clear();
        } else {
            std::size_t line_start = 0;
            bool skipped_header = false;
            std::size_t source_index = 0;
            while (line_start <= content->size()) {
                const auto line_end = content->find('\n', line_start);
                std::string_view line(
                    content->data() + line_start,
                    (line_end == std::string::npos ? content->size() : line_end) - line_start);

                if (!line.empty() && line.back() == '\r') {
                    line.remove_suffix(1);
                }

                if (!skipped_header) {
                    skipped_header = true;
                } else if (!line.empty()) {
                    const auto columns = split_tab_separated_line(line);
                    if (!columns.empty() && is_decimal_number(columns[0])) {
                        ItemCatalogEntry entry;
                        entry.item_id = static_cast<std::uint32_t>(std::stoul(std::string(columns[0]), nullptr, 10));
                        // Some shop-only/localized rows keep the primary Korean price at 0 and only populate
                        // the per-language price columns near the end of item.csv.
                        entry.luna_price = parse_preferred_price(columns, {6, 44, 45, 46});
                        entry.cash_price = parse_preferred_price(columns, {7, 29, 31, 38});
                        if (columns.size() > 15) {
                            entry.display_name = std::string(columns[15]);
                        }
                        if (columns.size() > 16) {
                            entry.character_name = std::string(columns[16]);
                        }
                        if (columns.size() > 17) {
                            entry.item_type_name = std::string(columns[17]);
                        }
                        if (columns.size() > 18) {
                            entry.equip_slot_name = std::string(columns[18]);
                        }
                        entry.equipment_like = is_equipment_category(entry.item_type_name);
                        entry.source_index = source_index++;

                        items_by_id_[entry.item_id] = entry;
                        if (entry.equipment_like && !entry.character_name.empty()) {
                            equipment_items_by_character_[normalize_character_key(entry.character_name)].push_back(entry);
                        }
                    }
                }

                if (line_end == std::string::npos) {
                    break;
                }
                line_start = line_end + 1;
            }
        }
    }

    if (!package_contents_path_.empty()) {
        const auto content = read_text_file(package_contents_path_);
        if (!content) {
            package_contents_path_.clear();
        } else {
            std::size_t line_start = 0;
            bool skipped_header = false;
            while (line_start <= content->size()) {
                const auto line_end = content->find('\n', line_start);
                std::string_view line(
                    content->data() + line_start,
                    (line_end == std::string::npos ? content->size() : line_end) - line_start);

                if (!line.empty() && line.back() == '\r') {
                    line.remove_suffix(1);
                }

                if (!skipped_header) {
                    skipped_header = true;
                } else if (!line.empty()) {
                    const auto columns = split_comma_separated_line(line);
                    if (columns.size() >= 2 && is_decimal_number(columns[0]) && is_decimal_number(columns[1])) {
                        const auto package_item_id =
                            static_cast<std::uint32_t>(std::stoul(std::string(columns[0]), nullptr, 10));
                        const auto content_item_id =
                            static_cast<std::uint32_t>(std::stoul(std::string(columns[1]), nullptr, 10));
                        package_contents_by_id_[package_item_id].push_back(content_item_id);
                    }
                }

                if (line_end == std::string::npos) {
                    break;
                }
                line_start = line_end + 1;
            }
        }
    }

    if (!character_metadata_path_.empty()) {
        const auto content = read_text_file(character_metadata_path_);
        if (!content) {
            character_metadata_path_.clear();
        } else {
            std::size_t line_start = 0;
            bool skipped_header = false;
            while (line_start <= content->size()) {
                const auto line_end = content->find('\n', line_start);
                std::string_view line(
                    content->data() + line_start,
                    (line_end == std::string::npos ? content->size() : line_end) - line_start);

                if (!line.empty() && line.back() == '\r') {
                    line.remove_suffix(1);
                }

                if (!skipped_header) {
                    skipped_header = true;
                } else if (!line.empty()) {
                    const auto columns = split_tab_separated_line(line);
                    if (columns.size() > 11 && is_decimal_number(columns[1]) && !columns[11].empty()) {
                        const auto character_id =
                            static_cast<std::uint32_t>(std::stoul(std::string(columns[1]), nullptr, 10));
                        if (character_id != 0) {
                            character_names_by_id_[character_id] = std::string(columns[11]);
                            if (columns.size() > 18 && !columns[18].empty()) {
                                character_asset_keys_by_id_[character_id] = std::string(columns[18]);
                            } else if (columns.size() > 2 && !columns[2].empty()) {
                                character_asset_keys_by_id_[character_id] = std::string(columns[2]);
                            }
                        }
                    }
                }

                if (line_end == std::string::npos) {
                    break;
                }
                line_start = line_end + 1;
            }
        }
    }

    if (!mission_catalog_path_.empty()) {
        const auto content = read_text_file(mission_catalog_path_);
        if (!content) {
            mission_catalog_path_.clear();
        } else {
            std::size_t line_start = 0;
            bool parsed_header = false;
            std::optional<std::size_t> english_title_index;
            std::size_t source_index = 0;
            while (line_start <= content->size()) {
                const auto line_end = content->find('\n', line_start);
                std::string_view line(
                    content->data() + line_start,
                    (line_end == std::string::npos ? content->size() : line_end) - line_start);

                if (!line.empty() && line.back() == '\r') {
                    line.remove_suffix(1);
                }

                const auto columns = split_tab_separated_line(line);
                if (!parsed_header) {
                    english_title_index = find_header_column(columns, "Room Title");
                    parsed_header = true;
                } else if (columns.size() > 34 && is_decimal_number(columns[0])) {
                    MissionCatalogEntry entry;
                    entry.mission_rule_id =
                        static_cast<std::uint32_t>(std::stoul(std::string(columns[0]), nullptr, 10));
                    entry.mission_ui_key = std::string(columns[33]);
                    entry.scene_key = confirmed_mission_scene_key(entry.mission_rule_id);
                    entry.player_spawn_position = confirmed_mission_spawn_position(entry.mission_rule_id);
                    entry.time_limit_seconds =
                        columns.size() > 12 ? parse_optional_u32(columns[12]).value_or(0) : 0;
                    entry.max_players = parse_optional_u8(columns[34]).value_or(0);
                    if (english_title_index && *english_title_index < columns.size()) {
                        entry.english_room_title = std::string(columns[*english_title_index]);
                    }
                    entry.entity_definitions = load_mission_entity_definitions(anchor_path_, entry.scene_key);
                    entry.source_index = source_index++;
                    missions_by_rule_id_[entry.mission_rule_id] = std::move(entry);
                }

                if (line_end == std::string::npos) {
                    break;
                }
                line_start = line_end + 1;
            }
        }
    }
}

std::filesystem::path GameDataCatalog::resolve_skill_catalog_path(const std::filesystem::path& anchor_path) {
    if (const auto preferred = resolve_preferred_path(
            anchor_path,
            "data/setting/item_skill_v2.csv",
            "CDirect3DEngineRecomp/GameFiles/setting/item_skill_v2.csv");
        !preferred.empty()) {
        return preferred;
    }

    return resolve_preferred_path(
        anchor_path,
        "data/setting/item_skill.csv",
        "CDirect3DEngineRecomp/GameFiles/setting/item_skill.csv");
}

std::filesystem::path GameDataCatalog::resolve_item_catalog_path(const std::filesystem::path& anchor_path) {
    return resolve_preferred_path(
        anchor_path,
        "data/setting/game_itemlist.csv",
        "CDirect3DEngineRecomp/GameFiles/setting/game_itemlist.csv");
}

std::filesystem::path GameDataCatalog::resolve_item_metadata_path(const std::filesystem::path& anchor_path) {
    return resolve_preferred_path(
        anchor_path,
        "data/setting/item.csv",
        "CDirect3DEngineRecomp/GameFiles/setting/item.csv");
}

std::filesystem::path GameDataCatalog::resolve_character_metadata_path(const std::filesystem::path& anchor_path) {
    return resolve_preferred_path(
        anchor_path,
        "data/setting/character.csv",
        "CDirect3DEngineRecomp/GameFiles/setting/character.csv");
}

std::filesystem::path GameDataCatalog::resolve_package_contents_path(const std::filesystem::path& anchor_path) {
    return resolve_preferred_path(anchor_path, "data/package_contents.csv", "CPP_Server/data/package_contents.csv");
}

std::filesystem::path GameDataCatalog::resolve_mission_catalog_path(const std::filesystem::path& anchor_path) {
    return resolve_preferred_path(
        anchor_path,
        "data/setting/mission.csv",
        "CDirect3DEngineRecomp/GameFiles/setting/mission.csv");
}

}  // namespace cpp_server::core
