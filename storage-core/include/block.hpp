#pragma once

#include <cstdint>
#include <string>
#include <iomanip>
#include <sstream>
#include <ctime>

#include <nlohmann/json.hpp>
#include <openssl/evp.h>

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

inline std::string compute_hash(std::uint64_t index, const std::string& timestamp, const EventPayload& event, const std::string& prev_hash){

    std::ostringstream oss;
    unsigned char b[8];

    // Convert the index to a byte array in little-endian order
    // required for consistent hashing across different systems
    for (unsigned int i = 0; i < sizeof(b); i++) b[i] = static_cast<unsigned char>(index >> (i * 8) & 0xFF);
    oss.write(reinterpret_cast<const char*>(b), sizeof(b));
    oss << timestamp << nlohmann::json(event).dump() << prev_hash;
    const std::string input = oss.str();

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, input.data(), input.size());

    unsigned char out[EVP_MAX_MD_SIZE];
    unsigned int out_len = 0;
    EVP_DigestFinal_ex(ctx, out, &out_len);
    EVP_MD_CTX_free(ctx);

    std::ostringstream hex;
    hex << std::hex << std::setfill('0');

    for (unsigned int i = 0; i < out_len; i++) {
        hex << std::setw(2) << static_cast<int>(out[i]);
    }

    return hex.str();
}

inline std::string now_rfc3339() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

inline Block make_block(std::uint64_t index, EventPayload event, std::string prev_hash) {
    std::string ts = now_rfc3339();
    std::string h = compute_hash(index, ts, event, prev_hash);
    return Block{index, ts, std::move(event), std::move(prev_hash), std::move(h)};
}

inline Block genesis(){
    EventPayload event{"GENESIS", "system", "system", "Genesis block", nlohmann::json::object()};
    std::string prev_hash = std::string(64, '0');
    return make_block(0, event, prev_hash);
} 

}