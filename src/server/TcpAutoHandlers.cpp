#include "cpp_server/server/TcpLzssServer.h"

#include <span>

#include "cpp_server/core/PacketLog.h"
#include "cpp_server/core/Utf.h"
#include "cpp_server/packets/shared/PacketSupport.h"
#include "cpp_server/server/OptionPacketHelpers.h"
#include "cpp_server/udp/UdpProtocol.h"

namespace cpp_server::server {

void TcpLzssServer::auto_echo_frame(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    const int delay_ms = config_.auto_echo_delays_ms.contains(frame.opcode)
                             ? config_.auto_echo_delays_ms.at(frame.opcode)
                             : config_.auto_delay_ms;
    start_delayed_action(
        session,
        "opcode=0x" + core::HexBytes(std::span<const std::uint8_t>(&frame.opcode, 1)) + " echo logical",
        delay_ms,
        [this, session, frame]() {
            session->send_logical_frame(frame, logger_);
        });
}

void TcpLzssServer::auto_send_empty_completion(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    const auto packet = core::BuildLogicalPacket(frame.opcode);
    start_delayed_action(
        session,
        "opcode=0x" + core::HexBytes(std::span<const std::uint8_t>(&frame.opcode, 1)) + " -> 0x" +
            core::HexBytes(std::span<const std::uint8_t>(&frame.opcode, 1)) + " reply logical",
        config_.auto_delay_ms,
        [this, session, packet]() {
            session->send_logical_frame(packet, logger_);
        });
}

void TcpLzssServer::auto_send_paired_completion(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    const std::uint8_t reply_opcode = static_cast<std::uint8_t>(frame.opcode + 1U);
    const auto packet = core::BuildLogicalPacket(reply_opcode);
    start_delayed_action(
        session,
        "opcode=0x" + core::HexBytes(std::span<const std::uint8_t>(&frame.opcode, 1)) + " -> 0x" +
            core::HexBytes(std::span<const std::uint8_t>(&reply_opcode, 1)) + " reply logical",
        config_.auto_delay_ms,
        [this, session, packet]() {
            session->send_logical_frame(packet, logger_);
        });
}

void TcpLzssServer::handle_observed_no_response_packet(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    if (frame.opcode == 0x05 && frame.payload.size() >= 2) {
        const auto payload = std::span<const std::uint8_t>(frame.payload);
        const auto declared_chars = udp::ReadU16Le(payload.subspan(0, 2));
        const auto text = core::DecodeUtf16Le(payload.subspan(2));
        logger_.log("[chat] client=" + std::to_string(session->client_id) +
                    " opcode=0x05 observed chat/control send declared_chars=" +
                    std::to_string(declared_chars) + " text='" + text +
                    "'; no server echo/response proven");
    }

    if (frame.opcode == 0x33 && frame.payload.size() == 12) {
        const auto payload = std::span<const std::uint8_t>(frame.payload);
        const auto initialized_word = udp::ReadU16Le(payload);
        const auto initialized_dword = udp::ReadU32Le(payload.subspan(2, 4));
        const auto trailing_stack = payload.subspan(6, 6);
        logger_.log("[udp] client=" + std::to_string(session->client_id) +
                    " opcode=0x33 native sender initializes first_2_bytes=" +
                    std::to_string(initialized_word) + " next_4_bytes=0x" +
                    udp::HexU32(initialized_dword) + " trailing_stack_bytes=" +
                    core::HexBytes(trailing_stack) + "; no response proven");
    }

    logger_.log("[auto] client=" + std::to_string(session->client_id) +
                " opcode=0x" + core::OpcodeHex(frame.opcode) +
                " observed handled with no server response payload=" + core::HexBytes(frame.payload));
}

void TcpLzssServer::auto_send_hakan_training_info(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    (void)frame;
    packets::s2c::EnumHakanTrainingClearInfo_9D packet_9d;
    const auto packet_9d_frame = packets::shared::ToFrame(packet_9d);
    start_delayed_action(session, "opcode=0x9D -> payload-bearing 0x9D reply logical", config_.auto_delay_ms,
                         [this, session, packet_9d_frame]() { session->send_logical_frame(packet_9d_frame, logger_); });
    const auto packet_9e_entries = options().packet_9e_entries;
    if (IsPlaceholderStateUpdateList9E(packet_9e_entries)) {
        logger_.log("[auto] client=" + std::to_string(session->client_id) +
                    " opcode=0x9D suppressed option-file placeholder 0x9E sideband entries");
        return;
    }

    const auto packet_9e_frames = packets::shared::ToFrames(packet_9e_entries);
    for (std::size_t index = 0; index < packet_9e_frames.size(); ++index) {
        const auto packet = packet_9e_frames[index];
        start_delayed_action(
            session,
            "opcode=0x9D -> sideband 0x9E state entry " + std::to_string(index) + " logical",
            config_.auto_delay_ms + static_cast<int>(index + 1) * 50,
            [this, session, packet]() { session->send_logical_frame(packet, logger_); });
    }
}

void TcpLzssServer::auto_send_training_start_guard_state(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    (void)frame;
    (void)schedule_packet_stream(
        session,
        BuildTrainingGuardState42Frames(effective_options_for_session(session)),
        "opcode=0x7F -> payload-bearing 0x42 training entry");
}

void TcpLzssServer::auto_send_enumchannel_reply(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    (void)frame;
    auto packets = packets::shared::ToFrames(options().packet_a3_entries);
    if (packets.empty()) {
        packets::s2c::EnumChannel_A3 packet;
        packet.channel_type_id = config_.auto_enum_channel_type_id;
        packet.channel_selector_id = config_.auto_enum_channel_selector_id;
        packet.name = config_.auto_enum_name;
        packet.packed_ipv4 = auto_enum_packed_ipv4_;
        packet.tcp_port = config_.auto_enum_port;
        packets.push_back(packets::shared::ToFrame(packet));
    }

    schedule_stream_with_commit(
        session,
        packets,
        "enumchannel entry",
        packets::shared::ToFrame(packets::s2c::EnumChannelDone_A4{}),
        "enumchannel done");
}

}  // namespace cpp_server::server
