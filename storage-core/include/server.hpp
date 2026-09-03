#pragma once
#include <mutex>
#include <shared_mutex>

#include <broadcast.hpp>
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

    /// Where the writer fans newly appended blocks out to the live feed
    /// (IOT-27). Optional in the same way and for the same reason: null simply
    /// means nothing is listening, and publishing is skipped.
    Broadcaster* broadcaster = nullptr;
};

/// Install every handler — logging, CORS, error mapping, and all four routes —
/// onto `srv`. Split out of serve() so the integration tests can drive the same
/// routes on a server they own, bind it to an ephemeral port, and stop it when
/// the test ends. serve() blocks forever, which a test cannot use.
void install_routes(httplib::Server& srv, AppState& state);

/// A stop button for a running serve(), usable from another thread.
///
/// serve() owns its httplib::Server as a local, so nothing outside it can call
/// stop(). This hands out that ability without putting the ~10k-line httplib
/// header into every translation unit that needs to shut the server down.
///
/// Safe to call stop() at any point in the server's life, including *before*
/// serve() has started: a stop that arrives early is remembered and applied the
/// moment the server attaches. Without that, a signal racing startup would be
/// dropped and the process would run on forever.
class ServerHandle {
public:
    /// Ask the server to stop accepting and return from serve(). Idempotent,
    /// and safe to call from any thread.
    void stop();

private:
    friend bool serve(AppState&, const Config&, ServerHandle*);

    /// Returns true if stop() already happened, meaning the caller should not
    /// bother listening at all.
    bool attach(httplib::Server* srv);
    void detach();

    std::mutex mtx_;
    httplib::Server* srv_ = nullptr;
    bool stopped_ = false;
};

/// Build the routes and serve on config.bind_host:config.bind_port. Blocks
/// until the server stops. Returns false if the bind fails (port in use, bad
/// host) — the caller turns that into a non-zero exit.
///
/// Pass a ServerHandle to be able to stop it from another thread; without one
/// the server runs until the process dies.
bool serve(AppState& state, const Config& config,
           ServerHandle* handle = nullptr);

}  // namespace ledger
