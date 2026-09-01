#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "block.hpp"
#include "chain.hpp"
#include "server.hpp"
#include "writer.hpp"
#include "storage.hpp"

using namespace ledger;

namespace {

/// A real server on a real socket, driven by a real HTTP client.
///
/// Bound to an ephemeral port on 127.0.0.1 (`bind_to_any_port` returns the one
/// the OS picked) so cases never collide with each other or with a storage-core
/// running on 8080 on the dev box. Each instance gets its own temp ledger, so
/// no case sees another's blocks.
///
/// Neither copyable nor movable — AppState holds a std::shared_mutex. Construct
/// it as a local in the test case.
struct TestServer {
    std::filesystem::path dir;
    AppState state;
    WriteQueue queue;
    httplib::Server srv;
    int port = 0;
    std::thread thread;
    std::thread writer;

    /// `seed` runs against the ledger path before the server loads it, so a case
    /// can start from a chain the API itself could never produce — a tampered
    /// one, for instance.
    using SeedFn = std::function<void(const std::filesystem::path&)>;

    explicit TestServer(const std::string& tag, const SeedFn& seed = nullptr)
        : dir(std::filesystem::temp_directory_path() /
              ("iot_server_test_" + tag)) {
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);

        const auto path = dir / "ledger.log";
        if (seed) seed(path);
        state.chain = load_chain(path);  // missing file -> genesis-only chain

        // POST /events writes through the writer thread now (IOT-26), so a
        // server without one cannot append at all. The fixture owns the same
        // pieces main() does: a queue, the log handle moved into the thread.
        state.write_queue = &queue;
        writer = std::thread(writer_loop, std::ref(queue), std::ref(state),
                             open_append(path));

        install_routes(srv, state);

        port = srv.bind_to_any_port("127.0.0.1");
        thread = std::thread([this] { srv.listen_after_bind(); });
        srv.wait_until_ready();
    }

    /// Torn down in the same order main() uses: stop taking requests first, so
    /// no handler is left waiting on a future the writer will never fulfil,
    /// then close the queue and let the writer drain and exit.
    ~TestServer() {
        srv.stop();
        if (thread.joinable()) thread.join();
        queue.close();  // idempotent
        // joinable(), because a case may have shut the writer down itself to
        // exercise what the API does once the write path is closed.
        if (writer.joinable()) writer.join();
        std::filesystem::remove_all(dir);
    }

    httplib::Client client() const { return httplib::Client("127.0.0.1", port); }
};

std::string event_json(const std::string& description) {
    return nlohmann::json{{"event_type", "DOOR_OPEN"},
                          {"location_id", "loc-1"},
                          {"actor", "alice"},
                          {"description", description},
                          {"metadata", nlohmann::json::object()}}
        .dump();
}

/// Well-formed JSON with every field present, but event_type blank — the one
/// rejection the handler makes itself rather than leaving to the parser.
std::string event_json_empty_type() {
    return nlohmann::json{{"event_type", ""},
                          {"location_id", "loc-1"},
                          {"actor", "alice"},
                          {"description", "no type"},
                          {"metadata", nlohmann::json::object()}}
        .dump();
}

nlohmann::json body_of(const httplib::Result& res) {
    return nlohmann::json::parse(res->body);
}

}  // namespace

TEST_CASE("GET /health reports the process is serving") {
    TestServer ts("health");
    auto res = ts.client().Get("/health");

    REQUIRE(res);
    CHECK(res->status == 200);
    CHECK(body_of(res)["status"] == "ok");
}

TEST_CASE("POST /events appends a block and returns it") {
    TestServer ts("post");
    auto res = ts.client().Post("/events", event_json("door opened"),
                                "application/json");

    REQUIRE(res);
    CHECK(res->status == 201);

    const auto block = body_of(res);
    CHECK(block["index"] == 1);  // 0 is genesis
    CHECK(block["event"]["description"] == "door opened");
    CHECK(block["hash"].get<std::string>().size() == 64);
    // The new block must chain onto genesis, not onto 64 zeros.
    CHECK(block["prev_hash"].get<std::string>() != std::string(64, '0'));
}

TEST_CASE("POST /events rejects bad input with 400") {
    TestServer ts("post_bad");
    auto c = ts.client();

    SUBCASE("malformed JSON") {
        auto res = c.Post("/events", "{not json", "application/json");
        REQUIRE(res);
        CHECK(res->status == 400);
    }

    SUBCASE("missing a required field") {
        auto res = c.Post("/events", R"({"event_type":"DOOR_OPEN"})",
                          "application/json");
        REQUIRE(res);
        CHECK(res->status == 400);
    }

    SUBCASE("empty event_type") {
        auto res = c.Post("/events", event_json_empty_type(), "application/json");
        REQUIRE(res);
        CHECK(res->status == 400);
    }

    // A rejected write must not reach the chain.
    auto res = c.Get("/blocks");
    REQUIRE(res);
    CHECK(body_of(res)["chain_length"] == 1);
}

TEST_CASE("POST then GET /blocks: the event is readable back and the chain still verifies") {
    TestServer ts("roundtrip");
    auto c = ts.client();

    auto posted = c.Post("/events", event_json("valve opened"),
                         "application/json");
    REQUIRE(posted);
    REQUIRE(posted->status == 201);
    const auto created = body_of(posted);

    auto listed = c.Get("/blocks");
    REQUIRE(listed);
    CHECK(listed->status == 200);

    const auto body = body_of(listed);
    REQUIRE(body["chain_length"] == 2);
    REQUIRE(body["blocks"].size() == 2);
    // The block that came back from POST is byte-for-byte the one in the chain.
    CHECK(body["blocks"][1] == created);

    auto verified = c.Get("/verify");
    REQUIRE(verified);
    CHECK(verified->status == 200);
    CHECK(body_of(verified)["valid"] == true);
    CHECK(body_of(verified)["chain_length"] == 2);
}

TEST_CASE("GET /blocks honours an inclusive from/to range") {
    TestServer ts("range");
    auto c = ts.client();
    for (int i = 1; i <= 3; ++i) {
        REQUIRE(c.Post("/events", event_json("e" + std::to_string(i)),
                       "application/json"));
    }
    // genesis + 3 events
    REQUIRE(body_of(c.Get("/blocks"))["chain_length"] == 4);

    SUBCASE("no params returns the whole chain") {
        CHECK(body_of(c.Get("/blocks"))["blocks"].size() == 4);
    }

    SUBCASE("both ends are inclusive") {
        const auto b = body_of(c.Get("/blocks?from=1&to=2"));
        REQUIRE(b["blocks"].size() == 2);
        CHECK(b["blocks"][0]["index"] == 1);
        CHECK(b["blocks"][1]["index"] == 2);
        // chain_length is the whole chain, not the slice.
        CHECK(b["chain_length"] == 4);
    }

    SUBCASE("from defaults to 0, to defaults to the latest") {
        CHECK(body_of(c.Get("/blocks?to=1"))["blocks"].size() == 2);
        CHECK(body_of(c.Get("/blocks?from=2"))["blocks"].size() == 2);
    }

    SUBCASE("to past the end clamps instead of failing") {
        CHECK(body_of(c.Get("/blocks?to=9999"))["blocks"].size() == 4);
    }
}

TEST_CASE("GET /blocks rejects an invalid range with 400") {
    TestServer ts("range_bad");
    auto c = ts.client();
    c.Post("/events", event_json("e1"), "application/json");

    for (const char* query : {"?from=3&to=1", "?from=99", "?from=abc", "?from=-1",
                              "?from=", "?from=12abc",
                              "?to=99999999999999999999"}) {
        CAPTURE(query);
        auto res = c.Get(std::string("/blocks") + query);
        REQUIRE(res);
        CHECK(res->status == 400);
        CHECK(body_of(res).contains("error"));
    }
}

TEST_CASE("GET /verify returns the full result shape") {
    TestServer ts("verify");
    auto c = ts.client();
    c.Post("/events", event_json("e1"), "application/json");

    auto res = c.Get("/verify");
    REQUIRE(res);
    CHECK(res->status == 200);

    const auto body = body_of(res);
    CHECK(body["valid"] == true);
    CHECK(body["chain_length"] == 2);
    CHECK(body["checked_blocks"] == 2);
    // Always present, null when the chain is intact, so consumers can read it
    // unconditionally.
    REQUIRE(body.contains("first_invalid_index"));
    CHECK(body["first_invalid_index"].is_null());
    CHECK(body["verified_at"].get<std::string>().size() == 20);  // RFC3339 Z
}

TEST_CASE("GET /verify reports a tampered chain as invalid and names the block") {
    // Seed a ledger the API could never have written: block 1's actor is edited
    // without recomputing its stored hash. The replacement is the same length so
    // the line still parses and load_chain accepts it verbatim — precisely what
    // tamper-evidence has to catch.
    TestServer ts("tampered", [](const std::filesystem::path& path) {
        Chain c;
        c.append(EventPayload{"DOOR_OPEN", "loc-1", "alice", "door opened",
                              nlohmann::json::object()});
        {
            std::ofstream os = open_append(path);
            for (const auto& b : c.blocks()) append_block(os, b);
        }

        std::vector<std::string> lines;
        {
            std::ifstream in(path);
            std::string line;
            while (std::getline(in, line)) lines.push_back(line);
        }
        REQUIRE(lines.size() == 2);
        const auto pos = lines[1].find("alice");
        REQUIRE(pos != std::string::npos);
        lines[1].replace(pos, 5, "mallo");

        std::ofstream out(path, std::ios::trunc);
        for (const auto& l : lines) out << l << '\n';
    });

    auto res = ts.client().Get("/verify");
    REQUIRE(res);
    CHECK(res->status == 200);  // the endpoint works; the *chain* is what failed

    const auto body = body_of(res);
    CHECK(body["valid"] == false);
    CHECK(body["chain_length"] == 2);
    REQUIRE_FALSE(body["first_invalid_index"].is_null());
    CHECK(body["first_invalid_index"] == 1);
}

TEST_CASE("CORS headers are present so the dashboard can call the API directly") {
    TestServer ts("cors");
    auto res = ts.client().Get("/blocks");

    REQUIRE(res);
    CHECK(res->get_header_value("Access-Control-Allow-Origin") == "*");
}

TEST_CASE("an unknown route is a 404, not a crash") {
    TestServer ts("notfound");
    auto res = ts.client().Get("/nope");

    REQUIRE(res);
    CHECK(res->status == 404);
}

// ---------------------------------------------------------------------------
// The write path now runs through the writer thread (IOT-26)
// ---------------------------------------------------------------------------

TEST_CASE("concurrent POSTs each get their own block and the chain stays valid") {
    TestServer ts("concurrent_post");
    constexpr int kClients = 6;
    constexpr int kEach = 5;

    // Results are collected here and asserted on the main thread afterwards.
    // doctest's assertion macros are not safe to call from several threads at
    // once, and a failure inside a client thread would be reported against
    // whichever case happened to be running.
    std::mutex results_mtx;
    std::vector<int> statuses;
    std::vector<std::uint64_t> indices;

    std::vector<std::thread> clients;
    for (int c = 0; c < kClients; ++c) {
        clients.emplace_back([&, c] {
            httplib::Client cli("127.0.0.1", ts.port);
            for (int i = 0; i < kEach; ++i) {
                auto res = cli.Post("/events", event_json("c" + std::to_string(c)),
                                    "application/json");
                std::lock_guard g(results_mtx);
                if (!res) {
                    statuses.push_back(-1);
                    continue;
                }
                statuses.push_back(res->status);
                if (res->status == 201) {
                    indices.push_back(body_of(res)["index"].get<std::uint64_t>());
                }
            }
        });
    }
    for (auto& t : clients) t.join();

    REQUIRE(statuses.size() == kClients * kEach);
    for (int status : statuses) CHECK(status == 201);

    // The point of the single writer: no two requests may be handed the same
    // slot in the chain, however they interleave.
    REQUIRE(indices.size() == kClients * kEach);
    std::sort(indices.begin(), indices.end());
    CHECK(std::adjacent_find(indices.begin(), indices.end()) == indices.end());
    CHECK(indices.front() == 1);  // 0 is genesis
    CHECK(indices.back() == kClients * kEach);

    // And the ledger the server reports must still be internally consistent.
    auto verify = ts.client().Get("/verify");
    REQUIRE(verify);
    CHECK(verify->status == 200);
    CHECK(body_of(verify)["valid"] == true);
    CHECK(body_of(verify)["chain_length"] == kClients * kEach + 1);
}

TEST_CASE("POST /events is a 500 when no writer thread is attached") {
    // A server whose write_queue was never set. This is a wiring mistake rather
    // than something a client can provoke, but it must fail loudly instead of
    // answering 201 for an event that was never written anywhere.
    AppState state;  // write_queue stays null
    httplib::Server srv;
    install_routes(srv, state);

    const int port = srv.bind_to_any_port("127.0.0.1");
    std::thread thread([&] { srv.listen_after_bind(); });
    srv.wait_until_ready();

    httplib::Client cli("127.0.0.1", port);
    auto res = cli.Post("/events", event_json("nowhere to go"), "application/json");

    REQUIRE(res);
    CHECK(res->status == 500);
    CHECK(body_of(res)["error"] == "writer unavailable");

    srv.stop();
    thread.join();
}

TEST_CASE("a closed queue makes POST /events fail instead of hanging") {
    TestServer ts("closed_queue");

    // Shutdown has begun: the writer has drained and exited, but the HTTP
    // server is still accepting. push() refuses, and the handler must turn that
    // into a response rather than blocking on a promise nobody owns.
    ts.queue.close();
    ts.writer.join();

    auto res = ts.client().Post("/events", event_json("too late"),
                                "application/json");
    REQUIRE(res);
    CHECK(res->status == 500);

    // Reads still work — a closed write path does not take the whole API down.
    auto blocks = ts.client().Get("/blocks");
    REQUIRE(blocks);
    CHECK(blocks->status == 200);
    CHECK(body_of(blocks)["chain_length"] == 1);
}
