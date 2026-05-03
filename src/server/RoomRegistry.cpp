#include "cpp_server/server/RoomRegistry.h"

#include <algorithm>

namespace cpp_server::server {

std::vector<packets::s2c::EnumGameRoom_0E> RoomRegistry::entries() const {
    std::scoped_lock lock(mutex_);
    std::vector<packets::s2c::EnumGameRoom_0E> result;
    result.reserve(rooms_.size());
    for (const auto& room : rooms_) {
        result.push_back(room.entry);
    }
    return result;
}

std::optional<packets::s2c::EnumGameRoom_0E> RoomRegistry::find_for_host(
    int host_client_id,
    const std::optional<std::string>& host_login_id) const {
    std::scoped_lock lock(mutex_);
    const auto room_it = std::find_if(
        rooms_.begin(),
        rooms_.end(),
        [&](const auto& room) { return matches_host(room, host_client_id, host_login_id); });
    if (room_it == rooms_.end()) {
        return std::nullopt;
    }
    return room_it->entry;
}

RoomRegistry::UpsertResult RoomRegistry::upsert(
    int host_client_id,
    std::optional<std::string> host_login_id,
    packets::s2c::EnumGameRoom_0E entry) {
    std::scoped_lock lock(mutex_);
    auto room_it = std::find_if(
        rooms_.begin(),
        rooms_.end(),
        [&](const auto& room) { return matches_host(room, host_client_id, host_login_id); });

    if (room_it == rooms_.end()) {
        entry.room_id = next_room_id_++;
        rooms_.push_back(RuntimeRoom{host_client_id, std::move(host_login_id), std::move(entry)});
        return UpsertResult{true, rooms_.back().entry};
    }

    entry.room_id = room_it->entry.room_id;
    room_it->host_client_id = host_client_id;
    room_it->host_login_id = std::move(host_login_id);
    room_it->entry = std::move(entry);
    return UpsertResult{false, room_it->entry};
}

bool RoomRegistry::remove_for_host(int host_client_id, const std::optional<std::string>& host_login_id) {
    std::scoped_lock lock(mutex_);
    const auto old_size = rooms_.size();
    rooms_.erase(
        std::remove_if(
            rooms_.begin(),
            rooms_.end(),
            [&](const auto& room) { return matches_host(room, host_client_id, host_login_id); }),
        rooms_.end());
    return rooms_.size() != old_size;
}

bool RoomRegistry::matches_host(
    const RuntimeRoom& room,
    int host_client_id,
    const std::optional<std::string>& host_login_id) {
    if (room.host_client_id == host_client_id) {
        return true;
    }
    return host_login_id && room.host_login_id && *room.host_login_id == *host_login_id;
}

}  // namespace cpp_server::server
