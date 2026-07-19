#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

#include "chain.hpp"
#include "storage.hpp"

namespace {

std::string env_or(const char* key, const std::string& fallback) {
    const char* v = std::getenv(key);
    return (v && *v) ? std::string(v) : fallback;
}

}  // namespace

int main() {
    // Config (formal config/logging arrives in IOT-19; minimal here).
    const std::string ledger_path = env_or("LEDGER_PATH", "./data/ledger.log");

    // A fresh boot = no ledger file yet, or an empty one. Decide from the file
    // *before* loading, so we never duplicate an already-persisted genesis.
    std::error_code ec;
    const auto size = std::filesystem::file_size(ledger_path, ec);
    const bool fresh = static_cast<bool>(ec) || size == 0;

    // Reload existing ledger state, or seed genesis on a fresh boot.
    ledger::Chain chain = ledger::load_chain(ledger_path);

    if (fresh) {
        // Persist the genesis that load_chain seeded in-memory, so the first
        // read after boot is consistent with disk.
        std::ofstream log = ledger::open_append(ledger_path);
        ledger::append_block(log, chain.latest());
    }

    std::cerr << "storage-core: loaded chain length " << chain.size()
              << " from " << ledger_path << "\n";

    // HTTP server (bind + serve) lands in IOT-20; until then, exit cleanly.
    return 0;
}
