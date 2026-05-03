#include "cpp_server/server/TcpLzssServer.h"

#include <optional>
#include <sstream>

#include "cpp_server/game/InventoryService.h"
#include "cpp_server/packets/shared/PacketSupport.h"
#include "cpp_server/server/OptionPacketHelpers.h"

namespace cpp_server::server {

void TcpLzssServer::auto_send_character_list(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    (void)frame;
    const auto effective = effective_options_for_session(session);
    logger_.log("[auto] client=" + std::to_string(session->client_id) + " opcode=0x6B source=" +
                character_list_source_for_session(session) + " -> streaming " +
                std::to_string(effective.packet_6b_entries.size()) + " character entries");
    schedule_stream_with_commit(
        session,
        packets::shared::ToFrames(effective.packet_6b_entries),
        "opcode=0x6B -> streamed 0x6B entry",
        core::BuildLogicalPacket(0x6C),
        "opcode=0x6B -> 0x6C commit logical");
}

void TcpLzssServer::auto_send_item_list(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    (void)frame;
    std::optional<std::string> login_id;
    {
        std::scoped_lock lock(session->state_mutex);
        login_id = session->authenticated_login_id;
    }
    if (login_id) {
        if (auto profile = account_database_.find_packet_profile(*login_id); profile && profile->inventory) {
            std::vector<std::uint32_t> expanded_package_ids;
            auto updated_inventory = *profile->inventory;
            const auto repaired_misexpanded_packages = game::RepairMisexpandedPackages(updated_inventory);
            const auto expanded_stored_packages =
                game::ExpandStoredPackageTokens(game_data_catalog_, updated_inventory, expanded_package_ids);
            if (repaired_misexpanded_packages || expanded_stored_packages) {
                profile->inventory = updated_inventory;
                if (account_database_.set_packet_profile(*login_id, *profile)) {
                    if (repaired_misexpanded_packages) {
                        logger_.log("[auto] client=" + std::to_string(session->client_id) +
                                    " opcode=0x3F repaired previously misexpanded Baekho swimsuit package items for '" +
                                    *login_id + "'");
                    }
                    if (expanded_stored_packages) {
                        std::ostringstream package_log;
                        package_log << "[auto] client=" << session->client_id
                                    << " opcode=0x3F expanded stored package token(s) for '" << *login_id << "':";
                        for (const auto package_item_id : expanded_package_ids) {
                            package_log << " " << package_item_id;
                        }
                        logger_.log(package_log.str());
                    }
                } else {
                    logger_.log("[auto] client=" + std::to_string(session->client_id) +
                                " opcode=0x3F failed to persist expanded package inventory for '" + *login_id + "'");
                }
            }
        }
    }

    const auto effective = effective_options_for_session(session);
    logger_.log("[auto] client=" + std::to_string(session->client_id) + " opcode=0x3F source=" +
                item_list_source_for_session(session) + " -> streaming " +
                std::to_string(effective.packet_3f_entries.size()) + " item entries");
    schedule_stream_with_commit(
        session,
        packets::shared::ToFrames(effective.packet_3f_entries),
        "opcode=0x3F -> streamed 0x3F entry",
        core::BuildLogicalPacket(0x40),
        "opcode=0x3F -> 0x40 commit logical");
}

void TcpLzssServer::auto_send_skill_list(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    (void)frame;
    const auto effective = effective_options_for_session(session);
    logger_.log("[auto] client=" + std::to_string(session->client_id) + " opcode=0x73 -> streaming " +
                std::to_string(effective.packet_73_entries.size()) + " skill entries");
    schedule_stream_with_commit(
        session,
        packets::shared::ToFrames(effective.packet_73_entries),
        "opcode=0x73 -> streamed 0x73 entry",
        core::BuildLogicalPacket(0x74),
        "opcode=0x73 -> 0x74 commit logical");
}

void TcpLzssServer::auto_send_guard_list(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    (void)frame;
    const auto effective = effective_options_for_session(session);
    schedule_stream_with_commit(
        session,
        packets::shared::ToFrames(effective.packet_6e_entries),
        "opcode=0x6E -> streamed 0x6E entry",
        core::BuildLogicalPacket(0x6F),
        "opcode=0x6E -> 0x6F commit logical");
}

void TcpLzssServer::auto_send_quickslot_list(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    (void)frame;
    const auto effective = effective_options_for_session(session);
    schedule_stream_with_commit(
        session,
        packets::shared::ToFrames(effective.packet_44_entries),
        "opcode=0x44 -> streamed 0x44 entry",
        core::BuildLogicalPacket(0x45),
        "opcode=0x44 -> 0x45 commit logical");
}

void TcpLzssServer::auto_send_room_list(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    (void)frame;
    bool should_cleanup_created_room = false;
    {
        std::scoped_lock lock(session->state_mutex);
        should_cleanup_created_room = session->pending_room_leave_request;
    }
    if (should_cleanup_created_room) {
        remove_created_rooms_for_session(session);
        clear_created_room_state_for_session(session);
        logger_.log("[room] client=" + std::to_string(session->client_id) +
                    " opcode=0x0E applied pending 0x12 room-leave cleanup before room-list reply");
    }
    const auto rooms = created_room_entries();
    const auto packets = packets::shared::ToFrames(rooms);
    const auto room_count = static_cast<std::uint16_t>(packets.size());
    logger_.log("[room] client=" + std::to_string(session->client_id) +
                " opcode=0x0E -> streaming " + std::to_string(room_count) +
                " runtime-created room(s)");
    packets::s2c::EnumGameRoomDone_0F done;
    done.room_count = room_count;
    schedule_stream_with_commit(session, packets, "opcode=0x0E -> streamed 0x0E room entry",
                                packets::shared::ToFrame(done), "opcode=0x0E -> 0x0F room commit logical");
}

void TcpLzssServer::auto_send_packet_52_list(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    (void)frame;
    const auto packets = options().packet_52_entries;
    if (IsPlaceholderTextList52(packets)) {
        logger_.log("[auto] client=" + std::to_string(session->client_id) +
                    " opcode=0x52 suppressed option-file placeholder 0x52 text row; sending 0x53 commit only");
        schedule_stream_with_commit(
            session,
            {},
            "opcode=0x52 -> streamed 0x52 entry",
            core::BuildLogicalPacket(0x53),
            "opcode=0x52 -> 0x53 commit logical");
        return;
    }

    schedule_stream_with_commit(
        session,
        packets::shared::ToFrames(packets),
        "opcode=0x52 -> streamed 0x52 entry",
        core::BuildLogicalPacket(0x53),
        "opcode=0x52 -> 0x53 commit logical");
}

}  // namespace cpp_server::server
