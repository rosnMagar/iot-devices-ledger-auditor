#pragma once
#include <vector>
#include <block.hpp>


namespace ledger {

class Chain {
public:
    Chain();
    const Block& append(EventPayload event);
    const Block& latest() const;
    std::size_t size() const;
private:
    std::vector<Block> blocks_;
};

}