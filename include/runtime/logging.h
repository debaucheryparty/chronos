#pragma once

#include <format>
#include <string_view>

namespace chronos {

enum class LogLevel {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
};

constexpr std::string_view LogLevelToString(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Trace:
        return "TRACE";
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warn:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    case LogLevel::Fatal:
        return "FATAL";
    }
    return "UNKNOWN";
}

class Logger {
public:
    static Logger& Instance() noexcept;

    void SetLevel(LogLevel level) noexcept;
    [[nodiscard]] LogLevel level() const noexcept;

    void Log(LogLevel level, std::string_view tag, std::string_view message);

    template <typename... Args>
    void LogFormat(LogLevel level, std::string_view tag, std::format_string<Args...> fmt, Args&&... args) {
        if (level < current_level_) {
            return;
        }
        std::string formatted = std::format(fmt, std::forward<Args>(args)...);
        Log(level, tag, formatted);
    }

private:
    Logger() noexcept = default;
    LogLevel current_level_ = LogLevel::Info;
};

template <typename... Args>
inline void LogDebug(std::string_view tag, std::format_string<Args...> fmt, Args&&... args) {
    Logger::Instance().LogFormat(LogLevel::Debug, tag, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
inline void LogInfo(std::string_view tag, std::format_string<Args...> fmt, Args&&... args) {
    Logger::Instance().LogFormat(LogLevel::Info, tag, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
inline void LogWarn(std::string_view tag, std::format_string<Args...> fmt, Args&&... args) {
    Logger::Instance().LogFormat(LogLevel::Warn, tag, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
inline void LogError(std::string_view tag, std::format_string<Args...> fmt, Args&&... args) {
    Logger::Instance().LogFormat(LogLevel::Error, tag, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
inline void LogFatal(std::string_view tag, std::format_string<Args...> fmt, Args&&... args) {
    Logger::Instance().LogFormat(LogLevel::Fatal, tag, fmt, std::forward<Args>(args)...);
}

} // namespace chronos
