#pragma once
#include <cstdint>
#include <string>

// namespace mtlog {

enum class LogLevel : uint32_t {
    Trace = 0,
    Debug,
    Info,
    Warning,
    Error,
    Fatal
};

constexpr std::string to_string(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace:   return "TRACE";
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error:   return "ERROR";
        case LogLevel::Fatal:   return "FATAL";
    }
    return "UNKNOWN";
}

constexpr bool is_enabled(LogLevel message_level, LogLevel logger_level) noexcept {
    return static_cast<uint32_t>(message_level) >= static_cast<uint32_t>(logger_level);
}

// } // namespace mtloig