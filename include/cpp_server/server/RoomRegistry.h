#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "cpp_server/packets/s2c/EnumGameRoom_0E.h"

namespace cpp_server::server {

class RoomRegistry {
public:
    struct UpsertResult {
        bool created{};
        packets::s2c::EnumGameRoom_0E entry{};
    };

    [[nodiscard]] std::vector<packets::s2c::EnumGameRoom_0E> entries() const;
    [[nodiscard]] std::optional<packets::s2c::EnumGameRoom_0E> find_for_host(
        int host_client_id,
        const std::optional<std::string>& host_login_id) const;
    UpsertResult upsert(
        int host_client_id,
        std::optional<std::string> host_login_id,
        packets::s2c::EnumGameRoom_0E entry);
    [[nodiscard]] bool remove_for_host(int host_client_id, const std::optional<std::string>& host_login_id);

private:
    struct RuntimeRoom {
        int host_client_id{};
        std::optional<std::string> host_login_id{};
        packets::s2c::EnumGameRoom_0E entry{};
    };

    [[nodiscard]] static bool matches_host(
        const RuntimeRoom& room,
        int host_client_id,
        const std::optional<std::string>& host_login_id);

    mutable std::mutex mutex_{};
    std::vector<RuntimeRoom> rooms_{};
    std::uint16_t next_room_id_{1};
};

}  // namespace cpp_server::server
