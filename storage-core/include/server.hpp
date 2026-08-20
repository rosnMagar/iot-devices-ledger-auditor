#pragma once
#include <fstream>
#include <shared_mutex>

#include <chain.hpp>
#include <config.hpp>

namespace ledger {

/// State shared by every request handler.
///
/// One mutex guards both the chain and the log handle, because appending an
/// event touches both and they must not drift apart. Reads (GET) take a shared
/// lock so they can run concurrently; a write (POST) takes a unique lock.
///
/// Not copyable or movable — std::shared_mutex is neither. Construct it once in
/// main() and pass it around by reference.
struct AppState {
    Chain chain;
    std::shared_mutex mtx;
    std::ofstream log;  // append handle for the ledger file
};

/// Build the routes and serve on config.bind_host:config.bind_port. Blocks
/// until the server stops. Returns false if the bind fails (port in use, bad
/// host) — the caller turns that into a non-zero exit.
bool serve(AppState& state, const Config& config);

}  // namespace ledger
