#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

#include "broadcast.hpp"
#include "chain.hpp"
#include "config.hpp"
#include "log.hpp"
#include "server.hpp"
#include "signals.hpp"
#include "storage.hpp"
#include "ws.hpp"
#include "writer.hpp"

namespace {

int run() {
    // Before anything else spawns a thread: SIGTERM/SIGINT are blocked
    // process-wide here and handled by one dedicated thread. Threads inherit
    // the mask from their creator, so this has to come first — a signal
    // delivered to a thread that has not blocked it kills the process outright.
    ledger::ServerHandle server_handle;
    ledger::SignalWaiter signals([&server_handle] { server_handle.stop(); });

    // Resolve config from the environment first; a bad BIND_ADDR fails fast here.
    const ledger::Config config = ledger::load_config();
    ledger::log::init(config.log_level);

    ledger::log::info("storage-core starting");
    ledger::log::info("config: bind=" + config.bind_host + ":" +
                      std::to_string(config.bind_port) +
                      " ledger_path=" + config.ledger_path);

    // A fresh boot = no ledger file yet, or an empty one. Decide from the file
    // *before* loading, so we never duplicate an already-persisted genesis.
    std::error_code ec;
    const auto size = std::filesystem::file_size(config.ledger_path, ec);
    const bool fresh = static_cast<bool>(ec) || size == 0;

    // Reload existing ledger state (or seed genesis on a fresh boot).
    ledger::AppState state;
    state.chain = ledger::load_chain(config.ledger_path);

    // The append handle is a local, not part of AppState: it is about to be
    // moved into the writer thread, which owns it from then on.
    std::ofstream log = ledger::open_append(config.ledger_path);

    if (fresh) {
        // Persist the genesis that load_chain seeded in-memory, so the first
        // read after boot is consistent with disk. Safe to do on this thread —
        // the writer has not started yet, so there is still only one writer.
        ledger::append_block(log, state.chain.latest());
    }

    ledger::log::info("loaded chain length " + std::to_string(state.chain.size()) +
                      " from " + config.ledger_path);

    // One writer thread for the process lifetime. Every POST /events goes
    // through this queue; nothing else may touch the ledger file.
    ledger::WriteQueue queue;
    state.write_queue = &queue;

    // Declared before the writer thread so it outlives it: the writer publishes
    // to this on every append, and must not be left holding a dangling pointer
    // while it drains during shutdown.
    ledger::Broadcaster broadcaster;
    state.broadcaster = &broadcaster;

    std::thread writer(ledger::writer_loop, std::ref(queue), std::ref(state),
                       std::move(log));

    // The live feed, on its own port (ADR 0008). A bind failure here is fatal:
    // starting with the REST API up and the feed silently missing is exactly the
    // kind of half-working deploy that IOT-49/51/52 were about.
    ledger::WsServer ws(broadcaster, config.ws_host, config.ws_port);
    if (!ws.start()) {
        ledger::log::error("failed to bind websocket listener on " +
                           config.ws_host + ":" + std::to_string(config.ws_port));
        queue.close();
        writer.join();
        return 1;
    }
    ledger::log::info("websocket listening on " + config.ws_host + ":" +
                      std::to_string(config.ws_port) + "/blocks");

    const bool ok = ledger::serve(state, config, &server_handle);

    // Reached on SIGTERM/SIGINT, or if listen() fails.
    //
    // Order matters and is the whole point of this sequence:
    //   1. stop accepting  — no new request can be queued from here on
    //   2. close the queue — refuses new work, keeps what was already accepted
    //   3. join the writer — drains that backlog to disk before we exit
    //   4. release readers — nothing is left blocked in next()
    //
    // Closing the queue before the listeners stop would make in-flight handlers
    // return 500 for requests that could have been served.
    ledger::log::info("shutting down: stopping the websocket listener");
    ws.stop();

    ledger::log::info("shutting down: closing the write queue");
    queue.close();
    writer.join();
    ledger::log::info("writer thread joined");

    // After the writer is joined, so nothing can publish to a subscription that
    // has already been closed.
    broadcaster.close_all();

    return ok ? 0 : 1;
}

}  // namespace

int main() {
    // Logging may not be initialized yet if config parsing itself fails, so the
    // last-resort error goes straight to stderr.
    try {
        return run();
    } catch (const std::exception& e) {
        std::cerr << "FATAL storage-core startup failed: " << e.what() << '\n';
        return 1;
    }
}
