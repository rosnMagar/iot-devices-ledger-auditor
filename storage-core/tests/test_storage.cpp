#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "block.hpp"
#include "chain.hpp"
#include "storage.hpp"

using namespace ledger;

namespace {

EventPayload sample(const std::string& actor = "alice") {
    return EventPayload{"DOOR_OPEN", "loc-1", actor, "door opened",
                        nlohmann::json::object()};
}

// A scratch directory unique to one test case, removed on destruction so cases
// never share on-disk state.
struct TempDir {
    std::filesystem::path dir;

    explicit TempDir(const std::string& tag)
        : dir(std::filesystem::temp_directory_path() /
              ("iot_storage_test_" + tag)) {
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
    }
    ~TempDir() { std::filesystem::remove_all(dir); }

    std::filesystem::path file(const std::string& name = "ledger.log") const {
        return dir / name;
    }
};

// Read every line of a file into a vector (helper for the tamper test).
std::vector<std::string> read_lines(const std::filesystem::path& path) {
    std::vector<std::string> lines;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) lines.push_back(line);
    return lines;
}

}  // namespace

TEST_CASE("roundtrip: append N blocks then load_chain yields an identical chain") {
    TempDir tmp("roundtrip");
    const auto path = tmp.file();

    // Build a genesis + two-event chain in memory and persist it verbatim.
    Chain original;
    original.append(sample("alice"));
    original.append(sample("bob"));
    {
        std::ofstream os = open_append(path);
        for (const auto& b : original.blocks()) append_block(os, b);
    }

    Chain loaded = load_chain(path);

    REQUIRE(loaded.size() == original.size());
    for (std::size_t i = 0; i < loaded.size(); ++i) {
        CHECK(loaded.blocks()[i].index == original.blocks()[i].index);
        CHECK(loaded.blocks()[i].hash == original.blocks()[i].hash);
        CHECK(loaded.blocks()[i].prev_hash == original.blocks()[i].prev_hash);
    }
    // A cleanly persisted chain still verifies after reload.
    CHECK(loaded.verify().valid);
}

TEST_CASE("tamper on disk: corrupting a line makes reload's verify() fail at that index") {
    TempDir tmp("tamper");
    const auto path = tmp.file();

    Chain original;
    original.append(sample("alice"));  // block 1 carries actor "alice"
    original.append(sample("bob"));
    {
        std::ofstream os = open_append(path);
        for (const auto& b : original.blocks()) append_block(os, b);
    }

    // Rewrite block 1's actor in the raw NDJSON without fixing its stored hash.
    // Same-length replacement keeps the JSON well-formed so the line still parses
    // and load_chain accepts it verbatim — exactly what tamper-evidence must catch.
    std::vector<std::string> lines = read_lines(path);
    REQUIRE(lines.size() == 3);
    const auto pos = lines[1].find("alice");
    REQUIRE(pos != std::string::npos);
    lines[1].replace(pos, 5, "mallo");
    {
        std::ofstream out(path, std::ios::trunc);
        for (const auto& l : lines) out << l << '\n';
    }

    Chain tampered = load_chain(path);  // loaded verbatim, no hash recompute
    VerifyResult r = tampered.verify();

    CHECK_FALSE(r.valid);
    REQUIRE(r.first_invalid_index.has_value());
    CHECK(r.first_invalid_index.value() == 1);
}

TEST_CASE("fresh boot: empty or missing file seeds a genesis-only chain") {
    SUBCASE("missing file") {
        TempDir tmp("missing");
        Chain c = load_chain(tmp.file("does-not-exist.log"));
        REQUIRE(c.size() == 1);
        CHECK(c.latest().event.event_type == "GENESIS");
        CHECK(c.verify().valid);
    }

    SUBCASE("empty file") {
        TempDir tmp("empty");
        const auto path = tmp.file();
        std::ofstream{path};  // touch: create a 0-byte file
        Chain c = load_chain(path);
        REQUIRE(c.size() == 1);
        CHECK(c.latest().event.event_type == "GENESIS");
        CHECK(c.verify().valid);
    }
}

TEST_CASE("malformed line: load_chain throws StorageError naming the line number") {
    TempDir tmp("malformed");
    const auto path = tmp.file();
    {
        std::ofstream out(path);
        out << nlohmann::json(genesis()).dump() << '\n';  // line 1 ok
        out << "{not valid json\n";                       // line 2 broken
    }

    // Catch manually: the parser's message is verbose/version-dependent, so we
    // assert on the substring we control (the line number) rather than an exact
    // match. The exception type must be StorageError, not a raw json exception.
    bool threw = false;
    try {
        load_chain(path);
    } catch (const StorageError& e) {
        threw = true;
        CHECK(std::string(e.what()).find("line 2") != std::string::npos);
    }
    CHECK(threw);
}
