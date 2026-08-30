#pragma once
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "log.hpp"

namespace ledger {

/// Runtime configuration, read from the environment with defaults.
struct Config {
    std::string bind_host = "0.0.0.0";  // from BIND_ADDR host:port
    int bind_port = 8080;               // from BIND_ADDR host:port
    std::string ws_host = "0.0.0.0";    // from WS_BIND_ADDR host:port
    int ws_port = 8081;                 // from WS_BIND_ADDR host:port
    std::string ledger_path = "./data/ledger.log";
    log::Level log_level = log::Level::Info;
};

namespace detail {

inline std::string env_or(const char* key, const std::string& fallback) {
    const char* v = std::getenv(key);
    return (v && *v) ? std::string(v) : fallback;
}

}  // namespace detail

/// Read configuration from the environment, applying defaults:
///   BIND_ADDR    (default 0.0.0.0:8080)
///   WS_BIND_ADDR (default 0.0.0.0:8081)
///   LEDGER_PATH (default ./data/ledger.log)
///   LOG_LEVEL   (default info)
/// Throws std::invalid_argument on a malformed BIND_ADDR, so a bad value fails
/// at startup with a clear message instead of at bind time.
inline Config load_config() {
    Config c;

    const std::string addr = detail::env_or("BIND_ADDR", "0.0.0.0:8080");
    const auto colon = addr.rfind(':');
    if (colon == std::string::npos) {
        throw std::invalid_argument("BIND_ADDR must be host:port, got: " + addr);
    }
    c.bind_host = addr.substr(0, colon);
    try {
        c.bind_port = std::stoi(addr.substr(colon + 1));
    } catch (const std::exception&) {
        throw std::invalid_argument("BIND_ADDR port is not a number: " + addr);
    }

    // The WebSocket server is a *second* listener on its own port: cpp-httplib
    // and IXWebSocket each own a listening socket and cannot share 8080.
    const std::string ws_addr = detail::env_or("WS_BIND_ADDR", "0.0.0.0:8081");
    const auto ws_colon = ws_addr.rfind(':');
    if (ws_colon == std::string::npos) {
        throw std::invalid_argument("WS_BIND_ADDR must be host:port, got: " + ws_addr);
    }
    c.ws_host = ws_addr.substr(0, ws_colon);
    try {
        c.ws_port = std::stoi(ws_addr.substr(ws_colon + 1));
    } catch (const std::exception&) {
        throw std::invalid_argument("WS_BIND_ADDR port is not a number: " + ws_addr);
    }

    c.ledger_path = detail::env_or("LEDGER_PATH", "./data/ledger.log");
    c.log_level = log::level_from(detail::env_or("LOG_LEVEL", "info"));
    return c;
}

}  // namespace ledger
