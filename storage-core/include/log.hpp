#pragma once
#include <cctype>
#include <chrono>
#include <ctime>
#include <iostream>
#include <string>
#include <string_view>

namespace ledger::log {

/// Severity, most severe first. A message prints when its level is at or below
/// the configured threshold (INFO shows ERROR/WARN/INFO, hides DEBUG).
enum class Level { Error = 0, Warn = 1, Info = 2, Debug = 3 };

namespace detail {

/// Process-wide threshold. Defaults to INFO until init() overrides it.
inline Level& threshold() {
    static Level t = Level::Info;
    return t;
}

inline std::string_view name(Level l) {
    switch (l) {
        case Level::Error: return "ERROR";
        case Level::Warn:  return "WARN";
        case Level::Info:  return "INFO";
        case Level::Debug: return "DEBUG";
    }
    return "INFO";
}

/// ISO-8601 UTC timestamp, e.g. 2026-07-23T14:05:09Z.
inline std::string now_utc() {
    const auto t = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

}  // namespace detail

/// Parse a LOG_LEVEL string (case-insensitive) to a Level; unknown → INFO.
inline Level level_from(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (s == "error") return Level::Error;
    if (s == "warn" || s == "warning") return Level::Warn;
    if (s == "debug" || s == "trace") return Level::Debug;
    return Level::Info;
}

/// Set the process-wide log threshold. Call once at startup, before logging.
inline void init(Level threshold) { detail::threshold() = threshold; }

/// Emit one `TIMESTAMP LEVEL message` line to stderr if level passes the
/// threshold. stderr keeps logs off the HTTP response stream.
inline void log(Level level, std::string_view message) {
    if (static_cast<int>(level) > static_cast<int>(detail::threshold())) return;
    std::cerr << detail::now_utc() << ' ' << detail::name(level) << ' '
              << message << '\n';
}

inline void error(std::string_view m) { log(Level::Error, m); }
inline void warn(std::string_view m)  { log(Level::Warn, m); }
inline void info(std::string_view m)  { log(Level::Info, m); }
inline void debug(std::string_view m) { log(Level::Debug, m); }

}  // namespace ledger::log
