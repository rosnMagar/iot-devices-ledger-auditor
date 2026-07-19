#pragma once
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include <block.hpp>

namespace ledger {

/// Thrown when the ledger log cannot be opened or written.
struct StorageError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

/// Open (creating the parent directory + file if missing) the ledger log for
/// appending. Throws StorageError on failure.
std::ofstream open_append(const std::filesystem::path& path);

/// Append one block as an NDJSON line (JSON object + '\n') and flush for
/// durability. Throws StorageError if the write fails.
void append_block(std::ofstream& os, const Block& block);

}  // namespace ledger
