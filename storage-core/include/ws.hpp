#pragma once
#include <memory>
#include <string>

#include <broadcast.hpp>

namespace ledger {

/// The live block feed: a WebSocket listener that streams newly appended blocks
/// to every connected client.
///
/// **A second listener on its own port**, not a path on 8080. cpp-httplib and
/// IXWebSocket each own a listening socket and cannot share one — see
/// docs/decisions/0008-vendored-websocket-second-listener.md.
///
/// ## Threading
/// IXWebSocket runs an accept thread plus one thread per connection. Each
/// connection gets a *third* thread here — a "pump" that blocks on its
/// Subscription and writes frames.
///
/// The pump has to be separate: the connection's own thread sits inside
/// `WebSocket::run()` dispatching callbacks, and that is also how a disconnect
/// is noticed. Blocking it on `Subscription::next()` would mean a client that
/// goes away while no blocks are being published is never detected, and its
/// subscription leaks until shutdown.
///
/// ## Wire format
/// Two frame shapes, both JSON text:
/// - a **block**, serialised exactly as `GET /blocks` serialises it, so a client
///   can reuse one parser for history and live updates;
/// - a **lag notice**, `{"type":"lagged","dropped":N}`, sent immediately before
///   the block that follows the gap.
///
/// A client tells them apart by the `type` field, which a block never has. The
/// lag notice exists so a dashboard that fell behind shows a gap it knows about
/// rather than a chain with silent holes in it; the client can backfill with
/// `GET /blocks?from=`.
class WsServer {
public:
    /// Does not bind — call start(). `broadcaster` must outlive this object.
    WsServer(Broadcaster& broadcaster, std::string host, int port);
    ~WsServer();

    WsServer(const WsServer&) = delete;
    WsServer& operator=(const WsServer&) = delete;

    /// Bind and begin accepting. Returns false if the bind fails (port in use,
    /// bad host), leaving the object safely destructible.
    bool start();

    /// Stop accepting, close every open connection, and join every pump thread.
    /// Idempotent, and called by the destructor.
    void stop();

    /// Connections currently attached. For tests and logging.
    std::size_t connection_count() const;

private:
    struct Impl;
    // pimpl: keeps the IXWebSocket headers out of everything that includes this.
    std::unique_ptr<Impl> impl_;
};

}  // namespace ledger
