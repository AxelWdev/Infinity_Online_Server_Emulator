#pragma once

#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace cpp_server::core {

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif

class SocketRuntime {
public:
    SocketRuntime() {
#ifdef _WIN32
        WSADATA data{};
        const int result = WSAStartup(MAKEWORD(2, 2), &data);
        if (result != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
#endif
    }

    ~SocketRuntime() {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

[[nodiscard]] inline bool IsValidSocket(SocketHandle socket) {
    return socket != kInvalidSocket;
}

inline void CloseSocket(SocketHandle socket) {
    if (!IsValidSocket(socket)) {
        return;
    }
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

inline void ShutdownSocket(SocketHandle socket) {
    if (!IsValidSocket(socket)) {
        return;
    }
#ifdef _WIN32
    shutdown(socket, SD_BOTH);
#else
    shutdown(socket, SHUT_RDWR);
#endif
}

[[nodiscard]] inline std::string LastSocketErrorText() {
#ifdef _WIN32
    return "socket error " + std::to_string(WSAGetLastError());
#else
    return std::strerror(errno);
#endif
}

inline void SendAll(SocketHandle socket, std::span<const std::uint8_t> bytes) {
    std::size_t sent_total = 0;
    while (sent_total < bytes.size()) {
#ifdef _WIN32
        const int sent = send(socket, reinterpret_cast<const char*>(bytes.data() + sent_total),
                              static_cast<int>(bytes.size() - sent_total), 0);
#else
        const auto sent = send(socket, bytes.data() + sent_total, bytes.size() - sent_total, 0);
#endif
        if (sent <= 0) {
            throw std::runtime_error("send failed: " + LastSocketErrorText());
        }
        sent_total += static_cast<std::size_t>(sent);
    }
}

}  // namespace cpp_server::core
