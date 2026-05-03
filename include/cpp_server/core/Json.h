#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace cpp_server::core {

class JsonValue {
public:
    using Array = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue, std::less<>>;
    using Storage = std::variant<std::nullptr_t, bool, std::int64_t, std::string, Array, Object>;

    JsonValue() = default;
    JsonValue(std::nullptr_t value) : storage_(value) {}
    JsonValue(bool value) : storage_(value) {}
    JsonValue(std::int64_t value) : storage_(value) {}
    JsonValue(std::string value) : storage_(std::move(value)) {}
    JsonValue(Array value) : storage_(std::move(value)) {}
    JsonValue(Object value) : storage_(std::move(value)) {}

    [[nodiscard]] bool is_null() const { return std::holds_alternative<std::nullptr_t>(storage_); }
    [[nodiscard]] bool is_boolean() const { return std::holds_alternative<bool>(storage_); }
    [[nodiscard]] bool is_integer() const { return std::holds_alternative<std::int64_t>(storage_); }
    [[nodiscard]] bool is_string() const { return std::holds_alternative<std::string>(storage_); }
    [[nodiscard]] bool is_array() const { return std::holds_alternative<Array>(storage_); }
    [[nodiscard]] bool is_object() const { return std::holds_alternative<Object>(storage_); }

    [[nodiscard]] bool as_boolean() const;
    [[nodiscard]] std::int64_t as_integer() const;
    [[nodiscard]] const std::string& as_string() const;
    [[nodiscard]] const Array& as_array() const;
    [[nodiscard]] const Object& as_object() const;
    [[nodiscard]] Array& as_array();
    [[nodiscard]] Object& as_object();

    [[nodiscard]] const JsonValue* find(std::string_view key) const;

private:
    Storage storage_{nullptr};
};

[[nodiscard]] JsonValue ParseJson(std::string_view text);
[[nodiscard]] JsonValue LoadJsonFile(const std::filesystem::path& path);

}  // namespace cpp_server::core
