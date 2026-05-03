#include "cpp_server/core/Logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace cpp_server::core {

namespace {

std::string TimestampText() {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const auto whole = clock::to_time_t(now);
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm local_tm{};
#ifdef _WIN32
    localtime_s(&local_tm, &whole);
#else
    localtime_r(&whole, &local_tm);
#endif

    std::ostringstream stream;
    stream << std::put_time(&local_tm, "%H:%M:%S") << '.' << std::setw(3) << std::setfill('0')
           << millis.count();
    return stream.str();
}

}  // namespace

void Logger::configure(const std::optional<std::filesystem::path>& debug_log_path) {
    std::scoped_lock lock(mutex_);
    if (debug_log_handle_.is_open()) {
        debug_log_handle_.close();
    }

    debug_log_path_ = debug_log_path;
    if (!debug_log_path_) {
        return;
    }

    if (!debug_log_path_->parent_path().empty()) {
        std::filesystem::create_directories(debug_log_path_->parent_path());
    }
    debug_log_handle_.open(*debug_log_path_, std::ios::out | std::ios::trunc);
    if (!debug_log_handle_) {
        throw std::runtime_error("failed to open debug log file: " + debug_log_path_->string());
    }
}

void Logger::close() {
    std::scoped_lock lock(mutex_);
    if (debug_log_handle_.is_open()) {
        debug_log_handle_.close();
    }
}

void Logger::log(std::string_view message) {
    emit(message, true);
}

void Logger::debug(std::string_view message) {
    emit(message, false);
}

std::string Logger::debug_log_path_text() const {
    std::scoped_lock lock(mutex_);
    if (!debug_log_path_) {
        return "(disabled)";
    }
    return debug_log_path_->string();
}

void Logger::emit(std::string_view message, bool console) {
    const auto line = '[' + TimestampText() + "] " + std::string(message);
    std::scoped_lock lock(mutex_);
    if (console) {
        std::cout << line << '\n';
    }
    if (debug_log_handle_.is_open()) {
        debug_log_handle_ << line << '\n';
        debug_log_handle_.flush();
    }
}

}  // namespace cpp_server::core
