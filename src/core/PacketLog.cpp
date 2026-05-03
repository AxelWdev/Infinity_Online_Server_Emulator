#include "cpp_server/core/PacketLog.h"

#include <sstream>

namespace cpp_server::core {

std::string OpcodeHex(std::uint8_t opcode) {
    return HexBytes(std::span<const std::uint8_t>(&opcode, 1));
}

std::string PacketInfoText(std::uint8_t opcode) {
    switch (opcode) {
    case 0x01: return "connectlobby()/reconnect-login confidence=confirmed";
    case 0x02: return "post-connectlobby() follow-up confidence=observed";
    case 0x03: return "enterchannel() confidence=confirmed";
    case 0x05: return "chat/control message send confidence=confirmed";
    case 0x0C: return "observed mission-room metadata upload confidence=observed";
    case 0x0D: return "room-enter token send / inbound room-info state reply confidence=confirmed";
    case 0x0E: return "EnumGameRoom() confidence=confirmed";
    case 0x0F: return "EnumGameRoom() completion confidence=confirmed";
    case 0x10: return "game-server endpoint handoff confidence=confirmed";
    case 0x12: return "observed leavegameroom()/room-state reset confidence=observed";
    case 0x16: return "room player slot/state update confidence=confirmed";
    case 0x17: return "room-create completion confidence=observed";
    case 0x1D: return "room player id table / room setup state confidence=confirmed";
    case 0x18: return "observed room-start request confidence=observed";
    case 0x21: return "observed mission room-start request confidence=observed";
    case 0x22: return "observed post-training-start one-byte state send confidence=observed";
    case 0x29: return "mission result trigger ($NextMission/$MissionResult) confidence=confirmed";
    case 0x33: return "observed post-training-start 12-byte state send confidence=observed";
    case 0x34: return "updatelimitediteminfo() confidence=confirmed";
    case 0x35: return "updatelimitediteminfo() completion confidence=confirmed";
    case 0x36: return "observed shop item buy request/ack confidence=observed";
    case 0x39: return "removeitem()/inventory item delete confidence=observed";
    case 0x3F: return "enumitem()/updateitemlist confidence=confirmed";
    case 0x40: return "enumitem()/updateitemlist completion confidence=confirmed";
    case 0x42: return "training guard state confidence=observed";
    case 0x44: return "EnumQuickSlot()/EnumCharQuickSlot() confidence=confirmed";
    case 0x45: return "EnumQuickSlot() completion confidence=confirmed";
    case 0x47: return "observed InsertQuickSlotbyItem() assign quickslot request confidence=observed";
    case 0x48: return "observed equipment apply/remove request confidence=observed";
    case 0x52: return "observed post-0x68 streamed 0x52/0x53 list request confidence=observed";
    case 0x53: return "observed streamed 0x52/0x53 list completion confidence=observed";
    case 0x61: return "observed one-byte state notification, no response known confidence=observed";
    case 0x67: return "observed post-entry pending request that advances on 0x68 confidence=observed";
    case 0x68: return "pending request completion confidence=observed";
    case 0x6B: return "updatecharacterlist() confidence=confirmed";
    case 0x6C: return "updatecharacterlist() completion confidence=confirmed";
    case 0x6D: return "observed mission-room create/open sideband send confidence=observed";
    case 0x6E: return "updateguardlist() confidence=confirmed";
    case 0x6F: return "updateguardlist() completion confidence=confirmed";
    case 0x70: return "updateaccountinfo() confidence=confirmed";
    case 0x71: return "keepalive confidence=confirmed";
    case 0x73: return "updateskilllist() confidence=confirmed";
    case 0x74: return "updateskilllist() completion confidence=confirmed";
    case 0x75: return "observed shop skill buy request/ack confidence=observed";
    case 0x79: return "observed post-room-create room-state sync send confidence=confirmed";
    case 0x7A: return "holdchannel() confidence=confirmed";
    case 0x7F: return "TrainingStart() one-byte mode send confidence=confirmed";
    case 0x84: return "observed full-room-state upload tail / room-host sync confidence=observed";
    case 0x85: return "observed full-room-state slot upload confidence=observed";
    case 0x8D: return "server-driven room-enter trigger that calls TCP_Send_RoomEnterWithToken confidence=confirmed";
    case 0x9D: return "EnumHakanTrainingClearInfo() / inbound 4-flag state update confidence=confirmed";
    case 0x9E: return "inbound payload-bearing state update at +0x4594 confidence=confirmed";
    case 0x9F: return "session/login room-context init payload confidence=confirmed";
    case 0xA1: return "observed empty post-training notification, no response known confidence=observed";
    case 0xA3: return "enumchannel() confidence=confirmed";
    case 0xA4: return "enumchannel() completion confidence=confirmed";
    case 0xA6: return "character-name / nickname confirmation confidence=medium";
    case 0xFF: return "shared error response confidence=confirmed";
    default: return {};
    }
}

std::string PacketSummaryText(const LogicalPacketFrame& frame) {
    std::ostringstream stream;
    stream << "opcode=0x" << OpcodeHex(frame.opcode);
    const auto info = PacketInfoText(frame.opcode);
    if (!info.empty()) {
        stream << " " << info;
    } else {
        stream << " unknown";
    }
    stream << " payload_len=" << frame.payload.size() << " frame_len=" << (frame.payload.size() + 2U);
    return stream.str();
}

std::string FormatPacketLogLine(
    std::string_view direction,
    int client_id,
    const LogicalPacketFrame& frame,
    std::string_view status) {
    std::ostringstream stream;
    stream << "[packet][" << direction << "]";
    if (!status.empty()) {
        stream << "[" << status << "]";
    }
    stream << " client=" << client_id << " " << PacketSummaryText(frame)
           << " frame=" << HexBytes(frame.serialize());
    return stream.str();
}

std::string FormatPacketPayloadLine(
    std::string_view direction,
    int client_id,
    const LogicalPacketFrame& frame) {
    std::ostringstream stream;
    stream << "[packet][" << direction << "][payload] client=" << client_id
           << " opcode=0x" << OpcodeHex(frame.opcode)
           << " bytes=" << HexBytes(frame.payload);
    return stream.str();
}

std::string FormatUnhandledPacketLine(
    int client_id,
    const LogicalPacketFrame& frame,
    std::string_view reason) {
    std::ostringstream stream;
    stream << "[packet][C->S][UNHANDLED] client=" << client_id << " "
           << PacketSummaryText(frame) << " reason=" << reason
           << " payload=" << HexBytes(frame.payload);
    return stream.str();
}

std::optional<LogicalPacketFrame> TryParseSerializedLogicalPacket(std::span<const std::uint8_t> packet) {
    if (packet.size() < 2) {
        return std::nullopt;
    }

    const auto payload_len = packet[1];
    const auto expected_size = static_cast<std::size_t>(payload_len) + 2U;
    if (packet.size() != expected_size) {
        return std::nullopt;
    }

    LogicalPacketFrame frame;
    frame.opcode = packet[0];
    frame.payload.assign(packet.begin() + 2, packet.end());
    return frame;
}

}  // namespace cpp_server::core
