#pragma once

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

namespace ledger {

/// Domain content of an audit event — what POST /events accepts.
struct EventPayload {
    std::string event_type;
    std::string location_id;
    std::string actor;
    std::string description;
    nlohmann::json metadata;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(EventPayload, event_type, location_id, actor,
                                   description, metadata)

/// One entry in the hash-chained ledger.
struct Block {
    std::uint64_t index;
    std::string timestamp;  // RFC3339
    EventPayload event;
    std::string prev_hash;
    std::string hash;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Block, index, timestamp, event, prev_hash, hash)

}  // namespace ledger
