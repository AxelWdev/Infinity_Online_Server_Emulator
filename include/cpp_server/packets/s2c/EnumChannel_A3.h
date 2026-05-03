#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "cpp_server/core/Utf.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::s2c {

struct EnumChannel_A3 {
    static constexpr std::uint8_t kOpcode = 0xA3;

    std::uint32_t channel_type_id{};
    std::uint32_t packed_ipv4{};
    std::uint16_t tcp_port{};
    std::uint16_t channel_selector_id{};
    std::string name{};

    [[nodiscard]] core::ByteVector serialize_payload() const {
        const auto encoded_name = core::EncodeUtf16Le(name);
        const auto name_chars = encoded_name.size() / 2;
        if (name_chars > 0xFF) {
            throw std::runtime_error("EnumChannel_A3 name is too long");
        }

        ByteWriter writer;
        writer.write_u8(static_cast<std::uint8_t>(name_chars));
        writer.write_u32(channel_type_id);
        writer.write_u32(packed_ipv4);
        writer.write_u16(tcp_port);
        writer.write_u16(channel_selector_id);
        writer.write_bytes(encoded_name);
        return writer.take();
    }

    static EnumChannel_A3 deserialize(std::span<const std::uint8_t> payload) {
        ByteReader reader(payload);
        EnumChannel_A3 packet;
        const auto name_chars = reader.read_u8();
        packet.channel_type_id = reader.read_u32();
        packet.packed_ipv4 = reader.read_u32();
        packet.tcp_port = reader.read_u16();
        packet.channel_selector_id = reader.read_u16();
        packet.name = core::DecodeUtf16Le(reader.read_bytes(name_chars * 2));
        return packet;
    }
};

}  // namespace cpp_server::packets::s2c
