#pragma once

#include <cstdint>
#include <vector>

#include "cpp_server/core/ByteBuffer.h"

namespace cpp_server::core {

class LzssDecoder {
public:
    [[nodiscard]] ByteVector feed(std::span<const std::uint8_t> data);

private:
    static constexpr std::size_t kWindowSize = 0x1000;
    static constexpr std::size_t kMinMatch = 4;

    ByteVector window_{ByteVector(kWindowSize, 0)};
    std::size_t window_pos_{};
    ByteVector pending_{};
    std::uint8_t flags_{};
    std::uint8_t bit_{8};
};

class LzssEncoder {
public:
    [[nodiscard]] ByteVector encode(std::span<const std::uint8_t> data);

private:
    static constexpr std::size_t kWindowSize = 0x1000;
    static constexpr std::size_t kMaxMatch = 0x12;
    static constexpr std::size_t kMinMatch = 4;

    [[nodiscard]] std::pair<std::size_t, std::size_t> find_best_match(std::span<const std::uint8_t> data, std::size_t pos) const;

    ByteVector window_{ByteVector(kWindowSize, 0)};
    std::size_t window_pos_{};
};

}  // namespace cpp_server::core
