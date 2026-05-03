#include <filesystem>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "cpp_server/core/ByteBuffer.h"
#include "cpp_server/server/TcpLzssServer.h"

namespace {

std::optional<std::filesystem::path> find_in_parents(std::filesystem::path start, const std::filesystem::path& relative) {
    for (int depth = 0; depth < 8; ++depth) {
        const auto candidate = start / relative;
        if (std::filesystem::exists(candidate)) {
            return std::filesystem::weakly_canonical(candidate);
        }
        if (!start.has_parent_path()) {
            break;
        }
        start = start.parent_path();
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> find_cpp_server_root(std::filesystem::path start) {
    for (int depth = 0; depth < 8; ++depth) {
        if (std::filesystem::exists(start / "CMakeLists.txt") &&
            std::filesystem::exists(start / "account_database.json")) {
            return std::filesystem::weakly_canonical(start);
        }

        const auto nested = start / "CPP_Server";
        if (std::filesystem::exists(nested / "CMakeLists.txt") &&
            std::filesystem::exists(nested / "account_database.json")) {
            return std::filesystem::weakly_canonical(nested);
        }

        if (!start.has_parent_path()) {
            break;
        }
        start = start.parent_path();
    }
    return std::nullopt;
}

std::filesystem::path default_options_path() {
    if (const auto root = find_cpp_server_root(std::filesystem::current_path())) {
        return *root / "tcp_lzss_server_options.json";
    }
    return "tcp_lzss_server_options.json";
}

std::filesystem::path default_account_database_path() {
    if (const auto root = find_cpp_server_root(std::filesystem::current_path())) {
        return *root / "account_database.json";
    }
    return "account_database.json";
}

std::filesystem::path default_debug_log_path() {
    if (const auto root = find_cpp_server_root(std::filesystem::current_path())) {
        using clock = std::chrono::system_clock;
        const auto now = clock::now();
        const auto whole = clock::to_time_t(now);
        std::tm local_tm{};
#ifdef _WIN32
        localtime_s(&local_tm, &whole);
#else
        localtime_r(&whole, &local_tm);
#endif
        std::ostringstream name;
        name << std::put_time(&local_tm, "%Y%m%d_%H%M%S") << "_tcp_udp_mission_server.log";
        return *root / "logs" / "server" / name.str();
    }
    return "tcp_udp_mission_server.log";
}

void print_help() {
    std::cout << "commands:\n"
              << "  help\n"
              << "  clients\n"
              << "  debug-log\n"
              << "  use <client_id>\n"
              << "  send-logical <full logical frame hex>\n"
              << "  send-frame <opcode_hex> [payload_hex]\n"
              << "  send-lzss <full lzss frame hex>\n"
              << "  enumchannel-ex <channel_type_id> <channel_selector_id> <ipv4> <port> <name>\n"
              << "  enumdone\n"
              << "  close\n"
              << "  quit\n";
}

void command_loop(cpp_server::server::TcpLzssServer& server) {
    print_help();
    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) {
            std::cout << '\n';
            return;
        }
        if (line.empty()) {
            continue;
        }

        std::istringstream tokens(line);
        std::vector<std::string> parts;
        for (std::string part; tokens >> part;) {
            parts.push_back(part);
        }
        if (parts.empty()) {
            continue;
        }

        const auto& cmd = parts[0];
        try {
            if (cmd == "help") {
                print_help();
            } else if (cmd == "clients") {
                server.list_clients();
            } else if (cmd == "debug-log") {
                server.logger().log("[server] debug log: " + server.debug_log_path_text());
            } else if (cmd == "use") {
                if (parts.size() != 2) {
                    throw std::runtime_error("usage: use <client_id>");
                }
                server.set_active(std::stoi(parts[1]));
            } else if (cmd == "send-logical") {
                server.send_logical_to_active(cpp_server::core::ParseHexString(line.substr(cmd.size())));
            } else if (cmd == "send-frame") {
                if (parts.size() < 2) {
                    throw std::runtime_error("usage: send-frame <opcode_hex> [payload_hex]");
                }
                const auto opcode = static_cast<std::uint8_t>(std::stoul(parts[1], nullptr, 16));
                const auto payload = (parts.size() > 2)
                                         ? cpp_server::core::ParseHexString(line.substr(line.find(parts[2])))
                                         : cpp_server::core::ByteVector{};
                server.send_frame_to_active(opcode, payload);
            } else if (cmd == "send-lzss") {
                server.send_raw_lzss_to_active(cpp_server::core::ParseHexString(line.substr(cmd.size())));
            } else if (cmd == "enumchannel-ex") {
                if (parts.size() < 6) {
                    throw std::runtime_error(
                        "usage: enumchannel-ex <channel_type_id> <channel_selector_id> <ipv4> <port> <name>");
                }
                const auto channel_type_id = static_cast<std::uint32_t>(std::stoul(parts[1], nullptr, 0));
                const auto channel_selector_id = static_cast<std::uint16_t>(std::stoul(parts[2], nullptr, 0));
                const auto packed_ipv4 = cpp_server::core::ParseIpv4(parts[3]);
                const auto tcp_port = static_cast<std::uint16_t>(std::stoul(parts[4], nullptr, 0));
                const auto name = line.substr(line.find(parts[5]));
                server.send_enumchannel_ex_to_active(channel_type_id, channel_selector_id, packed_ipv4, tcp_port, name);
            } else if (cmd == "enumdone") {
                server.send_enumdone_to_active();
            } else if (cmd == "close") {
                server.close_active();
            } else if (cmd == "quit") {
                return;
            } else {
                server.logger().log("unknown command: " + cmd);
            }
        } catch (const std::exception& ex) {
            server.logger().log(std::string("[error] ") + ex.what());
        }
    }
}

void print_usage() {
    std::cout << "tcp_lzss_server_cpp [--host value] [--port value] [--options-file path] [--account-db-file path]\n"
              << "                   [--debug-log-file path]\n"
              << "                   [--auto-delay-ms value] [--list-stream-start-delay-ms value]\n"
              << "                   [--list-stream-step-ms value] [--auto-reply-9f-ms value] [--auto-reply-a6-ms value]\n"
              << "                   [--auto-enum-channel-type-id value] [--auto-enum-channel-selector-id value]\n"
              << "                   [--auto-enum-ipv4 value] [--auto-enum-port value] [--auto-enum-name value]\n"
              << "                   [--game-udp-port value] [--experimental-game-udp-sync]\n";
}

}  // namespace

int main(int argc, char** argv) {
    cpp_server::server::ServerConfiguration config;
    config.options_path = default_options_path();
    config.account_database_path = default_account_database_path();
    config.debug_log_path = default_debug_log_path();
    config.auto_echo_delays_ms[0x9F] = 250;
    config.auto_echo_delays_ms[0xA6] = 250;

    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        auto require_value = [&](const std::string& flag) -> std::string {
            if (index + 1 >= argc) {
                throw std::runtime_error("missing value for " + flag);
            }
            return argv[++index];
        };

        if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        }
        if (arg == "--host") {
            config.host = require_value(arg);
        } else if (arg == "--port") {
            config.port = static_cast<std::uint16_t>(std::stoul(require_value(arg)));
        } else if (arg == "--options-file") {
            config.options_path = require_value(arg);
        } else if (arg == "--account-db-file") {
            config.account_database_path = require_value(arg);
        } else if (arg == "--debug-log-file") {
            config.debug_log_path = require_value(arg);
        } else if (arg == "--auto-delay-ms") {
            config.auto_delay_ms = std::stoi(require_value(arg));
        } else if (arg == "--list-stream-start-delay-ms") {
            config.list_stream_start_delay_ms = std::stoi(require_value(arg));
        } else if (arg == "--list-stream-step-ms") {
            config.list_stream_step_ms = std::stoi(require_value(arg));
        } else if (arg == "--auto-reply-9f-ms") {
            config.auto_echo_delays_ms[0x9F] = std::stoi(require_value(arg));
        } else if (arg == "--auto-reply-a6-ms") {
            config.auto_echo_delays_ms[0xA6] = std::stoi(require_value(arg));
        } else if (arg == "--auto-enum-channel-type-id") {
            config.auto_enum_channel_type_id = static_cast<std::uint32_t>(std::stoul(require_value(arg)));
        } else if (arg == "--auto-enum-channel-selector-id") {
            config.auto_enum_channel_selector_id = static_cast<std::uint16_t>(std::stoul(require_value(arg)));
        } else if (arg == "--auto-enum-ipv4") {
            config.auto_enum_ipv4 = require_value(arg);
        } else if (arg == "--auto-enum-port") {
            config.auto_enum_port = static_cast<std::uint16_t>(std::stoul(require_value(arg)));
        } else if (arg == "--auto-enum-name") {
            config.auto_enum_name = require_value(arg);
        } else if (arg == "--game-udp-port") {
            config.game_udp_port = static_cast<std::uint16_t>(std::stoul(require_value(arg)));
        } else if (arg == "--experimental-game-udp-sync") {
            config.experimental_game_udp_sync = true;
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }

    cpp_server::server::TcpLzssServer server(std::move(config));
    server.start();
    try {
        command_loop(server);
    } catch (...) {
        server.stop();
        throw;
    }
    server.stop();
    return 0;
}
