#pragma once
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
/// The mutex guards the chain. Readers (GET) take a shared lock and run
/// concurrently; the writer thread is the only holder of the unique lock, and
/// holds it only long enough to push one block.
///
/// There is deliberately no log handle here. The writer thread owns it for its
/// whole lifetime (IOT-25), which is what makes "exactly one thread ever writes
/// the ledger file" a property of the types rather than a rule to remember —
/// a handler cannot append to the file because it has nothing to append to.
///
/// Not copyable or movable — std::shared_mutex is neither. Construct it once in
/// main() and pass it around by reference.
struct AppState {
    Chain chain;
    std::shared_mutex mtx;

    /// The writer thread's queue. A pointer rather than a reference so AppState
    /// stays default-constructible for tests that only exercise read routes.
    /// Null means "no writer attached", and POST /events answers 500 rather
    /// than pretending to have written something.
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
