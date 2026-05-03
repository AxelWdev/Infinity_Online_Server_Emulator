#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "cpp_server/core/ByteBuffer.h"

namespace cpp_server::core {

[[nodiscard]] std::u16string Utf8ToUtf16(std::string_view text);
[[nodiscard]] std::string Utf16ToUtf8(std::u16string_view text);
[[nodiscard]] ByteVector EncodeUtf16Le(std::string_view text, bool append_terminator = false);
[[nodiscard]] std::string DecodeUtf16Le(std::span<const std::uint8_t> bytes);

}  // namespace cpp_server::core
