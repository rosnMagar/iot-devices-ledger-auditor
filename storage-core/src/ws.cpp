#include <ws.hpp>

#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include <nlohmann/json.hpp>

#include <block.hpp>
#include <log.hpp>

namespace ledger {

namespace {

/// The only path this listener serves. Anything else is refused rather than
/// silently upgraded, so a client with a typo learns about it.
constexpr const char* kFeedPath = "/blocks";

/// 1008 = policy violation, the closest standard close code for "that path is
/// not something I serve".
constexpr std::uint16_t kCloseUnknownPath = 1008;

/// The request target can carry a query string; only the path selects a route.
std::string path_of(const std::string& uri) {
    const auto q = uri.find('?');
    return q == std::string::npos ? uri : uri.substr(0, q);
}

std::string lag_frame(std::size_t dropped) {
    return nlohmann::json{{"type", "lagged"}, {"dropped", dropped}}.dump();
}

}  // namespace

struct WsServer::Impl {
    /// One connected client: its subscription and the thread draining it.
    struct Conn {
        std::shared_ptr<Subscription> subscription;
        std::thread pump;
    };

    Broadcaster& broadcaster;
    ix::WebSocketServer server;

    mutable std::mutex mtx;
    std::unordered_map<std::string, Conn> conns;  // keyed by connection id
    bool running = false;

    Impl(Broadcaster& b, const std::string& host, int port)
        : broadcaster(b), server(port, host) {}

    /// Blocks on the subscription and writes frames until the client goes away
    /// or the subscription is closed.
    ///
    /// Holds a weak_ptr, never a shared_ptr: the WebSocket belongs to
    /// IXWebSocket's connection thread, and keeping it alive from here would
    /// outlive the connection it belongs to. Every send re-locks it, so a client
    /// that vanishes mid-stream ends the loop instead of writing to a dead
    /// socket.
    static void pump_loop(std::weak_ptr<ix::WebSocket> weak_ws,
                          std::shared_ptr<Subscription> subscription) {
        while (auto item = subscription->next()) {
            auto ws = weak_ws.lock();
            if (!ws) break;

            // The gap is announced before the block that follows it, so the
            // client can attribute the missing range correctly.
            if (item->lagged > 0) {
                log::info("ws client lagged, dropped " +
                          std::to_string(item->lagged) + " block(s)");
                ws->sendText(lag_frame(item->lagged));
            }

            ws->sendText(nlohmann::json(item->block).dump());
        }
    }

    void on_open(const std::string& id, std::weak_ptr<ix::WebSocket> weak_ws,
                 const std::string& uri) {
        auto ws = weak_ws.lock();
        if (!ws) return;

        if (path_of(uri) != kFeedPath) {
            log::info("ws connection to unknown path " + uri + ", refusing");
            ws->close(kCloseUnknownPath, "unknown path");
            return;
        }

        auto subscription = broadcaster.subscribe();

        std::lock_guard lock(mtx);
        // Subscribing after the lock would let a block published in between be
        // missed; subscribing before it is fine because a subscription with no
        // reader simply buffers.
        auto& conn = conns[id];
        conn.subscription = subscription;
        conn.pump = std::thread(pump_loop, weak_ws, subscription);
        log::info("ws client connected (" + std::to_string(conns.size()) +
                  " total)");
    }

    /// Called from the connection's own thread as its run() loop winds down.
    void on_close(const std::string& id) {
        Conn conn;
        {
            std::lock_guard lock(mtx);
            auto it = conns.find(id);
            if (it == conns.end()) return;  // never opened, or already closed
            conn = std::move(it->second);
            conns.erase(it);
        }

        // Outside the lock: close() wakes the pump out of next(), and joining
        // while holding the map lock would block every other connection's
        // open/close for the duration.
        if (conn.subscription) conn.subscription->close();
        if (conn.pump.joinable()) conn.pump.join();
        log::info("ws client disconnected");
    }

    void close_all() {
        std::unordered_map<std::string, Conn> taken;
        {
            std::lock_guard lock(mtx);
            taken.swap(conns);
        }
        for (auto& [id, conn] : taken) {
            if (conn.subscription) conn.subscription->close();
            if (conn.pump.joinable()) conn.pump.join();
        }
    }
};

WsServer::WsServer(Broadcaster& broadcaster, std::string host, int port)
    : impl_(std::make_unique<Impl>(broadcaster, host, port)) {}

WsServer::~WsServer() {
    stop();
}

bool WsServer::start() {
    Impl* impl = impl_.get();

    // setOnConnectionCallback rather than setOnClientMessageCallback: it is the
    // one that hands over a weak_ptr to the WebSocket, which is what makes a
    // disconnect during a send safe. IXWebSocket requires the message callback
    // to be registered from inside it.
    impl->server.setOnConnectionCallback(
        [impl](std::weak_ptr<ix::WebSocket> weak_ws,
               std::shared_ptr<ix::ConnectionState> state) {
            auto ws = weak_ws.lock();
            if (!ws) return;

            const std::string id = state->getId();
            ws->setOnMessageCallback(
                [impl, weak_ws, id](const ix::WebSocketMessagePtr& msg) {
                    switch (msg->type) {
                        case ix::WebSocketMessageType::Open:
                            impl->on_open(id, weak_ws, msg->openInfo.uri);
                            break;
                        case ix::WebSocketMessageType::Close:
                        case ix::WebSocketMessageType::Error:
                            impl->on_close(id);
                            break;
                        default:
                            // The feed is one-directional. Anything a client
                            // sends is ignored rather than treated as an error.
                            break;
                    }
                });
        });

    auto [ok, error] = impl->server.listen();
    if (!ok) {
        log::error("websocket listen failed: " + error);
        return false;
    }

    impl->server.start();
    impl->running = true;
    return true;
}

void WsServer::stop() {
    Impl* impl = impl_.get();
    if (impl == nullptr || !impl->running) return;
    impl->running = false;

    // Stop accepting and close the sockets first. That makes each connection
    // thread wind down and fire on_close, which joins its own pump.
    impl->server.stop();

    // Then sweep up anything that never delivered a Close — an aborted
    // connection, or one closed before it was fully open.
    impl->close_all();
}

std::size_t WsServer::connection_count() const {
    std::lock_guard lock(impl_->mtx);
    return impl_->conns.size();
}

}  // namespace ledger
