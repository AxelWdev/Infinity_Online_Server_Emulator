#include "cpp_server/server/TcpLzssServer.h"

namespace cpp_server::server {

TcpLzssServer::TcpLzssServer(ServerConfiguration config)
    : config_(std::move(config)),
      options_store_(config_.options_path),
      account_database_(config_.account_database_path),
      game_data_catalog_(config_.account_database_path),
      auto_enum_packed_ipv4_(core::ParseIpv4(config_.auto_enum_ipv4)) {}

TcpLzssServer::~TcpLzssServer() {
    stop();
}

void TcpLzssServer::start() {
    if (running_.exchange(true)) {
        return;
    }

    logger_.configure(config_.debug_log_path);
    game_udp_server_.start(
        udp::GameServerConfig{config_.host, config_.game_udp_port, config_.auto_enum_port, config_.experimental_game_udp_sync},
        logger_,
        game_data_catalog_,
        [this]() { return game_udp_initial_sync_context(); },
        [this](udp::MissionResultEvent event) { send_mission_result_from_udp(std::move(event)); });
    config_.game_udp_port = game_udp_server_.bound_port();

    listener_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (!core::IsValidSocket(listener_)) {
        throw std::runtime_error("failed to create listener socket: " + core::LastSocketErrorText());
    }

    int opt_value = 1;
    setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt_value), sizeof(opt_value));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(config_.port);
    if (inet_pton(AF_INET, config_.host.c_str(), &address.sin_addr) != 1) {
        throw std::runtime_error("invalid bind address: " + config_.host);
    }

    if (bind(listener_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
        throw std::runtime_error("bind failed: " + core::LastSocketErrorText());
    }

    if (listen(listener_, SOMAXCONN) != 0) {
        throw std::runtime_error("listen failed: " + core::LastSocketErrorText());
    }

    logger_.log("[server] listening on " + config_.host + ":" + std::to_string(config_.port));
    logger_.log("[server] options file: " + config_.options_path.string());
    logger_.log("[server] account database: " + account_database_.path().string());
    logger_.log("[server] debug log: " + logger_.debug_log_path_text());
    logger_.log(std::string("[game-udp] experimental gameplay sync: ") +
                (config_.experimental_game_udp_sync ? "enabled" : "disabled"));

    accept_thread_ = std::thread(&TcpLzssServer::accept_loop, this);
}

void TcpLzssServer::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    core::CloseSocket(listener_);
    listener_ = core::kInvalidSocket;
    game_udp_server_.stop();

    std::vector<ClientSessionPtr> sessions;
    {
        std::scoped_lock lock(sessions_mutex_);
        for (const auto& [client_id, session] : sessions_) {
            (void)client_id;
            sessions.push_back(session);
        }
    }
    for (const auto& session : sessions) {
        session->close();
    }

    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
    {
        std::scoped_lock lock(client_threads_mutex_);
        for (auto& thread : client_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        client_threads_.clear();
    }

    packet_scheduler_.join_all();

    logger_.close();
}

}  // namespace cpp_server::server
