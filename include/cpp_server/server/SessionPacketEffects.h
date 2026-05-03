#pragma once

#include "cpp_server/core/LogicalPacket.h"
#include "cpp_server/server/ClientSession.h"

namespace cpp_server::server {

void ApplySentPacketSideEffects(const ClientSessionPtr& session, const core::LogicalPacketFrame& logical_packet);

}  // namespace cpp_server::server
