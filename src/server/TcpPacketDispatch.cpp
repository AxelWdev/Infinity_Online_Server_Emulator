#include "cpp_server/server/TcpLzssServer.h"

#include <cstdint>
#include <utility>

namespace cpp_server::server {

bool TcpLzssServer::maybe_schedule_auto_action(ClientSessionPtr session, const core::LogicalPacketFrame& frame) {
    using Handler = void (TcpLzssServer::*)(ClientSessionPtr, const core::LogicalPacketFrame&);
    struct DispatchEntry {
        std::uint8_t opcode{};
        Handler handler{};
    };

    static constexpr DispatchEntry kDispatchTable[] = {
        {0x01, &TcpLzssServer::handle_connect_lobby},
        {0x02, &TcpLzssServer::handle_observed_no_response_packet},
        {0x03, &TcpLzssServer::auto_send_empty_completion},
        {0x05, &TcpLzssServer::handle_observed_no_response_packet},
        {0x0C, &TcpLzssServer::auto_send_room_create_completion},
        {0x0D, &TcpLzssServer::auto_send_room_info},
        {0x0E, &TcpLzssServer::auto_send_room_list},
        {0x12, &TcpLzssServer::handle_room_leave_request},
        {0x18, &TcpLzssServer::handle_room_start_request},
        {0x21, &TcpLzssServer::handle_room_start_request},
        {0x22, &TcpLzssServer::handle_observed_no_response_packet},
        {0x33, &TcpLzssServer::handle_observed_no_response_packet},
        {0x34, &TcpLzssServer::auto_send_paired_completion},
        {0x36, &TcpLzssServer::handle_buy_item},
        {0x39, &TcpLzssServer::handle_remove_item},
        {0x3F, &TcpLzssServer::auto_send_item_list},
        {0x44, &TcpLzssServer::auto_send_quickslot_list},
        {0x47, &TcpLzssServer::handle_assign_quickslot},
        {0x48, &TcpLzssServer::handle_equip_item},
        {0x52, &TcpLzssServer::auto_send_packet_52_list},
        {0x61, &TcpLzssServer::handle_observed_no_response_packet},
        {0x67, &TcpLzssServer::auto_send_paired_completion},
        {0x6B, &TcpLzssServer::auto_send_character_list},
        {0x6D, &TcpLzssServer::auto_send_room_character_select_state},
        {0x6E, &TcpLzssServer::auto_send_guard_list},
        {0x70, &TcpLzssServer::auto_send_account_info},
        {0x71, &TcpLzssServer::handle_observed_no_response_packet},
        {0x73, &TcpLzssServer::auto_send_skill_list},
        {0x75, &TcpLzssServer::handle_buy_skill},
        {0x79, &TcpLzssServer::handle_observed_no_response_packet},
        {0x7A, &TcpLzssServer::auto_send_empty_completion},
        {0x7F, &TcpLzssServer::auto_send_training_start_guard_state},
        {0x84, &TcpLzssServer::auto_send_full_room_state_commit},
        {0x85, &TcpLzssServer::handle_observed_no_response_packet},
        {0x9D, &TcpLzssServer::auto_send_hakan_training_info},
        {0x9E, &TcpLzssServer::handle_state_update_upload},
        {0x9F, &TcpLzssServer::handle_login_challenge},
        {0xA1, &TcpLzssServer::handle_observed_no_response_packet},
        {0xA3, &TcpLzssServer::auto_send_enumchannel_reply},
        {0xA6, &TcpLzssServer::handle_character_name},
    };

    for (const auto& entry : kDispatchTable) {
        if (entry.opcode != frame.opcode) {
            continue;
        }
        (this->*entry.handler)(std::move(session), frame);
        return true;
    }
    return false;
}

}  // namespace cpp_server::server
