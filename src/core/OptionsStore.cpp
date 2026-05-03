#include "cpp_server/core/OptionsStore.h"

#include <limits>

#include "cpp_server/core/ByteBuffer.h"
#include "cpp_server/core/Json.h"

namespace cpp_server::core {

namespace {

using JsonObject = JsonValue::Object;
using JsonArray = JsonValue::Array;

const JsonObject& empty_object() {
    static const JsonObject value{};
    return value;
}

const JsonArray& empty_array() {
    static const JsonArray value{};
    return value;
}

const JsonObject& object_value(const JsonValue* value, std::string_view name) {
    if (value == nullptr) {
        return empty_object();
    }
    if (!value->is_object()) {
        throw std::runtime_error("JSON section '" + std::string(name) + "' must be an object");
    }
    return value->as_object();
}

const JsonArray& array_value(const JsonValue* value, std::string_view name) {
    if (value == nullptr) {
        return empty_array();
    }
    if (!value->is_array()) {
        throw std::runtime_error("JSON entry '" + std::string(name) + "' must be an array");
    }
    return value->as_array();
}

std::uint32_t to_u32(std::int64_t value, std::string_view name) {
    if (value < 0 || value > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("JSON integer '" + std::string(name) + "' must fit in u32");
    }
    return static_cast<std::uint32_t>(value);
}

std::uint16_t to_u16(std::int64_t value, std::string_view name) {
    if (value < 0 || value > std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error("JSON integer '" + std::string(name) + "' must fit in u16");
    }
    return static_cast<std::uint16_t>(value);
}

std::uint8_t to_u8(std::int64_t value, std::string_view name) {
    if (value < 0 || value > std::numeric_limits<std::uint8_t>::max()) {
        throw std::runtime_error("JSON integer '" + std::string(name) + "' must fit in u8");
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

std::vector<std::uint32_t> get_u32_list(
    const JsonObject& object,
    std::string_view key,
    std::size_t expected_length,
    const std::vector<std::uint32_t>& default_value = {}) {
    const auto it = object.find(key);
    if (it == object.end()) {
        if (default_value.size() != expected_length) {
            throw std::runtime_error("default JSON list length mismatch");
        }
        return default_value;
    }

    const auto& array = it->second.as_array();
    if (array.size() != expected_length) {
        throw std::runtime_error("JSON array '" + std::string(key) + "' has an unexpected length");
    }

    std::vector<std::uint32_t> values;
    values.reserve(expected_length);
    for (const auto& value : array) {
        values.push_back(to_u32(value.as_integer(), key));
    }
    return values;
}

packets::s2c::EnumChannel_A3 parse_packet_a3_entry(const JsonObject& object) {
    packets::s2c::EnumChannel_A3 packet;
    packet.channel_type_id = get_alias<std::uint32_t>(object, "channel_type_id", "channel_target_id", 0, get_u32);
    packet.channel_selector_id = get_alias<std::uint16_t>(object, "channel_selector_id", "channel_id", 0, get_u16);
    packet.packed_ipv4 = ParseIpv4(get_string(object, "ipv4", "127.0.0.1"));
    packet.tcp_port = get_u16(object, "tcp_port", 0);
    packet.name = get_string(object, "name");
    return packet;
}

packets::s2c::EnumGameRoom_0E parse_packet_0e_entry(const JsonObject& object) {
    packets::s2c::EnumGameRoom_0E packet;
    packet.primary_name = get_alias<std::string>(object, "primary_name", "field_0f_primary_name", {}, get_string);
    packet.secondary_name = get_alias<std::string>(object, "secondary_name", "field_11_secondary_name", {}, get_string);
    packet.room_state_code = get_alias<std::uint8_t>(object, "room_state_code", "field_02", 0, get_u8);
    packet.password_required_flag = get_alias<std::uint8_t>(object, "password_required_flag", "field_03", 0, get_u8);
    packet.room_id = get_alias<std::uint16_t>(object, "room_id", "field_04", 0, get_u16);
    packet.current_players = get_alias<std::uint8_t>(object, "current_players", "field_07", 0, get_u8);
    packet.max_players = get_alias<std::uint8_t>(object, "max_players", "field_06", 0, get_u8);
    packet.rule_or_mission_id = get_alias<std::uint16_t>(object, "rule_or_mission_id", "field_08", 0, get_u16);
    packet.field_0a_reserved = get_alias<std::uint8_t>(object, "field_0a_reserved", "field_0a", 0, get_u8);
    packet.mission_icon_count = get_alias<std::uint8_t>(object, "mission_icon_count", "field_0b", 0, get_u8);
    packet.flags = get_alias<std::uint8_t>(object, "flags", "field_0c", 0, get_u8);
    packet.limit_minutes = get_alias<std::uint8_t>(object, "limit_minutes", "field_0d", 0, get_u8);
    packet.limit_kills = get_alias<std::uint8_t>(object, "limit_kills", "field_0e", 0, get_u8);
    return packet;
}

packets::s2c::RoomInfo_0D parse_packet_0d(const JsonObject& object, const std::vector<packets::s2c::EnumGameRoom_0E>& room_entries) {
    const packets::s2c::EnumGameRoom_0E fallback = room_entries.empty() ? packets::s2c::EnumGameRoom_0E{} : room_entries.front();

    packets::s2c::RoomInfo_0D packet;
    packet.room_name = get_string(object, "room_name", fallback.primary_name);
    packet.field_01 = get_u8(object, "field_01", 0);
    packet.room_state_code = get_u8(object, "room_state_code", fallback.room_state_code);
    packet.password_required_flag = get_u8(object, "password_required_flag", fallback.password_required_flag);
    packet.room_id = get_u16(object, "room_id", fallback.room_id);
    packet.current_players = get_u8(object, "current_players", fallback.current_players);
    packet.max_players = get_u8(object, "max_players", fallback.max_players);
    packet.rule_or_mission_id = get_u16(object, "rule_or_mission_id", fallback.rule_or_mission_id);
    packet.field_0a_reserved = get_u8(object, "field_0a_reserved", fallback.field_0a_reserved);
    packet.mission_icon_count = get_u8(object, "mission_icon_count", fallback.mission_icon_count);
    packet.flags = get_u8(object, "flags", fallback.flags);
    packet.limit_minutes = get_u8(object, "limit_minutes", fallback.limit_minutes);
    packet.limit_kills = get_u8(object, "limit_kills", fallback.limit_kills);
    return packet;
}

packets::s2c::TrainingGuardState_42 parse_guard_42(const JsonObject& object) {
    packets::s2c::TrainingGuardState_42 packet;
    packet.guard_instance_id = get_alias<std::uint32_t>(object, "guard_instance_id", "field_00", 0, get_u32);
    packet.guard_nickname = get_alias<std::string>(object, "guard_nickname", "string", {}, get_string);
    packet.guard_kind_id = get_alias<std::uint32_t>(object, "guard_kind_id", "field_05", 0, get_u32);
    packet.selectable_flag = get_alias<std::uint32_t>(object, "selectable_flag", "field_09", 0, get_u32);
    packet.equipped_item_slot_0_id = get_alias<std::uint32_t>(object, "equipped_item_slot_0_id", "field_0d", 0, get_u32);
    packet.equipped_item_slot_1_id = get_alias<std::uint32_t>(object, "equipped_item_slot_1_id", "field_11", 0, get_u32);
    return packet;
}

packets::s2c::UpdateGuardList_6E parse_guard_6e(const JsonObject& object) {
    const auto training = parse_guard_42(object);
    packets::s2c::UpdateGuardList_6E packet;
    packet.guard_instance_id = training.guard_instance_id;
    packet.guard_nickname = training.guard_nickname;
    packet.guard_kind_id = training.guard_kind_id;
    packet.selectable_flag = training.selectable_flag;
    packet.equipped_item_slot_0_id = training.equipped_item_slot_0_id;
    packet.equipped_item_slot_1_id = training.equipped_item_slot_1_id;
    return packet;
}

packets::s2c::FullRoomStateReply_84 parse_packet_84(const JsonObject& object, const std::vector<packets::s2c::EnumGameRoom_0E>& room_entries) {
    const packets::s2c::EnumGameRoom_0E fallback = room_entries.empty() ? packets::s2c::EnumGameRoom_0E{} : room_entries.front();

    packets::s2c::FullRoomStateReply_84 packet;
    packet.string0 = get_string(object, "string0", fallback.primary_name);
    packet.string1 = get_string(object, "string1", fallback.secondary_name);
    packet.field_02 = get_u32(object, "field_02", 0);
    packet.field_06 = get_u32(object, "field_06", 0);
    packet.field_0a = get_u32(object, "field_0a", 0);
    packet.unused_0e_11_u32 = get_u32(object, "unused_0e_11_u32", 0);
    packet.field_12 = get_u32(object, "field_12", 0);
    packet.field_16 = get_u32(object, "field_16", 0);
    packet.field_1a = get_u8(object, "field_1a", 0);
    const auto field_1b_values = get_u32_list(object, "field_1b", 8, std::vector<std::uint32_t>(8, 0));
    for (std::size_t index = 0; index < packet.field_1b.size(); ++index) {
        packet.field_1b[index] = field_1b_values[index];
    }
    packet.field_3b = get_u32(object, "field_3b", 0);
    packet.field_3f = get_u32(object, "field_3f", 0);
    return packet;
}

packets::s2c::EnumQuickSlot_44 parse_packet_44_entry(const JsonObject& object) {
    packets::s2c::EnumQuickSlot_44 packet;
    packet.entry_key = get_alias<std::uint32_t>(object, "entry_key", "field_00", 0, get_u32);
    packet.slot_index = get_alias<std::uint8_t>(object, "slot_index", "field_04", 0, get_u8);
    packet.quickslot_entry_id = get_alias<std::uint32_t>(object, "quickslot_entry_id", "field_05", 0, get_u32);
    packet.slot_state = get_alias<std::uint8_t>(object, "slot_state", "field_09", 0, get_u8);
    packet.display_lookup_key = get_alias<std::uint16_t>(object, "display_lookup_key", "field_0a", 0, get_u16);
    packet.duration_minutes = get_alias<std::uint32_t>(object, "duration_minutes", "field_0e", 0, get_u32);
    return packet;
}

packets::s2c::StreamedTextList_52 parse_packet_52_entry(const JsonObject& object) {
    packets::s2c::StreamedTextList_52 packet;
    packet.field_00 = get_u8(object, "field_00", 0);
    packet.text = get_string(object, "text");
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

packets::s2c::FullRoomStateSlot_85 parse_packet_85_entry(const JsonObject& object) {
    packets::s2c::FullRoomStateSlot_85 packet;
    packet.lookup_key_00 = get_u8(object, "lookup_key_00", 0);
    packet.field_01 = get_u16(object, "field_01", 0);
    packet.field_03 = get_u16(object, "field_03", 0);
    packet.field_05 = get_u32(object, "field_05", 0);
    packet.field_09 = get_u32(object, "field_09", 0);
    packet.field_0d = get_u32(object, "field_0d", 0);
    packet.field_11 = get_u16(object, "field_11", 0);
    packet.field_13 = get_u16(object, "field_13", 0);
    packet.field_15 = get_u16(object, "field_15", 0);
    return packet;
}

packets::s2c::StateUpdate_9E parse_packet_9e_entry(const JsonObject& object) {
    packets::s2c::StateUpdate_9E packet;
    packet.state_entry_id = get_alias<std::uint32_t>(object, "state_entry_id", "field_00", 0, get_u32);
    packet.state_kind = get_alias<std::uint8_t>(object, "state_kind", "field_04", 0, get_u8);
    packet.state_value = get_alias<std::uint32_t>(object, "state_value", "field_05", 0, get_u32);
    packet.state_aux_value = get_alias<std::uint32_t>(object, "state_aux_value", "field_09", 0, get_u32);
    packet.state_flag = get_alias<std::uint8_t>(object, "state_flag", "field_0d", 0, get_u8);
    packet.display_lookup_key = get_alias<std::uint8_t>(object, "display_lookup_key", "lookup_key_0e", 0, get_u8);
    packet.display_resource_id = get_alias<std::uint32_t>(object, "display_resource_id", "field_0f", 0, get_u32);
    packet.record_flag_58 = get_alias<std::uint8_t>(object, "record_flag_58", "field_13", 0, get_u8);
    packet.record_flag_59 = get_alias<std::uint8_t>(object, "record_flag_59", "field_14", 0, get_u8);
    return packet;
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

template <typename PacketT, typename Parser>
std::vector<PacketT> parse_object_array(const JsonArray& array, Parser parser) {
    std::vector<PacketT> packets;
    packets.reserve(array.size());
    for (const auto& value : array) {
        if (!value.is_object()) {
            throw std::runtime_error("JSON packet entry must be an object");
        }
        packets.push_back(parser(value.as_object()));
    }
    return packets;
}

}  // namespace

OptionsStore::OptionsStore(std::filesystem::path path) : path_(std::move(path)) {}

ServerOptions OptionsStore::load() {
    std::optional<std::filesystem::file_time_type> mtime{};
    try {
        if (std::filesystem::exists(path_)) {
            mtime = std::filesystem::last_write_time(path_);
        }
    } catch (...) {
        mtime.reset();
    }

    if (mtime == cached_mtime_) {
        return cached_options_;
    }

    cached_options_ = load_uncached();
    cached_mtime_ = mtime;
    return cached_options_;
}

ServerOptions OptionsStore::load_uncached() const {
    if (!std::filesystem::exists(path_)) {
        return {};
    }

    const auto root = LoadJsonFile(path_);
    if (!root.is_object()) {
        throw std::runtime_error("options file must contain a top-level object");
    }
    const auto& object = root.as_object();

    ServerOptions options;

    options.packet_a3_entries = parse_object_array<packets::s2c::EnumChannel_A3>(
        array_value(root.find("packet_a3_entries"), "packet_a3_entries"),
        parse_packet_a3_entry);

    options.packet_0e_entries = parse_object_array<packets::s2c::EnumGameRoom_0E>(
        array_value(root.find("packet_0e_entries"), "packet_0e_entries"),
        parse_packet_0e_entry);

    if (const auto* packet_0f = root.find("packet_0f")) {
        const auto& section = object_value(packet_0f, "packet_0f");
        options.has_packet_0f_room_count = section.contains("room_count");
        options.packet_0f_room_count = get_u16(section, "room_count", 0);
    }

    options.packet_0d = parse_packet_0d(
        object_value(root.find("packet_0d"), "packet_0d"),
        options.packet_0e_entries);

    if (const auto* packet_42 = root.find("packet_42_entries")) {
        options.has_packet_42_entries = true;
        options.packet_42_entries = parse_object_array<packets::s2c::TrainingGuardState_42>(
            array_value(packet_42, "packet_42_entries"),
            parse_guard_42);
    }

    options.packet_84 = parse_packet_84(
        object_value(root.find("packet_84"), "packet_84"),
        options.packet_0e_entries);

    options.packet_85_entries = parse_object_array<packets::s2c::FullRoomStateSlot_85>(
        array_value(root.find("packet_85_entries"), "packet_85_entries"),
        parse_packet_85_entry);

    options.packet_44_entries = parse_object_array<packets::s2c::EnumQuickSlot_44>(
        array_value(root.find("packet_44_entries"), "packet_44_entries"),
        parse_packet_44_entry);

    options.packet_52_entries = parse_object_array<packets::s2c::StreamedTextList_52>(
        array_value(root.find("packet_52_entries"), "packet_52_entries"),
        parse_packet_52_entry);

    options.packet_73_entries = parse_object_array<packets::s2c::UpdateSkillList_73>(
        array_value(root.find("packet_73_entries"), "packet_73_entries"),
        parse_packet_73_entry);

    options.packet_9e_entries = parse_object_array<packets::s2c::StateUpdate_9E>(
        array_value(root.find("packet_9e_entries"), "packet_9e_entries"),
        parse_packet_9e_entry);

    options.packet_6e_entries = parse_object_array<packets::s2c::UpdateGuardList_6E>(
        array_value(root.find("packet_6e_entries"), "packet_6e_entries"),
        parse_guard_6e);

    options.packet_70 = parse_packet_70(object_value(root.find("packet_70"), "packet_70"));

    return options;
}

}  // namespace cpp_server::core
