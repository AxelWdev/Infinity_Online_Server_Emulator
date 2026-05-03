#include "cpp_server/core/Utf.h"

#include <stdexcept>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <codecvt>
#include <locale>
#endif

namespace cpp_server::core {

std::u16string Utf8ToUtf16(std::string_view text) {
#ifdef _WIN32
    if (text.empty()) {
        return {};
    }

    const int wide_length = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0);
    if (wide_length <= 0) {
        throw std::runtime_error("failed to convert UTF-8 text to UTF-16");
    }

    std::wstring wide(static_cast<std::size_t>(wide_length), L'\0');
    if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            static_cast<int>(text.size()),
            wide.data(),
            wide_length) <= 0) {
        throw std::runtime_error("failed to convert UTF-8 text to UTF-16");
    }

    return std::u16string(wide.begin(), wide.end());
#else
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
    return converter.from_bytes(text.data(), text.data() + text.size());
#endif
}

std::string Utf16ToUtf8(std::u16string_view text) {
#ifdef _WIN32
    if (text.empty()) {
        return {};
    }

    std::wstring wide(text.begin(), text.end());
    const int utf8_length = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        wide.data(),
        static_cast<int>(wide.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (utf8_length <= 0) {
        throw std::runtime_error("failed to convert UTF-16 text to UTF-8");
    }

    std::string utf8(static_cast<std::size_t>(utf8_length), '\0');
    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            wide.data(),
            static_cast<int>(wide.size()),
            utf8.data(),
            utf8_length,
            nullptr,
            nullptr) <= 0) {
        throw std::runtime_error("failed to convert UTF-16 text to UTF-8");
    }

    return utf8;
#else
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
    return converter.to_bytes(text.data(), text.data() + text.size());
#endif
}

ByteVector EncodeUtf16Le(std::string_view text, bool append_terminator) {
    const auto utf16 = Utf8ToUtf16(text);
    ByteVector bytes;
    bytes.reserve(utf16.size() * 2 + (append_terminator ? 2 : 0));
    for (const char16_t ch : utf16) {
        bytes.push_back(static_cast<std::uint8_t>(ch & 0xFF));
        bytes.push_back(static_cast<std::uint8_t>((ch >> 8) & 0xFF));
    }
    if (append_terminator) {
        bytes.push_back(0);
        bytes.push_back(0);
    }
    return bytes;
}

std::string DecodeUtf16Le(std::span<const std::uint8_t> bytes) {
    if (bytes.size() % 2 != 0) {
        throw std::runtime_error("UTF-16LE byte sequence must contain an even number of bytes");
    }

    std::u16string utf16;
    utf16.reserve(bytes.size() / 2);
    for (std::size_t index = 0; index < bytes.size(); index += 2) {
        const auto value = static_cast<char16_t>(bytes[index]) |
                           (static_cast<char16_t>(bytes[index + 1]) << 8);
        utf16.push_back(value);
    }
    return Utf16ToUtf8(utf16);
}

}  // namespace cpp_server::core
