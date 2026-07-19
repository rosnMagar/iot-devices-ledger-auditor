#pragma once
#include <cstddef>
#include <optional>
#include <vector>
#include <block.hpp>


namespace ledger {

/// Outcome of a full chain integrity check.
struct VerifyResult {
    bool valid;
    std::size_t chain_length;
    std::size_t checked_blocks;
    std::optional<std::size_t> first_invalid_index;
};

/// Verify an arbitrary sequence of blocks: recompute each hash and check the
/// prev_hash linkage. Genesis (index 0) is validated against a 64-zero prev_hash.
/// Exposed as a free function so the pure verification logic is directly testable.
VerifyResult verify_chain(const std::vector<Block>& blocks);

class Chain {
public:
    Chain();

    /// Build a chain from already-persisted blocks, stored verbatim — no genesis
    /// re-seed and no hash recompute — so verify() can still detect on-disk
    /// tampering. Used by storage::load_chain on startup.
    static Chain load(std::vector<Block> blocks);

    const Block& append(EventPayload event);
    const Block& latest() const;
    std::size_t size() const;

    /// Read-only view of the underlying blocks (for range reads and reload).
    const std::vector<Block>& blocks() const;

    /// Recompute every block's hash and check the prev_hash linkage.
    /// Genesis is validated against a 64-zero prev_hash.
    VerifyResult verify() const;
private:
    struct load_tag {};
    Chain(load_tag, std::vector<Block> blocks);

    std::vector<Block> blocks_;
};

}