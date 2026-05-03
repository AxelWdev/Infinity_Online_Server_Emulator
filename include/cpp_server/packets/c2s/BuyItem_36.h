#pragma once

#include <span>
#include <stdexcept>

#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct BuyItem_36 {
    static constexpr std::uint8_t kOpcode = 0x36;

    std::uint32_t item_id{};
    std::uint16_t field_04{};
    std::uint8_t buy_money_selection{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        ByteWriter writer;
        writer.write_u32(item_id);
        writer.write_u16(field_04);
        writer.write_u8(buy_money_selection);
        return writer.take();
    }

    static BuyItem_36 deserialize(std::span<const std::uint8_t> payload) {
        if (payload.size() != 7) {
            throw std::runtime_error("BuyItem_36 expects a 7-byte payload");
        }

        ByteReader reader(payload);
        BuyItem_36 packet;
        packet.item_id = reader.read_u32();
        packet.field_04 = reader.read_u16();
        packet.buy_money_selection = reader.read_u8();
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
