#include <iostream>
#include <string>

#include "block.hpp"

int main() {
    ledger::Block genesis{
        0,
        "1970-01-01T00:00:00Z",
        ledger::EventPayload{"GENESIS", "system", "system", "Genesis block",
                             nlohmann::json::object()},
        std::string(64, '0'),
        "",
    };

    nlohmann::json j = genesis;
    std::cout << j.dump(2) << "\n";
    return 0;
}
