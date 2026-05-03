#include "cpp_server/core/Json.h"

#include <cctype>
#include <fstream>
#include <sstream>

namespace cpp_server::core {

bool JsonValue::as_boolean() const {
    if (!is_boolean()) {
        throw std::runtime_error("JSON value is not a boolean");
    }
    return std::get<bool>(storage_);
}

std::int64_t JsonValue::as_integer() const {
    if (!is_integer()) {
        throw std::runtime_error("JSON value is not an integer");
    }
    return std::get<std::int64_t>(storage_);
}

const std::string& JsonValue::as_string() const {
    if (!is_string()) {
        throw std::runtime_error("JSON value is not a string");
    }
    return std::get<std::string>(storage_);
}

const JsonValue::Array& JsonValue::as_array() const {
    if (!is_array()) {
        throw std::runtime_error("JSON value is not an array");
    }
    return std::get<Array>(storage_);
}

const JsonValue::Object& JsonValue::as_object() const {
    if (!is_object()) {
        throw std::runtime_error("JSON value is not an object");
    }
    return std::get<Object>(storage_);
}

JsonValue::Array& JsonValue::as_array() {
    if (!is_array()) {
        throw std::runtime_error("JSON value is not an array");
    }
    return std::get<Array>(storage_);
}

JsonValue::Object& JsonValue::as_object() {
    if (!is_object()) {
        throw std::runtime_error("JSON value is not an object");
    }
    return std::get<Object>(storage_);
}

const JsonValue* JsonValue::find(std::string_view key) const {
    if (!is_object()) {
        return nullptr;
    }
    const auto& object = std::get<Object>(storage_);
    const auto it = object.find(key);
    if (it == object.end()) {
        return nullptr;
    }
    return &it->second;
}

namespace {

class JsonParser {
public:
    explicit JsonParser(std::string_view text) : text_(text) {}

    JsonValue parse() {
        skip_whitespace();
        auto value = parse_value();
        skip_whitespace();
        if (pos_ != text_.size()) {
            throw std::runtime_error("unexpected trailing data after JSON value");
        }
        return value;
    }

private:
    JsonValue parse_value() {
        skip_whitespace();
        if (pos_ >= text_.size()) {
            throw std::runtime_error("unexpected end of JSON input");
        }

        switch (text_[pos_]) {
        case '{':
            return parse_object();
        case '[':
            return parse_array();
        case '"':
            return JsonValue(parse_string());
        case 't':
            expect_keyword("true");
            return JsonValue(true);
        case 'f':
            expect_keyword("false");
            return JsonValue(false);
        case 'n':
            expect_keyword("null");
            return JsonValue(nullptr);
        default:
            if (text_[pos_] == '-' || std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                return JsonValue(parse_integer());
            }
            break;
        }

        throw std::runtime_error("unexpected character in JSON input");
    }

    JsonValue parse_object() {
        expect('{');
        JsonValue::Object object;
        skip_whitespace();
        if (peek('}')) {
            expect('}');
            return JsonValue(std::move(object));
        }

        while (true) {
            skip_whitespace();
            const auto key = parse_string();
            skip_whitespace();
            expect(':');
            skip_whitespace();
            object.emplace(std::move(key), parse_value());
            skip_whitespace();
            if (peek('}')) {
                expect('}');
                break;
            }
            expect(',');
        }

        return JsonValue(std::move(object));
    }

    JsonValue parse_array() {
        expect('[');
        JsonValue::Array array;
        skip_whitespace();
        if (peek(']')) {
            expect(']');
            return JsonValue(std::move(array));
        }

        while (true) {
            skip_whitespace();
            array.push_back(parse_value());
            skip_whitespace();
            if (peek(']')) {
                expect(']');
                break;
            }
            expect(',');
        }

        return JsonValue(std::move(array));
    }

    std::string parse_string() {
        expect('"');
        std::string result;
        while (pos_ < text_.size()) {
            const char ch = text_[pos_++];
            if (ch == '"') {
                return result;
            }
            if (ch == '\\') {
                if (pos_ >= text_.size()) {
                    throw std::runtime_error("unterminated escape sequence in JSON string");
                }
                const char escaped = text_[pos_++];
                switch (escaped) {
                case '"':
                case '\\':
                case '/':
                    result.push_back(escaped);
                    break;
                case 'b':
                    result.push_back('\b');
                    break;
                case 'f':
                    result.push_back('\f');
                    break;
                case 'n':
                    result.push_back('\n');
                    break;
                case 'r':
                    result.push_back('\r');
                    break;
                case 't':
                    result.push_back('\t');
                    break;
                case 'u':
                    result += parse_unicode_escape();
                    break;
                default:
                    throw std::runtime_error("unsupported escape sequence in JSON string");
                }
                continue;
            }
            result.push_back(ch);
        }

        throw std::runtime_error("unterminated JSON string literal");
    }

    std::string parse_unicode_escape() {
        if (pos_ + 4 > text_.size()) {
            throw std::runtime_error("incomplete unicode escape in JSON string");
        }

        std::uint32_t codepoint = 0;
        for (int index = 0; index < 4; ++index) {
            const char ch = text_[pos_++];
            codepoint <<= 4U;
            if (ch >= '0' && ch <= '9') {
                codepoint |= static_cast<std::uint32_t>(ch - '0');
            } else if (ch >= 'a' && ch <= 'f') {
                codepoint |= static_cast<std::uint32_t>(10 + ch - 'a');
            } else if (ch >= 'A' && ch <= 'F') {
                codepoint |= static_cast<std::uint32_t>(10 + ch - 'A');
            } else {
                throw std::runtime_error("invalid unicode escape in JSON string");
            }
        }

        std::string utf8;
        if (codepoint <= 0x7F) {
            utf8.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7FF) {
            utf8.push_back(static_cast<char>(0xC0U | (codepoint >> 6U)));
            utf8.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
        } else {
            utf8.push_back(static_cast<char>(0xE0U | (codepoint >> 12U)));
            utf8.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
            utf8.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
        }
        return utf8;
    }

    std::int64_t parse_integer() {
        const auto start = pos_;
        if (text_[pos_] == '-') {
            ++pos_;
        }
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }

        const auto token = text_.substr(start, pos_ - start);
        std::int64_t value = 0;
        const auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), value);
        if (ec != std::errc{} || ptr != token.data() + token.size()) {
            throw std::runtime_error("invalid integer in JSON input");
        }
        return value;
    }

    void skip_whitespace() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
    }

    void expect(char ch) {
        if (pos_ >= text_.size() || text_[pos_] != ch) {
            throw std::runtime_error("unexpected token in JSON input");
        }
        ++pos_;
    }

    void expect_keyword(std::string_view keyword) {
        if (text_.substr(pos_, keyword.size()) != keyword) {
            throw std::runtime_error("unexpected keyword in JSON input");
        }
        pos_ += keyword.size();
    }

    [[nodiscard]] bool peek(char ch) const {
        return pos_ < text_.size() && text_[pos_] == ch;
    }

    std::string_view text_{};
    std::size_t pos_{};
};

}  // namespace

JsonValue ParseJson(std::string_view text) {
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.remove_prefix(3);
    }
    return JsonParser(text).parse();
}

JsonValue LoadJsonFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open JSON file: " + path.string());
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return ParseJson(buffer.str());
}

}  // namespace cpp_server::core
