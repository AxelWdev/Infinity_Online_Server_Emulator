#pragma once

#include <span>
#include <stdexcept>

#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct BuySkill_75 {
    static constexpr std::uint8_t kOpcode = 0x75;

    std::uint32_t skill_shop_id{};
    std::uint8_t buy_money_selection{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        ByteWriter writer;
        writer.write_u32(skill_shop_id);
        writer.write_u8(buy_money_selection);
        return writer.take();
    }

    static BuySkill_75 deserialize(std::span<const std::uint8_t> payload) {
        if (payload.size() != 5) {
            throw std::runtime_error("BuySkill_75 expects a 5-byte payload");
        }

        ByteReader reader(payload);
        BuySkill_75 packet;
        packet.skill_shop_id = reader.read_u32();
        packet.buy_money_selection = reader.read_u8();
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
