#include "cpp_server/server/ClientSession.h"

#include "cpp_server/core/ByteBuffer.h"
#include "cpp_server/core/PacketLog.h"

namespace cpp_server::server {

void ClientSession::send_logical_frame(const core::LogicalPacketFrame& packet, core::Logger& logger) {
    send_logical_bytes(packet.serialize(), logger);
}

void ClientSession::send_logical_bytes(std::span<const std::uint8_t> packet, core::Logger& logger) {
    const auto encoded = encoder.encode(packet);
    {
        std::scoped_lock lock(send_mutex);
        core::SendAll(socket, encoded);
    }
    if (const auto frame = core::TryParseSerializedLogicalPacket(packet)) {
        logger.log(core::FormatPacketLogLine("S->C", client_id, *frame, "SENT"));
        if (!frame->payload.empty()) {
            logger.log(core::FormatPacketPayloadLine("S->C", client_id, *frame));
        }
    } else {
        logger.log("[packet][S->C][SENT] client=" + std::to_string(client_id) +
                   " logical_bytes=" + std::to_string(packet.size()) +
                   " frame=unparsed bytes=" + core::HexBytes(packet));
    }
    logger.debug("[packet][S->C][lzss] client=" + std::to_string(client_id) +
                 " bytes=" + std::to_string(encoded.size()) + " data=" + core::HexBytes(encoded));
}

void ClientSession::send_raw_lzss(std::span<const std::uint8_t> packet, core::Logger& logger) {
    {
        std::scoped_lock lock(send_mutex);
        core::SendAll(socket, packet);
    }
    logger.log("[packet][S->C][raw-lzss][SENT] client=" + std::to_string(client_id) +
               " bytes=" + std::to_string(packet.size()));
    logger.debug("[packet][S->C][raw-lzss] client=" + std::to_string(client_id) +
                 " data=" + core::HexBytes(packet));
}

void ClientSession::close() {
    if (closed.exchange(true)) {
        return;
    }
    core::ShutdownSocket(socket);
    core::CloseSocket(socket);
    socket = core::kInvalidSocket;
}

}  // namespace cpp_server::server
