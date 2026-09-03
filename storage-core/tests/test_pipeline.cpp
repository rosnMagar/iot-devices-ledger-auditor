// IOT-29 — the Weekend 4 capstone.
//
// Everything else tests one link in the chain. This tests the seam: a real
// POST /events travelling handler -> queue -> writer thread -> disk ->
// broadcaster -> WebSocket frame, in one process, and proves that what a
// subscriber receives is exactly what the ledger recorded.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <httplib.h>
#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>

#include "block.hpp"
#include "broadcast.hpp"
#include "chain.hpp"
#include "server.hpp"
#include "storage.hpp"
#include "writer.hpp"
#include "ws.hpp"

using namespace ledger;
using namespace std::chrono_literals;

namespace {

/// IXWebSocket cannot report an OS-assigned port (see the ticket), so the WS
/// side walks up from a base until a bind succeeds. Hard-coding one would
/// collide with a dev box already running storage-core.
std::atomic<int> g_next_ws_port{19300};

/// The whole service in one object: chain, queue, writer thread, broadcaster,
/// REST server and WebSocket listener — assembled the way main() assembles them,
/// and torn down in the same order.
struct Pipeline {
    std::filesystem::path dir;
    AppState state;
    WriteQueue queue;
    Broadcaster broadcaster;
    std::thread writer;

    httplib::Server srv;
    int http_port = 0;
    std::thread http_thread;

    std::unique_ptr<WsServer> ws;
    int ws_port = 0;

    explicit Pipeline(const std::string& tag)
        : dir(std::filesystem::temp_directory_path() / ("iot_pipeline_" + tag)) {
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);

        const auto path = dir / "ledger.log";
        state.chain = load_chain(path);
        state.write_queue = &queue;
        state.broadcaster = &broadcaster;

        writer = std::thread(writer_loop, std::ref(queue), std::ref(state),
                             open_append(path));

        install_routes(srv, state);
        http_port = srv.bind_to_any_port("127.0.0.1");
        http_thread = std::thread([this] { srv.listen_after_bind(); });
        srv.wait_until_ready();

        for (int attempt = 0; attempt < 50; ++attempt) {
            const int candidate = g_next_ws_port.fetch_add(1);
            auto candidate_server =
                std::make_unique<WsServer>(broadcaster, "127.0.0.1", candidate);
            if (candidate_server->start()) {
                ws = std::move(candidate_server);
                ws_port = candidate;
                break;
            }
        }
        REQUIRE(ws != nullptr);
    }

    /// Same order main() uses: stop accepting, then close the write path, then
    /// release subscribers.
    ~Pipeline() {
        srv.stop();
        if (http_thread.joinable()) http_thread.join();
        if (ws) ws->stop();
        queue.close();
        if (writer.joinable()) writer.join();
        broadcaster.close_all();
        std::filesystem::remove_all(dir);
    }

    httplib::Client client() const {
        return httplib::Client("127.0.0.1", http_port);
    }

    std::string ws_url() const {
        return "ws://127.0.0.1:" + std::to_string(ws_port) + "/blocks";
    }

    /// POST an event and return the block the API says it created.
    nlohmann::json post_event(const std::string& actor) {
        const std::string body = nlohmann::json{{"event_type", "DOOR_OPEN"},
                                                {"location_id", "loc-1"},
                                                {"actor", actor},
                                                {"description", "d-" + actor},
                                                {"metadata", nlohmann::json::object()}}
                                     .dump();
        auto res = client().Post("/events", body, "application/json");
        REQUIRE(res);
        REQUIRE(res->status == 201);
        return nlohmann::json::parse(res->body);
    }
};

/// A WebSocket client that records frames and lets a test block on their
/// arrival — a condition variable fed by the on-message callback, never a fixed
/// sleep.
struct FeedClient {
    ix::WebSocket ws;

    mutable std::mutex mtx;
    std::condition_variable cv;
    std::vector<std::string> frames;
    std::atomic<bool> open{false};

    explicit FeedClient(const std::string& url) {
        ws.setUrl(url);
        ws.disableAutomaticReconnection();
        ws.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
            if (msg->type == ix::WebSocketMessageType::Open) {
                open = true;
            } else if (msg->type == ix::WebSocketMessageType::Message) {
                std::lock_guard lock(mtx);
                frames.push_back(msg->str);
                cv.notify_all();
            }
        });
        ws.start();
    }

    ~FeedClient() { ws.stop(); }

    /// Block until `n` frames have arrived. Returns false on timeout, so the
    /// test fails loudly instead of hanging CI.
    bool wait_for(std::size_t n, std::chrono::milliseconds timeout = 5s) {
        std::unique_lock lock(mtx);
        return cv.wait_for(lock, timeout, [&] { return frames.size() >= n; });
    }

    std::vector<std::string> snapshot() const {
        std::lock_guard lock(mtx);
        return frames;
    }

    std::size_t count() const {
        std::lock_guard lock(mtx);
        return frames.size();
    }
};

/// Wait until the *server* agrees the expected clients are attached.
///
/// This is the step that keeps the capstone from being flaky: publishing before
/// the subscription exists sends the block into the void, and on a fast machine
/// the POST easily wins that race. `connection_count()` is the server's own
/// view, so there is nothing to guess at — and no hello frame had to be added to
/// the wire protocol just to make the test observable.
bool wait_for_subscribers(const Pipeline& p, std::size_t expected,
                          std::chrono::milliseconds timeout = 5s) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (p.ws->connection_count() == expected) return true;
        std::this_thread::sleep_for(2ms);
    }
    return false;
}

}  // namespace

TEST_CASE("capstone: POST /events reaches a WebSocket subscriber as the same block") {
    Pipeline p("capstone");
    FeedClient feed(p.ws_url());
    REQUIRE(wait_for_subscribers(p, 1));

    const nlohmann::json posted = p.post_event("alice");

    REQUIRE(feed.wait_for(1));
    const nlohmann::json received = nlohmann::json::parse(feed.snapshot()[0]);

    // The whole point: not "a block arrived", but *this* block arrived, whole.
    CHECK(received == posted);
    CHECK(received["index"] == 1);
    CHECK(received["event"]["actor"] == "alice");
    CHECK(received["hash"].get<std::string>().size() == 64);
}

TEST_CASE("the streamed block is identical to what GET /blocks returns") {
    Pipeline p("consistency");
    FeedClient feed(p.ws_url());
    REQUIRE(wait_for_subscribers(p, 1));

    p.post_event("alice");
    REQUIRE(feed.wait_for(1));
    const nlohmann::json streamed = nlohmann::json::parse(feed.snapshot()[0]);

    auto res = p.client().Get("/blocks?from=1&to=1");
    REQUIRE(res);
    REQUIRE(res->status == 200);
    const nlohmann::json rest = nlohmann::json::parse(res->body)["blocks"][0];

    // One parser for history and live updates only works if these agree
    // exactly — including field order, since both are nlohmann dumps.
    CHECK(streamed == rest);
    CHECK(streamed.dump() == rest.dump());
}

TEST_CASE("the streamed block is the one that was persisted") {
    Pipeline p("durability");
    FeedClient feed(p.ws_url());
    REQUIRE(wait_for_subscribers(p, 1));

    p.post_event("alice");
    REQUIRE(feed.wait_for(1));
    const nlohmann::json streamed = nlohmann::json::parse(feed.snapshot()[0]);

    // Read the ledger file directly. A block that was broadcast but never
    // written would survive every in-memory check and vanish on restart.
    std::ifstream in(p.dir / "ledger.log");
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(in, line)) lines.push_back(line);

    REQUIRE(lines.size() == 1);  // genesis was seeded in memory, not written
    CHECK(nlohmann::json::parse(lines[0]) == streamed);
}

TEST_CASE("fan-out: two subscribers both receive the block") {
    Pipeline p("fanout");
    FeedClient first(p.ws_url());
    FeedClient second(p.ws_url());
    REQUIRE(wait_for_subscribers(p, 2));

    const nlohmann::json posted = p.post_event("alice");

    REQUIRE(first.wait_for(1));
    REQUIRE(second.wait_for(1));
    CHECK(nlohmann::json::parse(first.snapshot()[0]) == posted);
    CHECK(nlohmann::json::parse(second.snapshot()[0]) == posted);
}

TEST_CASE("late subscriber isolation: a client connecting after the POST sees nothing") {
    Pipeline p("late");

    // Posted with nobody listening.
    p.post_event("early");

    FeedClient late(p.ws_url());
    REQUIRE(wait_for_subscribers(p, 1));

    // A bounded wait for something *not* to happen. Kept short, and paired with
    // the positive assertion below so a feed that is simply broken cannot pass
    // this case by delivering nothing at all.
    CHECK_FALSE(late.wait_for(1, 300ms));
    CHECK(late.count() == 0);

    const nlohmann::json posted = p.post_event("later");
    REQUIRE(late.wait_for(1));
    CHECK(nlohmann::json::parse(late.snapshot()[0]) == posted);
    CHECK(nlohmann::json::parse(late.snapshot()[0])["index"] == 2);
}

TEST_CASE("a burst of POSTs arrives in chain order and stays linked") {
    Pipeline p("burst");
    FeedClient feed(p.ws_url());
    REQUIRE(wait_for_subscribers(p, 1));

    constexpr int kEvents = 25;
    for (int i = 0; i < kEvents; ++i) p.post_event("a" + std::to_string(i));

    REQUIRE(feed.wait_for(kEvents));
    const auto frames = feed.snapshot();
    REQUIRE(frames.size() >= kEvents);

    // The feed must present the chain as a chain: consecutive indices, each
    // block's prev_hash matching the previous block's hash.
    std::string previous_hash;
    for (int i = 0; i < kEvents; ++i) {
        const auto block = nlohmann::json::parse(frames[i]);
        CHECK(block["index"] == i + 1);
        if (!previous_hash.empty()) {
            CHECK(block["prev_hash"] == previous_hash);
        }
        previous_hash = block["hash"].get<std::string>();
    }

    auto verify = p.client().Get("/verify");
    REQUIRE(verify);
    CHECK(nlohmann::json::parse(verify->body)["valid"] == true);
    CHECK(nlohmann::json::parse(verify->body)["chain_length"] == kEvents + 1);
}

TEST_CASE("concurrent POSTs are all delivered, each exactly once") {
    Pipeline p("concurrent");
    FeedClient feed(p.ws_url());
    REQUIRE(wait_for_subscribers(p, 1));

    constexpr int kClients = 4;
    constexpr int kEach = 5;
    std::vector<std::thread> posters;
    for (int c = 0; c < kClients; ++c) {
        posters.emplace_back([&p, c] {
            for (int i = 0; i < kEach; ++i) {
                p.post_event("c" + std::to_string(c) + "-" + std::to_string(i));
            }
        });
    }
    for (auto& t : posters) t.join();

    REQUIRE(feed.wait_for(kClients * kEach));
    std::vector<bool> seen(kClients * kEach + 1, false);
    for (const auto& frame : feed.snapshot()) {
        const auto index = nlohmann::json::parse(frame)["index"].get<std::size_t>();
        REQUIRE(index >= 1);
        REQUIRE(index <= kClients * kEach);
        CHECK_FALSE(seen[index]);  // no duplicates on the wire
        seen[index] = true;
    }
    for (std::size_t i = 1; i < seen.size(); ++i) CHECK(seen[i]);  // none missing
}

TEST_CASE("a subscriber disconnecting does not disturb the write path") {
    Pipeline p("disconnect");
    {
        FeedClient transient(p.ws_url());
        REQUIRE(wait_for_subscribers(p, 1));
        p.post_event("before");
        REQUIRE(transient.wait_for(1));
    }  // client goes away here

    REQUIRE(wait_for_subscribers(p, 0));

    // The API keeps working with nobody listening, and the chain is unbroken.
    const nlohmann::json posted = p.post_event("after");
    CHECK(posted["index"] == 2);

    auto verify = p.client().Get("/verify");
    REQUIRE(verify);
    CHECK(nlohmann::json::parse(verify->body)["valid"] == true);
    CHECK(nlohmann::json::parse(verify->body)["chain_length"] == 3);
}
