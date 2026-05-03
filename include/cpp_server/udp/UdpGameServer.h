#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <unordered_map>

#include "cpp_server/core/GameDataCatalog.h"
#include "cpp_server/core/Logger.h"
#include "cpp_server/core/SocketPlatform.h"
#include "cpp_server/udp/UdpInitialSyncContext.h"
#include "cpp_server/udp/UdpPeerState.h"

namespace cpp_server::udp {

struct GameServerConfig {
    std::string host{"0.0.0.0"};
    std::uint16_t port{};
    std::uint16_t fallback_port{};
    bool experimental_gameplay_sync{};
};

class GameServer {
public:
    GameServer() = default;
    ~GameServer();

    GameServer(const GameServer&) = delete;
    GameServer& operator=(const GameServer&) = delete;

    void start(
        GameServerConfig config,
        core::Logger& logger,
        const core::GameDataCatalog& game_data_catalog,
        InitialSyncProvider initial_sync_provider,
        MissionResultCallback mission_result_callback);
    void stop();

    [[nodiscard]] std::uint16_t bound_port() const;

private:
    void loop();

    GameServerConfig config_{};
    core::Logger* logger_{};
    const core::GameDataCatalog* game_data_catalog_{};
    InitialSyncProvider initial_sync_provider_{};
    core::SocketHandle socket_{core::kInvalidSocket};
    std::atomic<bool> running_{false};
    std::thread thread_{};
    std::unordered_map<std::string, PeerState> peers_{};
    MissionResultCallback mission_result_callback_{};
};

}  // namespace cpp_server::udp
