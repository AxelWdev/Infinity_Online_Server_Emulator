#pragma once

#include <span>
#include <stdexcept>

#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct RemoveItem_39 {
    static constexpr std::uint8_t kOpcode = 0x39;

    std::uint32_t item_id{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        ByteWriter writer;
        writer.write_u32(item_id);
        return writer.take();
    }

    static RemoveItem_39 deserialize(std::span<const std::uint8_t> payload) {
        if (payload.size() != 4) {
            throw std::runtime_error("RemoveItem_39 expects a 4-byte payload");
        }

        ByteReader reader(payload);
        RemoveItem_39 packet;
        packet.item_id = reader.read_u32();
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
