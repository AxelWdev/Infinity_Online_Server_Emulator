#pragma once

#include <array>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace cpp_server::core {

using ByteVector = std::vector<std::uint8_t>;

class ByteWriter {
public:
    void write_u8(std::uint8_t value) {
        bytes_.push_back(value);
    }

    void write_u16(std::uint16_t value) {
        bytes_.push_back(static_cast<std::uint8_t>(value & 0xFF));
        bytes_.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    }

    void write_u32(std::uint32_t value) {
        bytes_.push_back(static_cast<std::uint8_t>(value & 0xFF));
        bytes_.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
        bytes_.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
        bytes_.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
    }

    void write_bytes(std::span<const std::uint8_t> bytes) {
        bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
    }

    [[nodiscard]] const ByteVector& bytes() const {
        return bytes_;
    }

    [[nodiscard]] ByteVector take() {
        return std::move(bytes_);
    }

private:
    ByteVector bytes_{};
};

class ByteReader {
public:
    explicit ByteReader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    [[nodiscard]] std::size_t remaining() const {
        return bytes_.size() - offset_;
    }

    [[nodiscard]] std::size_t position() const {
        return offset_;
    }

    [[nodiscard]] std::uint8_t read_u8() {
        require(1);
        return bytes_[offset_++];
    }

    [[nodiscard]] std::uint16_t read_u16() {
        require(2);
        const auto value = static_cast<std::uint16_t>(bytes_[offset_]) |
                           (static_cast<std::uint16_t>(bytes_[offset_ + 1]) << 8);
        offset_ += 2;
        return value;
    }

    [[nodiscard]] std::uint32_t read_u32() {
        require(4);
        const auto value = static_cast<std::uint32_t>(bytes_[offset_]) |
                           (static_cast<std::uint32_t>(bytes_[offset_ + 1]) << 8) |
                           (static_cast<std::uint32_t>(bytes_[offset_ + 2]) << 16) |
                           (static_cast<std::uint32_t>(bytes_[offset_ + 3]) << 24);
        offset_ += 4;
        return value;
    }

    [[nodiscard]] ByteVector read_bytes(std::size_t count) {
        require(count);
        ByteVector out(bytes_.begin() + static_cast<std::ptrdiff_t>(offset_),
                       bytes_.begin() + static_cast<std::ptrdiff_t>(offset_ + count));
        offset_ += count;
        return out;
    }

    [[nodiscard]] ByteVector read_remaining() {
        return read_bytes(remaining());
    }

private:
    void require(std::size_t count) const {
        if (remaining() < count) {
            throw std::runtime_error("buffer underflow while reading packet bytes");
        }
    }

    std::span<const std::uint8_t> bytes_{};
    std::size_t offset_{};
};

[[nodiscard]] inline std::string HexBytes(std::span<const std::uint8_t> bytes) {
    static constexpr std::array<char, 16> kHexDigits = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
    };

    if (bytes.empty()) {
        return {};
    }

    std::string result;
    result.reserve(bytes.size() * 3 - 1);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        const auto value = bytes[index];
        if (index != 0) {
            result.push_back(' ');
        }
        result.push_back(kHexDigits[(value >> 4) & 0x0F]);
        result.push_back(kHexDigits[value & 0x0F]);
    }
    return result;
}

[[nodiscard]] inline int HexNibble(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    return -1;
}

[[nodiscard]] inline ByteVector ParseHexString(std::string_view text) {
    std::string compact;
    compact.reserve(text.size());
    for (const char ch : text) {
        if (!std::isspace(static_cast<unsigned char>(ch))) {
            compact.push_back(ch);
        }
    }

    if (compact.size() % 2 != 0) {
        throw std::runtime_error("hex text must contain an even number of digits");
    }

    ByteVector bytes;
    bytes.reserve(compact.size() / 2);
    for (std::size_t index = 0; index < compact.size(); index += 2) {
        const int hi = HexNibble(compact[index]);
        const int lo = HexNibble(compact[index + 1]);
        if (hi < 0 || lo < 0) {
            throw std::runtime_error("invalid hex digit in input");
        }
        bytes.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
    }
    return bytes;
}

[[nodiscard]] inline std::uint32_t ParseIpv4(std::string_view text) {
    std::array<unsigned int, 4> octets{};
    std::size_t octet_index = 0;
    std::size_t start = 0;

    while (start <= text.size()) {
        const auto end = text.find('.', start);
        const auto token = text.substr(start, end == std::string_view::npos ? text.size() - start : end - start);
        if (octet_index >= octets.size() || token.empty()) {
            throw std::runtime_error("IPv4 address must contain four dot-separated octets");
        }

        unsigned int value = 0;
        const auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), value);
        if (ec != std::errc{} || ptr != token.data() + token.size() || value > 0xFF) {
            throw std::runtime_error("IPv4 octets must be integers in 0..255");
        }

        octets[octet_index++] = value;
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }

    if (octet_index != 4) {
        throw std::runtime_error("IPv4 address must contain four dot-separated octets");
    }

    return octets[0] | (octets[1] << 8U) | (octets[2] << 16U) | (octets[3] << 24U);
}

[[nodiscard]] inline std::string FormatIpv4(std::uint32_t packed_ipv4) {
    std::ostringstream stream;
    stream << (packed_ipv4 & 0xFFU) << '.'
           << ((packed_ipv4 >> 8U) & 0xFFU) << '.'
           << ((packed_ipv4 >> 16U) & 0xFFU) << '.'
           << ((packed_ipv4 >> 24U) & 0xFFU);
    return stream.str();
}

}  // namespace cpp_server::core
