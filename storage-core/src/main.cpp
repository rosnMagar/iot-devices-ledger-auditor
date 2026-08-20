#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

#include "chain.hpp"
#include "config.hpp"
#include "log.hpp"
#include "server.hpp"
#include "storage.hpp"

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

    // Reload existing ledger state (or seed genesis on a fresh boot) and keep the
    // append handle open for as long as the server runs.
    ledger::AppState state{ledger::load_chain(config.ledger_path),
                           {},
                           ledger::open_append(config.ledger_path)};

    if (fresh) {
        // Persist the genesis that load_chain seeded in-memory, so the first
        // read after boot is consistent with disk.
        ledger::append_block(state.log, state.chain.latest());
    }

    ledger::log::info("loaded chain length " + std::to_string(state.chain.size()) +
                      " from " + config.ledger_path);

    return ledger::serve(state, config) ? 0 : 1;
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
