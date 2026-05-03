#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "cpp_server/core/Utf.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct UpdateAccountInfo_70 {
    static constexpr std::uint8_t kOpcode = 0x70;

    std::uint32_t exp_current{};
    std::uint32_t field_04_unknown{};
    std::uint8_t level_raw{};
    std::uint32_t field_09_unknown{};
    std::uint32_t luna{};
    std::uint32_t cash{};
    std::uint32_t event_cash{};
    std::uint32_t total_kill_count{};
    std::uint32_t profile_flags{};
    std::uint32_t indicator_max_or_limit{};
    std::uint32_t field_25_unknown{};
    std::uint32_t deploy_slot_0_id{};
    std::uint32_t deploy_slot_1_id{};
    std::uint32_t deploy_slot_2_id{};
    std::uint32_t deploy_slot_3_id{};
    std::uint32_t bp{};
    std::uint32_t clan_contribution{};
    std::string string0_display_label{};
    std::string clan_name{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        const auto encoded_string0 = core::EncodeUtf16Le(string0_display_label);
        const auto encoded_string1 = core::EncodeUtf16Le(clan_name);
        const auto string0_chars = encoded_string0.size() / 2;
        const auto string1_chars = encoded_string1.size() / 2;
        if (string0_chars > 0xFF || string1_chars > 0xFF) {
            throw std::runtime_error("UpdateAccountInfo_70 string field is too long");
        }

        ByteWriter writer;
        writer.write_u32(exp_current);
        writer.write_u32(field_04_unknown);
        writer.write_u8(level_raw);
        writer.write_u32(field_09_unknown);
        writer.write_u32(luna);
        writer.write_u32(cash);
        writer.write_u32(event_cash);
        writer.write_u32(total_kill_count);
        writer.write_u32(profile_flags);
        writer.write_u32(indicator_max_or_limit);
        writer.write_u32(field_25_unknown);
        writer.write_u32(deploy_slot_0_id);
        writer.write_u32(deploy_slot_1_id);
        writer.write_u32(deploy_slot_2_id);
        writer.write_u32(deploy_slot_3_id);
        writer.write_u32(bp);
        writer.write_u32(clan_contribution);
        writer.write_u8(static_cast<std::uint8_t>(string0_chars));
        writer.write_u8(static_cast<std::uint8_t>(string1_chars));
        writer.write_bytes(encoded_string0);
        writer.write_bytes(encoded_string1);
        return writer.take();
    }

    static UpdateAccountInfo_70 deserialize(std::span<const std::uint8_t> payload) {
        ByteReader reader(payload);
        UpdateAccountInfo_70 packet;
        packet.exp_current = reader.read_u32();
        packet.field_04_unknown = reader.read_u32();
        packet.level_raw = reader.read_u8();
        packet.field_09_unknown = reader.read_u32();
        packet.luna = reader.read_u32();
        packet.cash = reader.read_u32();
        packet.event_cash = reader.read_u32();
        packet.total_kill_count = reader.read_u32();
        packet.profile_flags = reader.read_u32();
        packet.indicator_max_or_limit = reader.read_u32();
        packet.field_25_unknown = reader.read_u32();
        packet.deploy_slot_0_id = reader.read_u32();
        packet.deploy_slot_1_id = reader.read_u32();
        packet.deploy_slot_2_id = reader.read_u32();
        packet.deploy_slot_3_id = reader.read_u32();
        packet.bp = reader.read_u32();
        packet.clan_contribution = reader.read_u32();
        const auto string0_chars = reader.read_u8();
        const auto string1_chars = reader.read_u8();
        packet.string0_display_label = core::DecodeUtf16Le(reader.read_bytes(string0_chars * 2));
        packet.clan_name = core::DecodeUtf16Le(reader.read_bytes(string1_chars * 2));
        return packet;
    }
};

}  // namespace cpp_server::packets::s2c
