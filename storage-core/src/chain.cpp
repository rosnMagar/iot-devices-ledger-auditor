#include "chain.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace ledger {

Chain::Chain() {
    blocks_.push_back(genesis());
}

Chain::Chain(load_tag, std::vector<Block> blocks) : blocks_(std::move(blocks)) {}

Chain Chain::load(std::vector<Block> blocks) {
    return Chain(load_tag{}, std::move(blocks));
}

const std::vector<Block>& Chain::blocks() const {
    return blocks_;
}

const Block& Chain::append(EventPayload event) {
    // Copy what we need from the tail before push_back may reallocate blocks_.
    const std::uint64_t next_index = latest().index + 1;
    std::string prev_hash = latest().hash;

    blocks_.push_back(make_block(next_index, std::move(event), std::move(prev_hash)));
    return blocks_.back();
}

const Block& Chain::latest() const {
    return blocks_.back();
}

std::size_t Chain::size() const {
    return blocks_.size();
}

VerifyResult verify_chain(const std::vector<Block>& blocks) {
    static const std::string kGenesisPrevHash(64, '0');

    VerifyResult result{true, blocks.size(), 0, std::nullopt};

    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const Block& b = blocks[i];

        // Expected prev_hash: 64 zeros for genesis, otherwise the prior hash.
        const std::string& expected_prev = (i == 0) ? kGenesisPrevHash : blocks[i - 1].hash;

        const std::string recomputed = compute_hash(b.index, b.timestamp, b.event, b.prev_hash);

        ++result.checked_blocks;

        if (b.prev_hash != expected_prev || recomputed != b.hash) {
            result.valid = false;
            result.first_invalid_index = i;
            break;
        }
    }

    return result;
}

VerifyResult Chain::verify() const {
    return verify_chain(blocks_);
}

}  // namespace ledger
