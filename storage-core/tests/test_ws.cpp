#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>

#include "block.hpp"
#include "broadcast.hpp"
#include "ws.hpp"

using namespace ledger;
using namespace std::chrono_literals;

namespace {

Block block_n(std::uint64_t n) {
    return make_block(n,
                      EventPayload{"DOOR_OPEN", "loc-1", "a" + std::to_string(n),
                                   "d" + std::to_string(n),
                                   nlohmann::json::object()},
                      std::string(64, '0'));
}

/// IXWebSocket's getPort() only echoes back the port it was configured with —
/// it does not resolve an OS-assigned ephemeral port the way httplib's
/// bind_to_any_port does. So a test cannot ask for port 0 and be told what it
/// got; it has to pick one and cope with it being taken.
///
/// Each fixture walks up from a per-suite base until a bind succeeds.
std::atomic<int> g_next_port{19100};

/// A listener on a free port, stopped on destruction.
struct TestWs {
    Broadcaster broadcaster;
    std::unique_ptr<WsServer> server;
    int port = 0;

    TestWs() {
        for (int attempt = 0; attempt < 50; ++attempt) {
            const int candidate = g_next_port.fetch_add(1);
            auto s = std::make_unique<WsServer>(broadcaster, "127.0.0.1", candidate);
            if (s->start()) {
                server = std::move(s);
                port = candidate;
                return;
            }
        }
        FAIL("could not bind a websocket listener on any candidate port");
    }

    ~TestWs() {
        if (server) server->stop();
    }

    std::string url() const {
        return "ws://127.0.0.1:" + std::to_string(port) + "/blocks";
    }
};

/// A client that records every text frame it receives.
struct TestClient {
    ix::WebSocket ws;

    mutable std::mutex mtx;
    std::condition_variable cv;
    std::vector<std::string> frames;
    std::atomic<bool> open{false};
    std::atomic<bool> closed{false};

    explicit TestClient(const std::string& url) {
        ws.setUrl(url);
        ws.disableAutomaticReconnection();
        ws.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
            switch (msg->type) {
                case ix::WebSocketMessageType::Open:
                    open = true;
                    break;
                case ix::WebSocketMessageType::Close:
                    closed = true;
                    break;
                case ix::WebSocketMessageType::Message: {
                    std::lock_guard lock(mtx);
                    frames.push_back(msg->str);
                    cv.notify_all();
                    break;
                }
                default:
                    break;
            }
        });
    }

    ~TestClient() { ws.stop(); }

    void connect() {
        ws.start();
        // Poll rather than sleep a fixed amount: the handshake is usually
        // sub-millisecond locally, and waiting a flat 500ms in every case adds
        // up across the suite.
        for (int i = 0; i < 200 && !open.load(); ++i) {
            std::this_thread::sleep_for(5ms);
        }
    }

    /// Wait until at least `n` frames have arrived, or give up.
    bool wait_for_frames(std::size_t n, std::chrono::milliseconds timeout = 3s) {
        std::unique_lock lock(mtx);
        return cv.wait_for(lock, timeout, [&] { return frames.size() >= n; });
    }

    std::vector<std::string> snapshot() const {
        std::lock_guard lock(mtx);
        return frames;
    }
};

/// Give the server a moment to register the connection. The client's Open
/// callback fires when *its* handshake completes, which can very slightly
/// precede the server finishing its own bookkeeping.
void wait_for_connections(const TestWs& ws, std::size_t expected) {
    for (int i = 0; i < 400; ++i) {
        if (ws.server->connection_count() == expected) return;
        std::this_thread::sleep_for(5ms);
    }
}

}  // namespace

TEST_CASE("a connected client receives published blocks as JSON frames") {
    TestWs ws;
    TestClient client(ws.url());
    client.connect();
    REQUIRE(client.open.load());
    wait_for_connections(ws, 1);
    CHECK(ws.server->connection_count() == 1);

    ws.broadcaster.publish(block_n(1));

    REQUIRE(client.wait_for_frames(1));
    const auto frames = client.snapshot();
    REQUIRE(frames.size() >= 1);

    // The block is serialised exactly as GET /blocks serialises it, so a client
    // can use one parser for both history and live updates.
    const auto parsed = nlohmann::json::parse(frames[0]);
    CHECK(parsed["index"] == 1);
    CHECK(parsed["event"]["actor"] == "a1");
    CHECK(parsed["hash"].get<std::string>().size() == 64);
    CHECK_FALSE(parsed.contains("type"));  // how a client tells it from a notice
}

TEST_CASE("blocks arrive in order") {
    TestWs ws;
    TestClient client(ws.url());
    client.connect();
    wait_for_connections(ws, 1);

    for (std::uint64_t i = 1; i <= 10; ++i) ws.broadcaster.publish(block_n(i));

    REQUIRE(client.wait_for_frames(10));
    const auto frames = client.snapshot();
    REQUIRE(frames.size() >= 10);
    for (std::size_t i = 0; i < 10; ++i) {
        CHECK(nlohmann::json::parse(frames[i])["index"] == i + 1);
    }
}

TEST_CASE("two clients each receive every block") {
    TestWs ws;
    TestClient first(ws.url());
    TestClient second(ws.url());
    first.connect();
    second.connect();
    wait_for_connections(ws, 2);
    CHECK(ws.server->connection_count() == 2);

    ws.broadcaster.publish(block_n(1));
    ws.broadcaster.publish(block_n(2));

    REQUIRE(first.wait_for_frames(2));
    REQUIRE(second.wait_for_frames(2));
    CHECK(nlohmann::json::parse(first.snapshot()[1])["index"] == 2);
    CHECK(nlohmann::json::parse(second.snapshot()[1])["index"] == 2);
}

TEST_CASE("a late client sees only blocks published after it connected") {
    TestWs ws;
    ws.broadcaster.publish(block_n(1));  // nobody is listening yet

    TestClient client(ws.url());
    client.connect();
    wait_for_connections(ws, 1);

    ws.broadcaster.publish(block_n(2));

    REQUIRE(client.wait_for_frames(1));
    const auto frames = client.snapshot();
    // Only the block published after connecting — history is GET /blocks' job.
    CHECK(nlohmann::json::parse(frames[0])["index"] == 2);
}

TEST_CASE("a lagging client is told it missed blocks") {
    TestWs ws;
    TestClient client(ws.url());
    client.connect();
    wait_for_connections(ws, 1);

    // Far more than a subscription buffers (64), published faster than the
    // socket can drain, so the ring buffer is guaranteed to drop.
    for (std::uint64_t i = 1; i <= 400; ++i) ws.broadcaster.publish(block_n(i));

    REQUIRE(client.wait_for_frames(1));
    // Let the stream settle so the notice, wherever it lands, has arrived.
    std::this_thread::sleep_for(500ms);

    bool saw_lag_notice = false;
    std::size_t dropped_total = 0;
    for (const auto& frame : client.snapshot()) {
        const auto parsed = nlohmann::json::parse(frame);
        if (parsed.contains("type") && parsed["type"] == "lagged") {
            saw_lag_notice = true;
            CHECK(parsed["dropped"].get<std::size_t>() > 0);
            dropped_total += parsed["dropped"].get<std::size_t>();
        }
    }

    // A silently truncated feed would show a chain with holes and no sign
    // anything was wrong — the notice is the whole point of IOT-27's counter.
    CHECK(saw_lag_notice);
    CHECK(dropped_total > 0);
}

TEST_CASE("a connection to an unknown path is refused") {
    TestWs ws;
    TestClient client("ws://127.0.0.1:" + std::to_string(ws.port) + "/nope");
    client.connect();

    // Either the upgrade never completes or the server closes it immediately;
    // what matters is that it does not end up subscribed to the feed.
    for (int i = 0; i < 200 && ws.server->connection_count() != 0; ++i) {
        std::this_thread::sleep_for(5ms);
    }
    CHECK(ws.server->connection_count() == 0);

    ws.broadcaster.publish(block_n(1));
    std::this_thread::sleep_for(200ms);
    CHECK(client.snapshot().empty());
}

TEST_CASE("a client that connects and immediately disconnects leaks nothing") {
    TestWs ws;

    for (int i = 0; i < 10; ++i) {
        TestClient client(ws.url());
        client.connect();
        client.ws.stop();  // gone again straight away
    }

    for (int i = 0; i < 400 && ws.server->connection_count() != 0; ++i) {
        std::this_thread::sleep_for(5ms);
    }
    CHECK(ws.server->connection_count() == 0);

    // The listener is still healthy afterwards.
    TestClient survivor(ws.url());
    survivor.connect();
    wait_for_connections(ws, 1);
    ws.broadcaster.publish(block_n(7));
    REQUIRE(survivor.wait_for_frames(1));
    CHECK(nlohmann::json::parse(survivor.snapshot()[0])["index"] == 7);
}

TEST_CASE("a client that never reads does not stop the publisher") {
    TestWs ws;
    TestClient stalled(ws.url());
    stalled.connect();
    wait_for_connections(ws, 1);

    // The publisher stands in for the writer thread. If a stalled socket could
    // block publish(), this would not finish.
    const auto started = std::chrono::steady_clock::now();
    for (std::uint64_t i = 1; i <= 2000; ++i) ws.broadcaster.publish(block_n(i));
    const auto elapsed = std::chrono::steady_clock::now() - started;

    CHECK(elapsed < 5s);
    CHECK(ws.server->connection_count() == 1);
}

TEST_CASE("publishing with no clients connected is harmless") {
    TestWs ws;
    CHECK(ws.server->connection_count() == 0);
    ws.broadcaster.publish(block_n(1));
    CHECK(ws.server->connection_count() == 0);
}

TEST_CASE("stop() closes connected clients and is idempotent") {
    TestWs ws;
    TestClient client(ws.url());
    client.connect();
    wait_for_connections(ws, 1);

    ws.server->stop();
    ws.server->stop();  // second call must be a no-op, not a crash or a hang

    CHECK(ws.server->connection_count() == 0);
    for (int i = 0; i < 200 && !client.closed.load(); ++i) {
        std::this_thread::sleep_for(5ms);
    }
    CHECK(client.closed.load());
}

TEST_CASE("client messages are ignored — the feed is one-directional") {
    TestWs ws;
    TestClient client(ws.url());
    client.connect();
    wait_for_connections(ws, 1);

    client.ws.sendText("hello?");
    std::this_thread::sleep_for(200ms);

    // No echo, no error, still connected and still working.
    CHECK(client.snapshot().empty());
    CHECK(ws.server->connection_count() == 1);

    ws.broadcaster.publish(block_n(1));
    REQUIRE(client.wait_for_frames(1));
    CHECK(nlohmann::json::parse(client.snapshot()[0])["index"] == 1);
}
