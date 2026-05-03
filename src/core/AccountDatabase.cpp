#include "cpp_server/core/AccountDatabase.h"

#include <cctype>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "cpp_server/core/Json.h"
#include "cpp_server/core/Utf.h"

namespace cpp_server::core {

namespace {

using JsonObject = JsonValue::Object;
using JsonArray = JsonValue::Array;

std::string escape_json(std::string_view text) {
    std::string escaped;
    escaped.reserve(text.size() + 8);
    for (const unsigned char ch : text) {
        switch (ch) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\b':
            escaped += "\\b";
            break;
        case '\f':
            escaped += "\\f";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            if (ch < 0x20) {
                static constexpr char kHex[] = "0123456789abcdef";
                escaped += "\\u00";
                escaped.push_back(kHex[(ch >> 4) & 0x0F]);
                escaped.push_back(kHex[ch & 0x0F]);
            } else {
                escaped.push_back(static_cast<char>(ch));
            }
            break;
        }
    }
    return escaped;
}

std::string_view trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    return text;
}

void write_indent(std::ostringstream& text, int indent) {
    for (int index = 0; index < indent; ++index) {
        text.put(' ');
    }
}

const JsonObject& empty_object() {
    static const JsonObject value{};
    return value;
}

const JsonArray& empty_array() {
    static const JsonArray value{};
    return value;
}

const JsonValue* find_value(const JsonObject& object, std::string_view key) {
    const auto it = object.find(key);
    if (it == object.end()) {
        return nullptr;
    }
    return &it->second;
}

const JsonObject& object_value(const JsonValue* value, std::string_view name) {
    if (value == nullptr) {
        return empty_object();
    }
    if (!value->is_object()) {
        throw std::runtime_error("account database entry '" + std::string(name) + "' must be an object");
    }
    return value->as_object();
}

const JsonArray& array_value(const JsonValue* value, std::string_view name) {
    if (value == nullptr) {
        return empty_array();
    }
    if (!value->is_array()) {
        throw std::runtime_error("account database entry '" + std::string(name) + "' must be an array");
    }
    return value->as_array();
}

std::uint32_t to_u32(std::int64_t value, std::string_view name) {
    if (value < 0 || value > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("account database integer '" + std::string(name) + "' must fit in u32");
    }
    return static_cast<std::uint32_t>(value);
}

std::uint16_t to_u16(std::int64_t value, std::string_view name) {
    if (value < 0 || value > std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error("account database integer '" + std::string(name) + "' must fit in u16");
    }
    return static_cast<std::uint16_t>(value);
}

std::uint8_t to_u8(std::int64_t value, std::string_view name) {
    if (value < 0 || value > std::numeric_limits<std::uint8_t>::max()) {
        throw std::runtime_error("account database integer '" + std::string(name) + "' must fit in u8");
    }
    return static_cast<std::uint8_t>(value);
}

std::uint32_t get_u32(const JsonObject& object, std::string_view key, std::uint32_t default_value = 0) {
    const auto it = object.find(key);
    if (it == object.end()) {
        return default_value;
    }
    return to_u32(it->second.as_integer(), key);
}

std::uint16_t get_u16(const JsonObject& object, std::string_view key, std::uint16_t default_value = 0) {
    const auto it = object.find(key);
    if (it == object.end()) {
        return default_value;
    }
    return to_u16(it->second.as_integer(), key);
}

std::uint8_t get_u8(const JsonObject& object, std::string_view key, std::uint8_t default_value = 0) {
    const auto it = object.find(key);
    if (it == object.end()) {
        return default_value;
    }
    return to_u8(it->second.as_integer(), key);
}

std::string get_string(const JsonObject& object, std::string_view key, std::string default_value = {}) {
    const auto it = object.find(key);
    if (it == object.end()) {
        return default_value;
    }
    return it->second.as_string();
}

template <typename T, typename Getter>
T get_alias(const JsonObject& object, std::string_view preferred_key, std::string_view legacy_key, T default_value, Getter getter) {
    if (object.contains(preferred_key)) {
        return getter(object, preferred_key, default_value);
    }
    return getter(object, legacy_key, default_value);
}

template <typename PacketT, typename Parser>
std::vector<PacketT> parse_object_array(const JsonArray& array, Parser parser);

std::vector<std::uint32_t> parse_u32_array(const JsonArray& array, std::string_view name) {
    std::vector<std::uint32_t> values;
    values.reserve(array.size());
    for (const auto& value : array) {
        if (!value.is_integer()) {
            throw std::runtime_error("account database array '" + std::string(name) + "' must contain integers");
        }
        values.push_back(to_u32(value.as_integer(), name));
    }
    return values;
}

packets::s2c::UpdateAccountInfo_70 parse_packet_70(const JsonObject& object) {
    packets::s2c::UpdateAccountInfo_70 packet;
    packet.exp_current = get_u32(object, "exp_current", 0);
    packet.field_04_unknown = get_u32(object, "field_04_unknown", 0);
    packet.level_raw = get_u8(object, "level_raw", 0);
    packet.field_09_unknown = get_u32(object, "field_09_unknown", 0);
    packet.luna = get_u32(object, "luna", 0);
    packet.cash = get_u32(object, "cash", 0);
    packet.event_cash = get_u32(object, "event_cash", 0);
    packet.total_kill_count = get_u32(object, "total_kill_count", 0);
    packet.profile_flags = get_u32(object, "profile_flags", 0);
    packet.indicator_max_or_limit = get_u32(object, "indicator_max_or_limit", 0);
    packet.field_25_unknown = get_u32(object, "field_25_unknown", 0);
    packet.deploy_slot_0_id = get_u32(object, "deploy_slot_0_id", 0);
    packet.deploy_slot_1_id = get_u32(object, "deploy_slot_1_id", 0);
    packet.deploy_slot_2_id = get_u32(object, "deploy_slot_2_id", 0);
    packet.deploy_slot_3_id = get_u32(object, "deploy_slot_3_id", 0);
    packet.bp = get_u32(object, "bp", 0);
    packet.clan_contribution = get_u32(object, "clan_contribution", 0);
    packet.string0_display_label = get_string(object, "string0_display_label");
    packet.clan_name = get_string(object, "clan_name");
    return packet;
}

packets::s2c::UpdateCharacterList_6B parse_packet_6b_entry(const JsonObject& object) {
    packets::s2c::UpdateCharacterList_6B packet;
    packet.character_id = get_u32(object, "character_id", 0);
    packet.room_config_slot_0 =
        get_alias<std::uint32_t>(object, "equipped_weapon_item_id", "room_config_slot_0", 0, get_u32);
    packet.room_config_slot_1 = get_u32(object, "room_config_slot_1", 0);
    packet.room_config_slot_2 = get_u32(object, "room_config_slot_2", 0);
    packet.room_config_slot_3 = get_alias<std::uint32_t>(object, "clothes_item_id", "room_config_slot_3", 0, get_u32);
    packet.room_config_slot_4 =
        get_alias<std::uint32_t>(object, "accessory_1_item_id", "room_config_slot_4", 0, get_u32);
    packet.room_config_slot_5 =
        get_alias<std::uint32_t>(object, "accessory_2_item_id", "room_config_slot_5", 0, get_u32);
    packet.room_config_slot_6 =
        get_alias<std::uint32_t>(object, "accessory_3_item_id", "room_config_slot_6", 0, get_u32);
    return packet;
}

packets::s2c::UpdateItemList_3F parse_packet_3f_entry(const JsonObject& object) {
    packets::s2c::UpdateItemList_3F packet;
    packet.field_00 = get_u32(object, "field_00", 0);
    packet.field_04 = get_u32(object, "field_04", 0);
    packet.field_08 = get_u32(object, "field_08", 0);
    packet.field_0c = get_u32(object, "field_0c", 0);
    packet.field_10 = get_u32(object, "field_10", 0);
    packet.field_14 = get_u32(object, "field_14", 0);
    packet.field_18 = get_u32(object, "field_18", 0);
    packet.field_1c = get_u32(object, "field_1c", 0);
    packet.field_20 = get_u32(object, "field_20", 0);
    packet.field_24 = get_u32(object, "field_24", 0);
    packet.field_28 = get_u16(object, "field_28", 0);
    packet.field_2a = get_u32(object, "field_2a", 0);
    packet.field_2e = get_u32(object, "field_2e", 0);
    packet.field_32 = get_u8(object, "field_32", 0);
    return packet;
}

packets::s2c::EnumQuickSlot_44 parse_packet_44_entry(const JsonObject& object) {
    packets::s2c::EnumQuickSlot_44 packet;
    packet.entry_key = get_u32(object, "entry_key", 0);
    packet.slot_index = get_u8(object, "slot_index", 0);
    packet.quickslot_entry_id = get_u32(object, "quickslot_entry_id", 0);
    packet.slot_state = get_u8(object, "slot_state", 0);
    packet.display_lookup_key = get_u16(object, "display_lookup_key", 0);
    packet.duration_minutes = get_u32(object, "duration_minutes", 0);
    return packet;
}

packets::s2c::UpdateSkillList_73 parse_packet_73_entry(const JsonObject& object) {
    packets::s2c::UpdateSkillList_73 packet;
    packet.field_00 = get_u32(object, "field_00", 0);
    packet.field_04 = get_u32(object, "field_04", 0);
    packet.skill_item_id = get_u32(object, "skill_item_id", 0);
    packet.duration_minutes = get_u32(object, "duration_minutes", 0);
    packet.update_existing_flag = get_u8(object, "update_existing_flag", 0);
    return packet;
}

packets::s2c::UpdateGuardList_6E parse_packet_6e_entry(const JsonObject& object) {
    packets::s2c::UpdateGuardList_6E packet;
    packet.guard_instance_id = get_u32(object, "guard_instance_id", 0);
    packet.guard_nickname = get_string(object, "guard_nickname");
    packet.guard_kind_id = get_u32(object, "guard_kind_id", 0);
    packet.selectable_flag = get_u32(object, "selectable_flag", 0);
    packet.equipped_item_slot_0_id = get_u32(object, "equipped_item_slot_0_id", 0);
    packet.equipped_item_slot_1_id = get_u32(object, "equipped_item_slot_1_id", 0);
    return packet;
}

InventoryItemStack parse_inventory_item_stack(const JsonObject& object) {
    InventoryItemStack stack;
    stack.item_id = get_u32(object, "item_id", 0);
    stack.owned_count = get_u16(object, "owned_count", 1);
    return stack;
}

OwnedSkill parse_owned_skill(const JsonObject& object) {
    OwnedSkill skill;
    skill.skill_item_id = get_u32(object, "skill_item_id", 0);
    skill.duration_minutes = get_u32(object, "duration_minutes", 0);
    return skill;
}

CharacterInventory parse_character_inventory(const JsonObject& object) {
    CharacterInventory inventory;
    inventory.character_id = get_u32(object, "character_id", 0);
    inventory.equipped_weapon_item_id = get_u32(object, "equipped_weapon_item_id", 0);
    inventory.clothes_item_id = get_u32(object, "clothes_item_id", 0);
    inventory.accessory_1_item_id = get_u32(object, "accessory_1_item_id", 0);
    inventory.accessory_2_item_id = get_u32(object, "accessory_2_item_id", 0);
    inventory.accessory_3_item_id = get_u32(object, "accessory_3_item_id", 0);
    inventory.owned_weapon_item_ids = parse_u32_array(
        array_value(find_value(object, "owned_weapon_item_ids"), "owned_weapon_item_ids"),
        "owned_weapon_item_ids");
    inventory.owned_clothes_item_ids = parse_u32_array(
        array_value(find_value(object, "owned_clothes_item_ids"), "owned_clothes_item_ids"),
        "owned_clothes_item_ids");
    inventory.owned_accessory_1_item_ids = parse_u32_array(
        array_value(find_value(object, "owned_accessory_1_item_ids"), "owned_accessory_1_item_ids"),
        "owned_accessory_1_item_ids");
    inventory.owned_accessory_2_item_ids = parse_u32_array(
        array_value(find_value(object, "owned_accessory_2_item_ids"), "owned_accessory_2_item_ids"),
        "owned_accessory_2_item_ids");
    inventory.owned_accessory_3_item_ids = parse_u32_array(
        array_value(find_value(object, "owned_accessory_3_item_ids"), "owned_accessory_3_item_ids"),
        "owned_accessory_3_item_ids");
    return inventory;
}

std::optional<AccountInventory> parse_optional_inventory(const JsonObject& object, std::string_view key) {
    const auto it = object.find(key);
    if (it == object.end()) {
        return std::nullopt;
    }

    const auto& inventory_object = object_value(&it->second, key);
    AccountInventory inventory;
    inventory.shared_item_stacks = parse_object_array<InventoryItemStack>(
        array_value(find_value(inventory_object, "shared_item_stacks"), "shared_item_stacks"),
        parse_inventory_item_stack);
    inventory.owned_skills = parse_object_array<OwnedSkill>(
        array_value(find_value(inventory_object, "owned_skills"), "owned_skills"),
        parse_owned_skill);
    inventory.characters = parse_object_array<CharacterInventory>(
        array_value(find_value(inventory_object, "characters"), "characters"),
        parse_character_inventory);
    return inventory;
}

template <typename PacketT, typename Parser>
std::vector<PacketT> parse_object_array(const JsonArray& array, Parser parser) {
    std::vector<PacketT> packets;
    packets.reserve(array.size());
    for (const auto& value : array) {
        if (!value.is_object()) {
            throw std::runtime_error("account database packet entry must be an object");
        }
        packets.push_back(parser(value.as_object()));
    }
    return packets;
}

template <typename PacketT, typename Parser>
std::optional<std::vector<PacketT>> parse_optional_object_array(
    const JsonObject& object,
    std::string_view key,
    Parser parser) {
    const auto it = object.find(key);
    if (it == object.end()) {
        return std::nullopt;
    }
    return parse_object_array<PacketT>(array_value(&it->second, key), parser);
}

std::optional<packets::s2c::UpdateAccountInfo_70> parse_optional_packet_70(
    const JsonObject& object,
    std::string_view key) {
    const auto it = object.find(key);
    if (it == object.end()) {
        return std::nullopt;
    }
    return parse_packet_70(object_value(&it->second, key));
}

AccountPacketProfile parse_packet_profile(const JsonObject& object) {
    AccountPacketProfile profile;
    const auto profile_it = object.find("profile");
    if (profile_it == object.end()) {
        return profile;
    }

    const auto& profile_object = object_value(&profile_it->second, "profile");
    profile.account_info = parse_optional_packet_70(profile_object, "packet_70");
    profile.character_list = parse_optional_object_array<packets::s2c::UpdateCharacterList_6B>(
        profile_object,
        "packet_6b_entries",
        parse_packet_6b_entry);
    profile.item_list = parse_optional_object_array<packets::s2c::UpdateItemList_3F>(
        profile_object,
        "packet_3f_entries",
        parse_packet_3f_entry);
    profile.quickslot_list = parse_optional_object_array<packets::s2c::EnumQuickSlot_44>(
        profile_object,
        "packet_44_entries",
        parse_packet_44_entry);
    profile.skill_list = parse_optional_object_array<packets::s2c::UpdateSkillList_73>(
        profile_object,
        "packet_73_entries",
        parse_packet_73_entry);
    profile.guard_list = parse_optional_object_array<packets::s2c::UpdateGuardList_6E>(
        profile_object,
        "packet_6e_entries",
        parse_packet_6e_entry);
    profile.inventory = parse_optional_inventory(profile_object, "inventory");
    return profile;
}

bool has_packet_profile_data(const AccountPacketProfile& profile) {
    return profile.account_info.has_value() ||
           profile.character_list.has_value() ||
           profile.item_list.has_value() ||
           profile.quickslot_list.has_value() ||
           profile.skill_list.has_value() ||
           profile.guard_list.has_value() ||
           profile.inventory.has_value();
}

void append_packet_70_json(
    std::ostringstream& text,
    const packets::s2c::UpdateAccountInfo_70& packet,
    int indent) {
    text << "{\n";
    write_indent(text, indent + 2);
    text << "\"exp_current\": " << packet.exp_current << ",\n";
    write_indent(text, indent + 2);
    text << "\"field_04_unknown\": " << packet.field_04_unknown << ",\n";
    write_indent(text, indent + 2);
    text << "\"level_raw\": " << static_cast<unsigned int>(packet.level_raw) << ",\n";
    write_indent(text, indent + 2);
    text << "\"field_09_unknown\": " << packet.field_09_unknown << ",\n";
    write_indent(text, indent + 2);
    text << "\"luna\": " << packet.luna << ",\n";
    write_indent(text, indent + 2);
    text << "\"cash\": " << packet.cash << ",\n";
    write_indent(text, indent + 2);
    text << "\"event_cash\": " << packet.event_cash << ",\n";
    write_indent(text, indent + 2);
    text << "\"total_kill_count\": " << packet.total_kill_count << ",\n";
    write_indent(text, indent + 2);
    text << "\"profile_flags\": " << packet.profile_flags << ",\n";
    write_indent(text, indent + 2);
    text << "\"indicator_max_or_limit\": " << packet.indicator_max_or_limit << ",\n";
    write_indent(text, indent + 2);
    text << "\"field_25_unknown\": " << packet.field_25_unknown << ",\n";
    write_indent(text, indent + 2);
    text << "\"deploy_slot_0_id\": " << packet.deploy_slot_0_id << ",\n";
    write_indent(text, indent + 2);
    text << "\"deploy_slot_1_id\": " << packet.deploy_slot_1_id << ",\n";
    write_indent(text, indent + 2);
    text << "\"deploy_slot_2_id\": " << packet.deploy_slot_2_id << ",\n";
    write_indent(text, indent + 2);
    text << "\"deploy_slot_3_id\": " << packet.deploy_slot_3_id << ",\n";
    write_indent(text, indent + 2);
    text << "\"bp\": " << packet.bp << ",\n";
    write_indent(text, indent + 2);
    text << "\"clan_contribution\": " << packet.clan_contribution << ",\n";
    write_indent(text, indent + 2);
    text << "\"string0_display_label\": \"" << escape_json(packet.string0_display_label) << "\",\n";
    write_indent(text, indent + 2);
    text << "\"clan_name\": \"" << escape_json(packet.clan_name) << "\"\n";
    write_indent(text, indent);
    text << "}";
}

void append_packet_6b_json(
    std::ostringstream& text,
    const packets::s2c::UpdateCharacterList_6B& packet,
    int indent) {
    text << "{\n";
    write_indent(text, indent + 2);
    text << "\"character_id\": " << packet.character_id << ",\n";
    write_indent(text, indent + 2);
    text << "\"equipped_weapon_item_id\": " << packet.room_config_slot_0 << ",\n";
    write_indent(text, indent + 2);
    text << "\"room_config_slot_1\": " << packet.room_config_slot_1 << ",\n";
    write_indent(text, indent + 2);
    text << "\"room_config_slot_2\": " << packet.room_config_slot_2 << ",\n";
    write_indent(text, indent + 2);
    text << "\"clothes_item_id\": " << packet.room_config_slot_3 << ",\n";
    write_indent(text, indent + 2);
    text << "\"accessory_1_item_id\": " << packet.room_config_slot_4 << ",\n";
    write_indent(text, indent + 2);
    text << "\"accessory_2_item_id\": " << packet.room_config_slot_5 << ",\n";
    write_indent(text, indent + 2);
    text << "\"accessory_3_item_id\": " << packet.room_config_slot_6 << "\n";
    write_indent(text, indent);
    text << "}";
}

void append_packet_3f_json(
    std::ostringstream& text,
    const packets::s2c::UpdateItemList_3F& packet,
    int indent) {
    text << "{\n";
    write_indent(text, indent + 2);
    text << "\"field_00\": " << packet.field_00 << ",\n";
    write_indent(text, indent + 2);
    text << "\"field_04\": " << packet.field_04 << ",\n";
    write_indent(text, indent + 2);
    text << "\"field_08\": " << packet.field_08 << ",\n";
    write_indent(text, indent + 2);
    text << "\"field_0c\": " << packet.field_0c << ",\n";
    write_indent(text, indent + 2);
    text << "\"field_10\": " << packet.field_10 << ",\n";
    write_indent(text, indent + 2);
    text << "\"field_14\": " << packet.field_14 << ",\n";
    write_indent(text, indent + 2);
    text << "\"field_18\": " << packet.field_18 << ",\n";
    write_indent(text, indent + 2);
    text << "\"field_1c\": " << packet.field_1c << ",\n";
    write_indent(text, indent + 2);
    text << "\"field_20\": " << packet.field_20 << ",\n";
    write_indent(text, indent + 2);
    text << "\"field_24\": " << packet.field_24 << ",\n";
    write_indent(text, indent + 2);
    text << "\"field_28\": " << packet.field_28 << ",\n";
    write_indent(text, indent + 2);
    text << "\"field_2a\": " << packet.field_2a << ",\n";
    write_indent(text, indent + 2);
    text << "\"field_2e\": " << packet.field_2e << ",\n";
    write_indent(text, indent + 2);
    text << "\"field_32\": " << static_cast<unsigned int>(packet.field_32) << "\n";
    write_indent(text, indent);
    text << "}";
}

void append_packet_44_json(
    std::ostringstream& text,
    const packets::s2c::EnumQuickSlot_44& packet,
    int indent) {
    text << "{\n";
    write_indent(text, indent + 2);
    text << "\"entry_key\": " << packet.entry_key << ",\n";
    write_indent(text, indent + 2);
    text << "\"slot_index\": " << static_cast<unsigned int>(packet.slot_index) << ",\n";
    write_indent(text, indent + 2);
    text << "\"quickslot_entry_id\": " << packet.quickslot_entry_id << ",\n";
    write_indent(text, indent + 2);
    text << "\"slot_state\": " << static_cast<unsigned int>(packet.slot_state) << ",\n";
    write_indent(text, indent + 2);
    text << "\"display_lookup_key\": " << packet.display_lookup_key << ",\n";
    write_indent(text, indent + 2);
    text << "\"duration_minutes\": " << packet.duration_minutes << "\n";
    write_indent(text, indent);
    text << "}";
}

void append_packet_73_json(
    std::ostringstream& text,
    const packets::s2c::UpdateSkillList_73& packet,
    int indent) {
    text << "{\n";
    write_indent(text, indent + 2);
    text << "\"field_00\": " << packet.field_00 << ",\n";
    write_indent(text, indent + 2);
    text << "\"field_04\": " << packet.field_04 << ",\n";
    write_indent(text, indent + 2);
    text << "\"skill_item_id\": " << packet.skill_item_id << ",\n";
    write_indent(text, indent + 2);
    text << "\"duration_minutes\": " << packet.duration_minutes << ",\n";
    write_indent(text, indent + 2);
    text << "\"update_existing_flag\": " << static_cast<unsigned int>(packet.update_existing_flag) << "\n";
    write_indent(text, indent);
    text << "}";
}

void append_packet_6e_json(
    std::ostringstream& text,
    const packets::s2c::UpdateGuardList_6E& packet,
    int indent) {
    text << "{\n";
    write_indent(text, indent + 2);
    text << "\"guard_instance_id\": " << packet.guard_instance_id << ",\n";
    write_indent(text, indent + 2);
    text << "\"guard_nickname\": \"" << escape_json(packet.guard_nickname) << "\",\n";
    write_indent(text, indent + 2);
    text << "\"guard_kind_id\": " << packet.guard_kind_id << ",\n";
    write_indent(text, indent + 2);
    text << "\"selectable_flag\": " << packet.selectable_flag << ",\n";
    write_indent(text, indent + 2);
    text << "\"equipped_item_slot_0_id\": " << packet.equipped_item_slot_0_id << ",\n";
    write_indent(text, indent + 2);
    text << "\"equipped_item_slot_1_id\": " << packet.equipped_item_slot_1_id << "\n";
    write_indent(text, indent);
    text << "}";
}

template <typename PacketT, typename Writer>
void append_packet_array_json(
    std::ostringstream& text,
    const std::vector<PacketT>& packets,
    int indent,
    Writer writer) {
    text << "[";
    if (!packets.empty()) {
        text << '\n';
    }
    for (std::size_t index = 0; index < packets.size(); ++index) {
        write_indent(text, indent + 2);
        writer(text, packets[index], indent + 2);
        if (index + 1 != packets.size()) {
            text << ',';
        }
        text << '\n';
    }
    write_indent(text, indent);
    text << "]";
}

void append_u32_array_json(std::ostringstream& text, const std::vector<std::uint32_t>& values, int indent) {
    text << "[";
    if (!values.empty()) {
        text << '\n';
    }
    for (std::size_t index = 0; index < values.size(); ++index) {
        write_indent(text, indent + 2);
        text << values[index];
        if (index + 1 != values.size()) {
            text << ',';
        }
        text << '\n';
    }
    write_indent(text, indent);
    text << "]";
}

void append_inventory_item_stack_json(
    std::ostringstream& text,
    const InventoryItemStack& stack,
    int indent) {
    text << "{\n";
    write_indent(text, indent + 2);
    text << "\"item_id\": " << stack.item_id << ",\n";
    write_indent(text, indent + 2);
    text << "\"owned_count\": " << stack.owned_count << "\n";
    write_indent(text, indent);
    text << "}";
}

void append_owned_skill_json(
    std::ostringstream& text,
    const OwnedSkill& skill,
    int indent) {
    text << "{\n";
    write_indent(text, indent + 2);
    text << "\"skill_item_id\": " << skill.skill_item_id << ",\n";
    write_indent(text, indent + 2);
    text << "\"duration_minutes\": " << skill.duration_minutes << "\n";
    write_indent(text, indent);
    text << "}";
}

void append_character_inventory_json(
    std::ostringstream& text,
    const CharacterInventory& inventory,
    int indent) {
    text << "{\n";
    write_indent(text, indent + 2);
    text << "\"character_id\": " << inventory.character_id << ",\n";
    write_indent(text, indent + 2);
    text << "\"equipped_weapon_item_id\": " << inventory.equipped_weapon_item_id << ",\n";
    write_indent(text, indent + 2);
    text << "\"clothes_item_id\": " << inventory.clothes_item_id << ",\n";
    write_indent(text, indent + 2);
    text << "\"accessory_1_item_id\": " << inventory.accessory_1_item_id << ",\n";
    write_indent(text, indent + 2);
    text << "\"accessory_2_item_id\": " << inventory.accessory_2_item_id << ",\n";
    write_indent(text, indent + 2);
    text << "\"accessory_3_item_id\": " << inventory.accessory_3_item_id << ",\n";
    write_indent(text, indent + 2);
    text << "\"owned_weapon_item_ids\": ";
    append_u32_array_json(text, inventory.owned_weapon_item_ids, indent + 2);
    text << ",\n";
    write_indent(text, indent + 2);
    text << "\"owned_clothes_item_ids\": ";
    append_u32_array_json(text, inventory.owned_clothes_item_ids, indent + 2);
    text << ",\n";
    write_indent(text, indent + 2);
    text << "\"owned_accessory_1_item_ids\": ";
    append_u32_array_json(text, inventory.owned_accessory_1_item_ids, indent + 2);
    text << ",\n";
    write_indent(text, indent + 2);
    text << "\"owned_accessory_2_item_ids\": ";
    append_u32_array_json(text, inventory.owned_accessory_2_item_ids, indent + 2);
    text << ",\n";
    write_indent(text, indent + 2);
    text << "\"owned_accessory_3_item_ids\": ";
    append_u32_array_json(text, inventory.owned_accessory_3_item_ids, indent + 2);
    text << '\n';
    write_indent(text, indent);
    text << "}";
}

void append_inventory_json(
    std::ostringstream& text,
    const AccountInventory& inventory,
    int indent) {
    text << "{\n";
    write_indent(text, indent + 2);
    text << "\"shared_item_stacks\": ";
    append_packet_array_json(text, inventory.shared_item_stacks, indent + 2, append_inventory_item_stack_json);
    text << ",\n";
    write_indent(text, indent + 2);
    text << "\"owned_skills\": ";
    append_packet_array_json(text, inventory.owned_skills, indent + 2, append_owned_skill_json);
    text << ",\n";
    write_indent(text, indent + 2);
    text << "\"characters\": ";
    append_packet_array_json(text, inventory.characters, indent + 2, append_character_inventory_json);
    text << '\n';
    write_indent(text, indent);
    text << "}";
}

void append_packet_profile_json(
    std::ostringstream& text,
    const AccountPacketProfile& profile,
    int indent) {
    text << "{";

    std::size_t field_count = 0;
    if (profile.account_info.has_value()) {
        ++field_count;
    }
    if (profile.character_list.has_value()) {
        ++field_count;
    }
    if (profile.item_list.has_value()) {
        ++field_count;
    }
    if (profile.quickslot_list.has_value()) {
        ++field_count;
    }
    if (profile.skill_list.has_value()) {
        ++field_count;
    }
    if (profile.guard_list.has_value()) {
        ++field_count;
    }
    if (profile.inventory.has_value()) {
        ++field_count;
    }

    if (field_count == 0) {
        text << "}";
        return;
    }

    text << '\n';
    std::size_t emitted = 0;
    auto emit_comma = [&]() {
        ++emitted;
        if (emitted != field_count) {
            text << ',';
        }
        text << '\n';
    };

    if (profile.account_info.has_value()) {
        write_indent(text, indent + 2);
        text << "\"packet_70\": ";
        append_packet_70_json(text, *profile.account_info, indent + 2);
        emit_comma();
    }
    if (profile.character_list.has_value()) {
        write_indent(text, indent + 2);
        text << "\"packet_6b_entries\": ";
        append_packet_array_json(text, *profile.character_list, indent + 2, append_packet_6b_json);
        emit_comma();
    }
    if (profile.item_list.has_value()) {
        write_indent(text, indent + 2);
        text << "\"packet_3f_entries\": ";
        append_packet_array_json(text, *profile.item_list, indent + 2, append_packet_3f_json);
        emit_comma();
    }
    if (profile.quickslot_list.has_value()) {
        write_indent(text, indent + 2);
        text << "\"packet_44_entries\": ";
        append_packet_array_json(text, *profile.quickslot_list, indent + 2, append_packet_44_json);
        emit_comma();
    }
    if (profile.skill_list.has_value()) {
        write_indent(text, indent + 2);
        text << "\"packet_73_entries\": ";
        append_packet_array_json(text, *profile.skill_list, indent + 2, append_packet_73_json);
        emit_comma();
    }
    if (profile.guard_list.has_value()) {
        write_indent(text, indent + 2);
        text << "\"packet_6e_entries\": ";
        append_packet_array_json(text, *profile.guard_list, indent + 2, append_packet_6e_json);
        emit_comma();
    }
    if (profile.inventory.has_value()) {
        write_indent(text, indent + 2);
        text << "\"inventory\": ";
        append_inventory_json(text, *profile.inventory, indent + 2);
        emit_comma();
    }

    write_indent(text, indent);
    text << "}";
}

}  // namespace

AccountDatabase::AccountDatabase(std::filesystem::path path) : path_(std::move(path)) {}

std::optional<AccountRecord> AccountDatabase::authenticate(
    std::string_view login_id,
    std::string_view password) const {
    std::scoped_lock lock(mutex_);
    ensure_loaded_locked();
    for (const auto& account : accounts_) {
        if (account.login_id == login_id && account.password == password) {
            return account;
        }
    }
    return std::nullopt;
}

std::optional<AccountRecord> AccountDatabase::find_by_login(std::string_view login_id) const {
    std::scoped_lock lock(mutex_);
    ensure_loaded_locked();
    for (const auto& account : accounts_) {
        if (account.login_id == login_id) {
            return account;
        }
    }
    return std::nullopt;
}

ResolveCharacterNameResult AccountDatabase::resolve_character_name(
    std::string_view login_id,
    std::string_view requested_name,
    std::string& effective_name) {
    std::scoped_lock lock(mutex_);
    ensure_loaded_locked();

    AccountRecord* target = nullptr;
    for (auto& account : accounts_) {
        if (account.login_id == login_id) {
            target = &account;
            break;
        }
    }
    if (target == nullptr) {
        return ResolveCharacterNameResult::kAccountNotFound;
    }

    if (target->nickname && !target->nickname->empty()) {
        effective_name = *target->nickname;
        return ResolveCharacterNameResult::kResolved;
    }

    const auto trimmed_name = trim(requested_name);
    if (!is_valid_character_name(trimmed_name)) {
        return ResolveCharacterNameResult::kInvalidName;
    }

    for (const auto& account : accounts_) {
        if (account.login_id != login_id && account.nickname && *account.nickname == trimmed_name) {
            return ResolveCharacterNameResult::kNameInUse;
        }
    }

    target->nickname = std::string(trimmed_name);
    effective_name = *target->nickname;
    store_locked();
    return ResolveCharacterNameResult::kResolved;
}

std::optional<AccountPacketProfile> AccountDatabase::find_packet_profile(std::string_view login_id) const {
    std::scoped_lock lock(mutex_);
    ensure_loaded_locked();
    for (const auto& account : accounts_) {
        if (account.login_id == login_id) {
            return account.packet_profile;
        }
    }
    return std::nullopt;
}

std::optional<packets::s2c::UpdateAccountInfo_70> AccountDatabase::find_account_info(std::string_view login_id) const {
    std::scoped_lock lock(mutex_);
    ensure_loaded_locked();
    for (const auto& account : accounts_) {
        if (account.login_id == login_id) {
            return account.packet_profile.account_info;
        }
    }
    return std::nullopt;
}

std::optional<std::vector<packets::s2c::UpdateCharacterList_6B>> AccountDatabase::find_character_list(
    std::string_view login_id) const {
    std::scoped_lock lock(mutex_);
    ensure_loaded_locked();
    for (const auto& account : accounts_) {
        if (account.login_id == login_id) {
            return account.packet_profile.character_list;
        }
    }
    return std::nullopt;
}

std::optional<std::vector<packets::s2c::UpdateItemList_3F>> AccountDatabase::find_item_list(std::string_view login_id) const {
    std::scoped_lock lock(mutex_);
    ensure_loaded_locked();
    for (const auto& account : accounts_) {
        if (account.login_id == login_id) {
            return account.packet_profile.item_list;
        }
    }
    return std::nullopt;
}

std::optional<std::vector<packets::s2c::EnumQuickSlot_44>> AccountDatabase::find_quickslot_list(
    std::string_view login_id) const {
    std::scoped_lock lock(mutex_);
    ensure_loaded_locked();
    for (const auto& account : accounts_) {
        if (account.login_id == login_id) {
            return account.packet_profile.quickslot_list;
        }
    }
    return std::nullopt;
}

std::optional<std::vector<packets::s2c::UpdateSkillList_73>> AccountDatabase::find_skill_list(
    std::string_view login_id) const {
    std::scoped_lock lock(mutex_);
    ensure_loaded_locked();
    for (const auto& account : accounts_) {
        if (account.login_id == login_id) {
            return account.packet_profile.skill_list;
        }
    }
    return std::nullopt;
}

std::optional<std::vector<packets::s2c::UpdateGuardList_6E>> AccountDatabase::find_guard_list(
    std::string_view login_id) const {
    std::scoped_lock lock(mutex_);
    ensure_loaded_locked();
    for (const auto& account : accounts_) {
        if (account.login_id == login_id) {
            return account.packet_profile.guard_list;
        }
    }
    return std::nullopt;
}

std::optional<AccountInventory> AccountDatabase::find_inventory(std::string_view login_id) const {
    std::scoped_lock lock(mutex_);
    ensure_loaded_locked();
    for (const auto& account : accounts_) {
        if (account.login_id == login_id) {
            return account.packet_profile.inventory;
        }
    }
    return std::nullopt;
}

bool AccountDatabase::set_packet_profile(std::string_view login_id, AccountPacketProfile profile) {
    std::scoped_lock lock(mutex_);
    ensure_loaded_locked();
    for (auto& account : accounts_) {
        if (account.login_id == login_id) {
            account.packet_profile = std::move(profile);
            store_locked();
            return true;
        }
    }
    return false;
}

bool AccountDatabase::set_account_info(
    std::string_view login_id,
    std::optional<packets::s2c::UpdateAccountInfo_70> account_info) {
    std::scoped_lock lock(mutex_);
    ensure_loaded_locked();
    for (auto& account : accounts_) {
        if (account.login_id == login_id) {
            account.packet_profile.account_info = std::move(account_info);
            store_locked();
            return true;
        }
    }
    return false;
}

bool AccountDatabase::set_character_list(
    std::string_view login_id,
    std::optional<std::vector<packets::s2c::UpdateCharacterList_6B>> character_list) {
    std::scoped_lock lock(mutex_);
    ensure_loaded_locked();
    for (auto& account : accounts_) {
        if (account.login_id == login_id) {
            account.packet_profile.character_list = std::move(character_list);
            store_locked();
            return true;
        }
    }
    return false;
}

bool AccountDatabase::set_item_list(
    std::string_view login_id,
    std::optional<std::vector<packets::s2c::UpdateItemList_3F>> item_list) {
    std::scoped_lock lock(mutex_);
    ensure_loaded_locked();
    for (auto& account : accounts_) {
        if (account.login_id == login_id) {
            account.packet_profile.item_list = std::move(item_list);
            store_locked();
            return true;
        }
    }
    return false;
}

bool AccountDatabase::set_quickslot_list(
    std::string_view login_id,
    std::optional<std::vector<packets::s2c::EnumQuickSlot_44>> quickslot_list) {
    std::scoped_lock lock(mutex_);
    ensure_loaded_locked();
    for (auto& account : accounts_) {
        if (account.login_id == login_id) {
            account.packet_profile.quickslot_list = std::move(quickslot_list);
            store_locked();
            return true;
        }
    }
    return false;
}

bool AccountDatabase::set_skill_list(
    std::string_view login_id,
    std::optional<std::vector<packets::s2c::UpdateSkillList_73>> skill_list) {
    std::scoped_lock lock(mutex_);
    ensure_loaded_locked();
    for (auto& account : accounts_) {
        if (account.login_id == login_id) {
            account.packet_profile.skill_list = std::move(skill_list);
            store_locked();
            return true;
        }
    }
    return false;
}

bool AccountDatabase::set_guard_list(
    std::string_view login_id,
    std::optional<std::vector<packets::s2c::UpdateGuardList_6E>> guard_list) {
    std::scoped_lock lock(mutex_);
    ensure_loaded_locked();
    for (auto& account : accounts_) {
        if (account.login_id == login_id) {
            account.packet_profile.guard_list = std::move(guard_list);
            store_locked();
            return true;
        }
    }
    return false;
}

bool AccountDatabase::set_inventory(
    std::string_view login_id,
    std::optional<AccountInventory> inventory) {
    std::scoped_lock lock(mutex_);
    ensure_loaded_locked();
    for (auto& account : accounts_) {
        if (account.login_id == login_id) {
            account.packet_profile.inventory = std::move(inventory);
            store_locked();
            return true;
        }
    }
    return false;
}

void AccountDatabase::ensure_loaded_locked() const {
    std::optional<std::filesystem::file_time_type> current_mtime;
    std::error_code error;
    if (std::filesystem::exists(path_, error) && !error) {
        current_mtime = std::filesystem::last_write_time(path_, error);
        if (error) {
            current_mtime.reset();
            error.clear();
        }
    }

    if (!loaded_ || current_mtime != cached_mtime_) {
        load_locked();
        loaded_ = true;
        cached_mtime_ = current_mtime;
    }
}

void AccountDatabase::load_locked() const {
    accounts_.clear();
    if (!std::filesystem::exists(path_)) {
        cached_mtime_.reset();
        return;
    }

    const auto root = LoadJsonFile(path_);
    if (!root.is_object()) {
        throw std::runtime_error("account database must contain a top-level object");
    }

    const auto* accounts_value = root.find("accounts");
    if (accounts_value == nullptr) {
        return;
    }
    if (!accounts_value->is_array()) {
        throw std::runtime_error("account database 'accounts' entry must be an array");
    }

    for (const auto& entry : accounts_value->as_array()) {
        if (!entry.is_object()) {
            throw std::runtime_error("account database entry must be an object");
        }

        const JsonObject& object = entry.as_object();
        const auto login_it = object.find("login_id");
        const auto password_it = object.find("password");
        if (login_it == object.end() || password_it == object.end()) {
            throw std::runtime_error("account database entries require 'login_id' and 'password'");
        }
        if (!login_it->second.is_string() || !password_it->second.is_string()) {
            throw std::runtime_error("account database 'login_id' and 'password' must be strings");
        }

        AccountRecord account;
        account.login_id = login_it->second.as_string();
        account.password = password_it->second.as_string();

        if (const auto nickname_it = object.find("nickname"); nickname_it != object.end()) {
            if (nickname_it->second.is_null()) {
                account.nickname.reset();
            } else if (nickname_it->second.is_string()) {
                const auto value = trim(nickname_it->second.as_string());
                if (!value.empty()) {
                    account.nickname = std::string(value);
                }
            } else {
                throw std::runtime_error("account database 'nickname' must be a string or null");
            }
        }

        account.packet_profile = parse_packet_profile(object);
        accounts_.push_back(std::move(account));
    }
}

void AccountDatabase::store_locked() const {
    if (path_.empty()) {
        throw std::runtime_error("account database path is empty");
    }

    if (path_.has_parent_path()) {
        std::filesystem::create_directories(path_.parent_path());
    }

    std::ostringstream text;
    text << "{\n  \"accounts\": [";
    if (!accounts_.empty()) {
        text << '\n';
    }

    for (std::size_t index = 0; index < accounts_.size(); ++index) {
        const auto& account = accounts_[index];
        text << "    {\n";
        text << "      \"login_id\": \"" << escape_json(account.login_id) << "\",\n";
        text << "      \"password\": \"" << escape_json(account.password) << "\",\n";
        text << "      \"nickname\": ";
        if (account.nickname && !account.nickname->empty()) {
            text << "\"" << escape_json(*account.nickname) << "\"";
        } else {
            text << "null";
        }

        if (has_packet_profile_data(account.packet_profile)) {
            text << ",\n";
            text << "      \"profile\": ";
            append_packet_profile_json(text, account.packet_profile, 6);
            text << '\n';
        } else {
            text << '\n';
        }

        text << "    }";
        if (index + 1 != accounts_.size()) {
            text << ',';
        }
        text << '\n';
    }

    text << "  ]\n}\n";

    const auto temp_path = path_.string() + ".tmp";
    {
        std::ofstream output(temp_path, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error("failed to open temporary account database file for writing");
        }
        output << text.str();
    }

    std::error_code error;
    std::filesystem::rename(temp_path, path_, error);
    if (error) {
        std::filesystem::remove(path_, error);
        error.clear();
        std::filesystem::rename(temp_path, path_, error);
        if (error) {
            throw std::runtime_error("failed to replace account database file");
        }
    }

    error.clear();
    cached_mtime_ = std::filesystem::last_write_time(path_, error);
    if (error) {
        cached_mtime_.reset();
    }
}

bool AccountDatabase::is_valid_character_name(std::string_view name) {
    const auto trimmed_name = trim(name);
    if (trimmed_name.empty()) {
        return false;
    }

    if (Utf8ToUtf16(trimmed_name).size() > 12) {
        return false;
    }

    return trimmed_name.find_first_of("\r\n\t") == std::string_view::npos;
}

}  // namespace cpp_server::core
