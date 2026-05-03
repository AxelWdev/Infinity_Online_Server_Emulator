#include "cpp_server/server/SessionPacketEffects.h"

namespace cpp_server::server {

void ApplySentPacketSideEffects(const ClientSessionPtr& session, const core::LogicalPacketFrame& logical_packet) {
    if (logical_packet.opcode == 0x8D) {
        std::scoped_lock lock(session->state_mutex);
        session->awaiting_room_enter_token = true;
    }
}

}  // namespace cpp_server::server
