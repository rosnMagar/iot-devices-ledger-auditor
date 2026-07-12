#include "chain.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace ledger {

Chain::Chain() {
    blocks_.push_back(genesis());
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

}  // namespace ledger
