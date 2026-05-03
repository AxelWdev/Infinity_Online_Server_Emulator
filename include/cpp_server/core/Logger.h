#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace cpp_server::core {

class Logger {
public:
    void configure(const std::optional<std::filesystem::path>& debug_log_path);
    void close();

    void log(std::string_view message);
    void debug(std::string_view message);

    [[nodiscard]] std::string debug_log_path_text() const;

private:
    void emit(std::string_view message, bool console);

    mutable std::mutex mutex_{};
    std::optional<std::filesystem::path> debug_log_path_{};
    std::ofstream debug_log_handle_{};
};

}  // namespace cpp_server::core
