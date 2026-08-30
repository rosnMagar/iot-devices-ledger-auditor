#pragma once
#include <fstream>
#include <shared_mutex>

#include <chain.hpp>
#include <config.hpp>
#include <writer.hpp>

// Forward-declared rather than including httplib.h: that header is ~10k lines,
// and only src/server.cpp and the integration test actually need its guts.
namespace httplib {
class Server;
}

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

    /// The writer thread's queue, once one exists (IOT-24). A pointer rather
    /// than a reference so AppState can still be constructed without a queue —
    /// POST /events keeps its unique_lock until IOT-26 moves it across, and the
    /// existing tests construct AppState directly. Null means "no writer thread".
    WriteQueue* write_queue = nullptr;
};

/// Install every handler — logging, CORS, error mapping, and all four routes —
/// onto `srv`. Split out of serve() so the integration tests can drive the same
/// routes on a server they own, bind it to an ephemeral port, and stop it when
/// the test ends. serve() blocks forever, which a test cannot use.
void install_routes(httplib::Server& srv, AppState& state);

/// Build the routes and serve on config.bind_host:config.bind_port. Blocks
/// until the server stops. Returns false if the bind fails (port in use, bad
/// host) — the caller turns that into a non-zero exit.
bool serve(AppState& state, const Config& config);

}  // namespace ledger
