#pragma once

#include <span>

#include "cpp_server/packets/shared/RawPacketBase.h"
#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::packets::c2s {

struct EnumHakanTrainingClearInfo_9D : shared::RawPacketBase<0x9D> {
    static EnumHakanTrainingClearInfo_9D deserialize(std::span<const std::uint8_t> payload) {
        EnumHakanTrainingClearInfo_9D packet;
        packet.payload_bytes = shared::CopyPayload(payload);
        return packet;
    }
};

}  // namespace cpp_server::packets::c2s
