#include "runtime/logging.h"

#include <chrono>
#include <iostream>
#include <mutex>

namespace chronos {

namespace {

std::mutex g_log_mutex;

} // namespace

Logger& Logger::Instance() noexcept {
    static Logger instance;
    return instance;
}

void Logger::SetLevel(LogLevel level) noexcept {
    current_level_ = level;
}

LogLevel Logger::level() const noexcept {
    return current_level_;
}

void Logger::Log(LogLevel level, std::string_view tag, std::string_view message) {
    if (level < current_level_) {
        return;
    }

    const auto now = std::chrono::system_clock::now();
    const auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now{};
#if defined(_WIN32)
    localtime_s(&tm_now, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_now);
#endif

    std::lock_guard<std::mutex> lock(g_log_mutex);
    std::ostream& out = (level >= LogLevel::Warn) ? std::cerr : std::cout;

    char time_buffer[16];
    std::strftime(time_buffer, sizeof(time_buffer), "%H:%M:%S", &tm_now);

    out << '[' << time_buffer << "] ["
        << LogLevelToString(level) << "] ["
        << tag << "] "
        << message << '\n';
}

} // namespace chronos
