#include "cpp_server/core/SocketPlatform.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

struct Endpoint {
    std::string host{"127.0.0.1"};
    std::uint16_t port{};
};

struct Options {
    std::string listen_host{"127.0.0.1"};
    std::uint16_t listen_port{};
    std::filesystem::path log_file{"udp_traffic_logger.log"};
    std::optional<Endpoint> forward_target{};
};

std::string timestamp_text() {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const auto whole = clock::to_time_t(now);
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm local_tm{};
#ifdef _WIN32
    localtime_s(&local_tm, &whole);
#else
    localtime_r(&local_tm, &whole);
#endif

    std::ostringstream stream;
    stream << std::put_time(&local_tm, "%H:%M:%S") << '.' << std::setw(3) << std::setfill('0')
           << millis.count();
    return stream.str();
}

std::string hex_dump(const std::uint8_t* data, std::size_t size) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < size; ++index) {
        if (index != 0) {
            stream << ' ';
        }
        stream << std::setw(2) << static_cast<unsigned>(data[index]);
    }
    return stream.str();
}

void log_line(std::ofstream& file, std::string_view message) {
    const auto line = '[' + timestamp_text() + "] " + std::string(message);
    std::cout << line << '\n';
    file << line << '\n';
    file.flush();
}

sockaddr_in make_address(const std::string& host, std::uint16_t port) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1) {
        throw std::runtime_error("invalid IPv4 address: " + host);
    }
    return address;
}

std::string address_text(const sockaddr_in& address) {
    char host[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &address.sin_addr, host, sizeof(host));
    return std::string(host) + ":" + std::to_string(ntohs(address.sin_port));
}

bool same_endpoint(const sockaddr_in& left, const sockaddr_in& right) {
    return left.sin_family == right.sin_family && left.sin_port == right.sin_port &&
           left.sin_addr.s_addr == right.sin_addr.s_addr;
}

void print_usage() {
    std::cout << "udp_traffic_logger [--listen-host value] [--listen-port value] [--log-file path]\n"
              << "                   [--forward-host value --forward-port value]\n\n"
              << "Without --forward-*, this is a passive UDP listener for traffic sent to its port.\n"
              << "With --forward-*, this proxies one client endpoint to the target and logs both directions.\n";
}

Options parse_options(int argc, char** argv) {
    Options options;
    std::optional<std::string> forward_host;
    std::optional<std::uint16_t> forward_port;

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
            std::exit(0);
        }
        if (arg == "--listen-host") {
            options.listen_host = require_value(arg);
        } else if (arg == "--listen-port") {
            options.listen_port = static_cast<std::uint16_t>(std::stoul(require_value(arg)));
        } else if (arg == "--log-file") {
            options.log_file = require_value(arg);
        } else if (arg == "--forward-host") {
            forward_host = require_value(arg);
        } else if (arg == "--forward-port") {
            forward_port = static_cast<std::uint16_t>(std::stoul(require_value(arg)));
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }

    if (forward_host.has_value() != forward_port.has_value()) {
        throw std::runtime_error("--forward-host and --forward-port must be provided together");
    }
    if (forward_host && forward_port) {
        options.forward_target = Endpoint{*forward_host, *forward_port};
    }
    return options;
}

void send_udp(cpp_server::core::SocketHandle socket, const sockaddr_in& target, const std::uint8_t* data,
              std::size_t size) {
#ifdef _WIN32
    const int sent = sendto(socket, reinterpret_cast<const char*>(data), static_cast<int>(size), 0,
                            reinterpret_cast<const sockaddr*>(&target), sizeof(target));
#else
    const auto sent = sendto(socket, data, size, 0, reinterpret_cast<const sockaddr*>(&target), sizeof(target));
#endif
    if (sent < 0 || static_cast<std::size_t>(sent) != size) {
        throw std::runtime_error("sendto failed: " + cpp_server::core::LastSocketErrorText());
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parse_options(argc, argv);
        cpp_server::core::SocketRuntime socket_runtime;

        if (!options.log_file.parent_path().empty()) {
            std::filesystem::create_directories(options.log_file.parent_path());
        }
        std::ofstream log_file(options.log_file, std::ios::out | std::ios::trunc);
        if (!log_file) {
            throw std::runtime_error("failed to open log file: " + options.log_file.string());
        }

        auto listen_address = make_address(options.listen_host, options.listen_port);
        const auto target_address =
            options.forward_target ? std::optional<sockaddr_in>(
                                         make_address(options.forward_target->host, options.forward_target->port))
                                   : std::nullopt;

        const auto socket_handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (!cpp_server::core::IsValidSocket(socket_handle)) {
            throw std::runtime_error("socket failed: " + cpp_server::core::LastSocketErrorText());
        }

        if (bind(socket_handle, reinterpret_cast<const sockaddr*>(&listen_address), sizeof(listen_address)) != 0) {
            cpp_server::core::CloseSocket(socket_handle);
            throw std::runtime_error("bind failed: " + cpp_server::core::LastSocketErrorText());
        }

        sockaddr_in bound_address{};
#ifdef _WIN32
        int bound_address_len = sizeof(bound_address);
#else
        socklen_t bound_address_len = sizeof(bound_address);
#endif
        if (getsockname(socket_handle, reinterpret_cast<sockaddr*>(&bound_address), &bound_address_len) == 0) {
            listen_address = bound_address;
        }

        log_line(log_file, "[udp-log] listening on " + address_text(listen_address));
        if (target_address) {
            log_line(log_file, "[udp-log] proxy target " + address_text(*target_address));
        }

        std::array<std::uint8_t, 65536> buffer{};
        std::optional<sockaddr_in> last_client;

        while (true) {
            sockaddr_in source{};
#ifdef _WIN32
            int source_len = sizeof(source);
            const int received =
                recvfrom(socket_handle, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0,
                         reinterpret_cast<sockaddr*>(&source), &source_len);
#else
            socklen_t source_len = sizeof(source);
            const auto received =
                recvfrom(socket_handle, buffer.data(), buffer.size(), 0, reinterpret_cast<sockaddr*>(&source),
                         &source_len);
#endif
            if (received <= 0) {
                log_line(log_file, "[udp-log] recvfrom failed: " + cpp_server::core::LastSocketErrorText());
                continue;
            }

            const auto byte_count = static_cast<std::size_t>(received);
            const bool from_target = target_address && same_endpoint(source, *target_address);
            const auto direction = from_target ? "S->P" : "C->P";
            log_line(log_file, std::string("[") + direction + "] " + address_text(source) + " len=" +
                                   std::to_string(byte_count) + " hex=" +
                                   hex_dump(buffer.data(), byte_count));

            if (!target_address) {
                continue;
            }

            if (from_target) {
                if (!last_client) {
                    log_line(log_file, "[P->C] dropped server datagram; no client endpoint seen yet");
                    continue;
                }
                send_udp(socket_handle, *last_client, buffer.data(), byte_count);
                log_line(log_file, "[P->C] " + address_text(*last_client) + " len=" + std::to_string(byte_count));
            } else {
                last_client = source;
                send_udp(socket_handle, *target_address, buffer.data(), byte_count);
                log_line(log_file, "[P->S] " + address_text(*target_address) + " len=" +
                                       std::to_string(byte_count));
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "[error] " << ex.what() << '\n';
        return 1;
    }
}
