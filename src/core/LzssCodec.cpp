#include "cpp_server/core/LzssCodec.h"

namespace cpp_server::core {

ByteVector LzssDecoder::feed(std::span<const std::uint8_t> data) {
    pending_.insert(pending_.end(), data.begin(), data.end());
    ByteVector output;
    std::size_t pos = 0;

    while (true) {
        if (bit_ >= 8) {
            if (pos >= pending_.size()) {
                break;
            }
            flags_ = pending_[pos++];
            bit_ = 0;
        }

        if ((flags_ & (1U << bit_)) != 0) {
            if (pos + 2 > pending_.size()) {
                break;
            }

            const auto token = static_cast<std::uint16_t>(pending_[pos]) |
                               (static_cast<std::uint16_t>(pending_[pos + 1]) << 8);
            pos += 2;
            ++bit_;

            if (token == 0) {
                bit_ = 8;
                continue;
            }

            const auto distance = static_cast<std::size_t>(token & 0x0FFFU) + 1U;
            const auto length = static_cast<std::size_t>(token >> 12U) + kMinMatch;
            auto src = static_cast<std::ptrdiff_t>(window_pos_) - static_cast<std::ptrdiff_t>(distance);
            for (std::size_t count = 0; count < length; ++count) {
                const auto value = window_[static_cast<std::size_t>(src) & 0x0FFFU];
                output.push_back(value);
                window_[window_pos_ & 0x0FFFU] = value;
                ++window_pos_;
                ++src;
            }
        } else {
            if (pos >= pending_.size()) {
                break;
            }
            const auto value = pending_[pos++];
            ++bit_;
            output.push_back(value);
            window_[window_pos_ & 0x0FFFU] = value;
            ++window_pos_;
        }
    }

    if (pos != 0) {
        pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(pos));
    }

    return output;
}

std::pair<std::size_t, std::size_t> LzssEncoder::find_best_match(std::span<const std::uint8_t> data, std::size_t pos) const {
    std::size_t best_length = 0;
    std::size_t best_distance = 0;
    const auto max_length = std::min(kMaxMatch, data.size() - pos);
    if (max_length < kMinMatch) {
        return {0, 0};
    }

    const auto max_distance = std::min(window_pos_, kWindowSize);
    for (std::size_t distance = 1; distance <= max_distance; ++distance) {
        std::size_t match_length = 0;
        auto src = window_pos_ - distance;
        while (match_length < max_length) {
            if (window_[(src + match_length) & 0x0FFFU] != data[pos + match_length]) {
                break;
            }
            ++match_length;
        }

        if (match_length >= kMinMatch && match_length > best_length) {
            best_length = match_length;
            best_distance = distance;
            if (best_length == kMaxMatch) {
                break;
            }
        }
    }

    return {best_distance, best_length};
}

ByteVector LzssEncoder::encode(std::span<const std::uint8_t> data) {
    ByteVector output;
    std::size_t pos = 0;

    while (pos < data.size()) {
        if (pos + 8 <= data.size()) {
            output.push_back(0x00);
            for (std::size_t index = 0; index < 8; ++index) {
                const auto value = data[pos + index];
                output.push_back(value);
                window_[window_pos_ & 0x0FFFU] = value;
                ++window_pos_;
            }
            pos += 8;
            continue;
        }

        const auto flags_index = output.size();
        output.push_back(0);
        std::uint8_t flags = 0;

        for (std::uint8_t bit = 0; bit < 8; ++bit) {
            if (pos >= data.size()) {
                flags |= static_cast<std::uint8_t>(1U << bit);
                output.push_back(0);
                output.push_back(0);
                break;
            }

            const auto [distance, length] = find_best_match(data, pos);
            if (length >= kMinMatch) {
                flags |= static_cast<std::uint8_t>(1U << bit);
                const auto token = static_cast<std::uint16_t>(((length - kMinMatch) << 12U) | (distance - 1U));
                output.push_back(static_cast<std::uint8_t>(token & 0xFF));
                output.push_back(static_cast<std::uint8_t>((token >> 8U) & 0xFF));
                for (std::size_t match_index = 0; match_index < length; ++match_index) {
                    const auto value = data[pos + match_index];
                    window_[window_pos_ & 0x0FFFU] = value;
                    ++window_pos_;
                }
                pos += length;
            } else {
                const auto value = data[pos++];
                output.push_back(value);
                window_[window_pos_ & 0x0FFFU] = value;
                ++window_pos_;
            }
        }

        output[flags_index] = flags;
    }

    return output;
}

}  // namespace cpp_server::core
