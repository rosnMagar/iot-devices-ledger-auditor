#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "block.hpp"
#include "chain.hpp"

using namespace ledger;

namespace {

EventPayload sample(const std::string& actor = "alice") {
    return EventPayload{"DOOR_OPEN", "loc-1", actor, "door opened",
                        nlohmann::json::object()};
}

// A valid 3-block chain (genesis + two events) built by linking prev_hash.
std::vector<Block> make_valid_chain() {
    Block b0 = genesis();
    Block b1 = make_block(1, sample("alice"), b0.hash);
    Block b2 = make_block(2, sample("bob"), b1.hash);
    return {b0, b1, b2};
}

}  // namespace

TEST_CASE("determinism: identical inputs produce identical hashes") {
    const std::string ts = "2026-07-12T00:00:00Z";
    EventPayload e = sample();
    const std::string prev(64, '0');

    std::string h1 = compute_hash(7, ts, e, prev);
    std::string h2 = compute_hash(7, ts, e, prev);

    CHECK(h1 == h2);
    CHECK(h1.size() == 64);  // SHA-256 hex
}

TEST_CASE("tamper: mutating any block field changes its hash") {
    const std::string ts = "2026-07-12T00:00:00Z";
    const std::string prev(64, '0');
    const std::string base = compute_hash(1, ts, sample("alice"), prev);

    SUBCASE("index") {
        CHECK(compute_hash(2, ts, sample("alice"), prev) != base);
    }
    SUBCASE("timestamp") {
        CHECK(compute_hash(1, "2026-07-12T00:00:01Z", sample("alice"), prev) != base);
    }
    SUBCASE("event field") {
        CHECK(compute_hash(1, ts, sample("mallory"), prev) != base);
    }
    SUBCASE("prev_hash") {
        CHECK(compute_hash(1, ts, sample("alice"), std::string(64, '1')) != base);
    }
}

TEST_CASE("integrity: a valid 3-block chain verifies") {
    std::vector<Block> chain = make_valid_chain();

    VerifyResult r = verify_chain(chain);

    CHECK(r.valid);
    CHECK(r.chain_length == 3);
    CHECK(r.checked_blocks == 3);
    CHECK_FALSE(r.first_invalid_index.has_value());
}

TEST_CASE("integrity: mutating a middle block fails verify at the right index") {
    std::vector<Block> chain = make_valid_chain();

    // Tamper block 1's payload without recomputing its stored hash.
    chain[1].event.actor = "mallory";

    VerifyResult r = verify_chain(chain);

    CHECK_FALSE(r.valid);
    REQUIRE(r.first_invalid_index.has_value());
    CHECK(r.first_invalid_index.value() == 1);
    // Verification stops at the first bad block: genesis + block 1 checked.
    CHECK(r.checked_blocks == 2);
}

TEST_CASE("genesis: validated against 64-zero prev_hash") {
    SUBCASE("genesis-only chain is valid") {
        std::vector<Block> chain{genesis()};
        VerifyResult r = verify_chain(chain);
        CHECK(r.valid);
        CHECK(r.checked_blocks == 1);
    }

    SUBCASE("genesis with a non-zero prev_hash fails at index 0") {
        // Build a genesis-like block whose prev_hash is not 64 zeros, with a
        // self-consistent hash so only the prev_hash rule can reject it.
        Block bad = make_block(0, sample("system"), std::string(64, 'a'));
        std::vector<Block> chain{bad};

        VerifyResult r = verify_chain(chain);
        CHECK_FALSE(r.valid);
        REQUIRE(r.first_invalid_index.has_value());
        CHECK(r.first_invalid_index.value() == 0);
    }
}
