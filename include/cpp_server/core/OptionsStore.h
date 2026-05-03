#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

#include "cpp_server/packets/s2c/PacketCatalog.h"

namespace cpp_server::core {

struct ServerOptions {
    std::vector<packets::s2c::EnumChannel_A3> packet_a3_entries{};
    std::vector<packets::s2c::EnumGameRoom_0E> packet_0e_entries{};
    bool has_packet_0f_room_count{};
    std::uint16_t packet_0f_room_count{};
    packets::s2c::RoomInfo_0D packet_0d{};
    bool has_packet_42_entries{};
    std::vector<packets::s2c::TrainingGuardState_42> packet_42_entries{};
    std::vector<packets::s2c::UpdateItemList_3F> packet_3f_entries{};
    packets::s2c::FullRoomStateReply_84 packet_84{};
    std::vector<packets::s2c::FullRoomStateSlot_85> packet_85_entries{};
    std::vector<packets::s2c::UpdateCharacterList_6B> packet_6b_entries{};
    std::vector<packets::s2c::EnumQuickSlot_44> packet_44_entries{};
    std::vector<packets::s2c::StreamedTextList_52> packet_52_entries{};
    std::vector<packets::s2c::UpdateSkillList_73> packet_73_entries{};
    std::vector<packets::s2c::StateUpdate_9E> packet_9e_entries{};
    std::vector<packets::s2c::UpdateGuardList_6E> packet_6e_entries{};
    packets::s2c::UpdateAccountInfo_70 packet_70{};
};

class OptionsStore {
public:
    explicit OptionsStore(std::filesystem::path path);

    [[nodiscard]] ServerOptions load();
    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    [[nodiscard]] ServerOptions load_uncached() const;

    std::filesystem::path path_{};
    std::optional<std::filesystem::file_time_type> cached_mtime_{};
    ServerOptions cached_options_{};
};

}  // namespace cpp_server::core
