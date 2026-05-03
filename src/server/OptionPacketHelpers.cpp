#include "cpp_server/server/OptionPacketHelpers.h"

#include <algorithm>

#include "cpp_server/packets/shared/PacketSupport.h"

namespace cpp_server::server {

namespace {

bool IsPlaceholderStateUpdate9E(const packets::s2c::StateUpdate_9E& packet) {
    return packet.state_kind == 1 && packet.state_value == 0 && packet.state_aux_value == 0 && packet.state_flag == 0 &&
           packet.display_lookup_key == 0 && packet.display_resource_id == 0 && packet.record_flag_58 == 0 &&
           packet.record_flag_59 == 0;
}

}  // namespace

bool IsPlaceholderTextList52(const std::vector<packets::s2c::StreamedTextList_52>& packets) {
    return packets.size() == 1 && packets.front().field_00 == 0 && packets.front().text.empty();
}

bool IsPlaceholderStateUpdateList9E(const std::vector<packets::s2c::StateUpdate_9E>& packets) {
    return !packets.empty() && std::all_of(
                                   packets.begin(),
                                   packets.end(),
                                   [](const auto& packet) { return IsPlaceholderStateUpdate9E(packet); });
}

std::vector<core::LogicalPacketFrame> BuildTrainingGuardState42Frames(const core::ServerOptions& options) {
    std::vector<packets::s2c::TrainingGuardState_42> packets;
    if (options.has_packet_42_entries) {
        packets = options.packet_42_entries;
    } else if (!options.packet_6e_entries.empty()) {
        const auto preferred_guard_id = options.packet_70.deploy_slot_0_id;
        auto it = std::find_if(
            options.packet_6e_entries.begin(),
            options.packet_6e_entries.end(),
            [preferred_guard_id](const auto& entry) { return entry.guard_instance_id == preferred_guard_id; });

        const auto& selected = (it != options.packet_6e_entries.end()) ? *it : options.packet_6e_entries.front();
        packets::s2c::TrainingGuardState_42 converted;
        converted.guard_instance_id = selected.guard_instance_id;
        converted.guard_nickname = selected.guard_nickname;
        converted.guard_kind_id = selected.guard_kind_id;
        converted.selectable_flag = selected.selectable_flag;
        converted.equipped_item_slot_0_id = selected.equipped_item_slot_0_id;
        converted.equipped_item_slot_1_id = selected.equipped_item_slot_1_id;
        packets.push_back(std::move(converted));
    }

    return packets::shared::ToFrames(packets);
}

}  // namespace cpp_server::server
