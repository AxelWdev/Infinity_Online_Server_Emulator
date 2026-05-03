#pragma once

#include <vector>

#include "cpp_server/core/LogicalPacket.h"
#include "cpp_server/core/OptionsStore.h"
#include "cpp_server/packets/s2c/StateUpdate_9E.h"
#include "cpp_server/packets/s2c/StreamedTextList_52.h"

namespace cpp_server::server {

[[nodiscard]] bool IsPlaceholderTextList52(const std::vector<packets::s2c::StreamedTextList_52>& packets);
[[nodiscard]] bool IsPlaceholderStateUpdateList9E(const std::vector<packets::s2c::StateUpdate_9E>& packets);
[[nodiscard]] std::vector<core::LogicalPacketFrame> BuildTrainingGuardState42Frames(
    const core::ServerOptions& options);

}  // namespace cpp_server::server
