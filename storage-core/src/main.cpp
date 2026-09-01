#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

#include "chain.hpp"
#include "config.hpp"
#include "log.hpp"
#include "server.hpp"
#include "storage.hpp"
#include "writer.hpp"

namespace {

int run() {
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
    std::thread writer(ledger::writer_loop, std::ref(queue), std::ref(state),
                       std::move(log));

    const bool ok = ledger::serve(state, config);

    // Shut down in order: stop accepting work, let the writer drain what was
    // already accepted, then join. Closing first is what makes pop() return
    // nullopt and the loop exit — joining without it would hang forever.
    ledger::log::info("shutting down: closing the write queue");
    queue.close();
    writer.join();
    ledger::log::info("writer thread joined");

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
