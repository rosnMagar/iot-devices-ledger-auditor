#include "storage.hpp"

#include <system_error>

#include <nlohmann/json.hpp>

namespace ledger {

std::ofstream open_append(const std::filesystem::path& path) {
    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            throw StorageError("failed to create directory " +
                               path.parent_path().string() + ": " + ec.message());
        }
    }

    std::ofstream os(path, std::ios::app);
    if (!os) {
        throw StorageError("failed to open ledger log for append: " + path.string());
    }
    return os;
}

void append_block(std::ofstream& os, const Block& block) {
    os << nlohmann::json(block).dump() << '\n';
    os.flush();
    if (!os) {
        throw StorageError("failed to append block to ledger log");
    }
}

Chain load_chain(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        // Missing file → fresh chain seeded with genesis.
        return Chain();
    }

    std::vector<Block> blocks;
    std::string line;
    std::size_t lineno = 0;
    while (std::getline(in, line)) {
        ++lineno;
        try {
            blocks.push_back(nlohmann::json::parse(line).get<Block>());
        } catch (const std::exception& e) {
            throw StorageError("malformed ledger line " + std::to_string(lineno) +
                               ": " + e.what());
        }
    }

    if (blocks.empty()) {
        // Empty file → fresh chain seeded with genesis.
        return Chain();
    }
    return Chain::load(std::move(blocks));
}

}  // namespace ledger
