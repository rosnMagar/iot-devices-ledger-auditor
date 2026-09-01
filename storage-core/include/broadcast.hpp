#pragma once
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include <block.hpp>

namespace ledger {

class Broadcaster;

/// One subscriber's view of the live block feed: a bounded ring buffer with its
/// own mutex and condition variable.
///
/// The rule this type exists to enforce is that **the writer thread never blocks
/// on a subscriber**. When a consumer falls behind, its buffer fills and the
/// oldest block is dropped to make room — the publisher does not wait. One
/// stalled dashboard tab must not be able to stop the ledger from accepting
/// writes.
///
/// Drop-oldest rather than drop-newest, because the value of a live feed is
/// recency: a client that reconnects wants the latest state, and can backfill
/// anything it missed from GET /blocks.
///
/// Obtained from Broadcaster::subscribe(); not constructible directly, and held
/// by shared_ptr so a client disconnecting mid-publish cannot pull the buffer
/// out from under the writer.
class Subscription {
public:
    /// 64 blocks of slack before a consumer starts losing them. Enough to ride
    /// out a brief stall without letting a dead connection hold memory forever.
    static constexpr std::size_t kDefaultCapacity = 64;

    /// One delivery: the block, plus how many were dropped immediately before it
    /// because this subscriber was too slow.
    ///
    /// `lagged` is the C++ stand-in for Tokio's `RecvError::Lagged` — it lets
    /// IOT-28 tell the client it missed blocks instead of silently showing a
    /// feed with holes in it. It is reported once, on the first delivery after
    /// the drops, then reset.
    struct Item {
        Block block;
        std::size_t lagged = 0;
    };

    explicit Subscription(std::size_t capacity = kDefaultCapacity);

    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;

    /// Block until a block is available, then return it. Returns nullopt once
    /// the subscription is closed *and* drained — buffered blocks are still
    /// delivered, so a shutdown does not throw away what was already published.
    std::optional<Item> next();

    /// Non-blocking variant: nullopt if nothing is buffered right now.
    std::optional<Item> try_next();

    /// Wake a blocked next() so the consumer thread can exit. Idempotent.
    void close();

    bool is_closed() const;
    std::size_t buffered() const;

    /// Drops not yet reported to the consumer.
    std::size_t pending_lag() const;

private:
    friend class Broadcaster;

    /// Called by Broadcaster under its subscriber-list lock. Never blocks.
    void push(const Block& block);

    mutable std::mutex mtx_;
    std::condition_variable not_empty_;
    std::deque<Block> buffer_;
    std::size_t capacity_;
    std::size_t lagged_ = 0;
    bool closed_ = false;
};

/// Fans each newly appended block out to every live subscriber.
///
/// Called by the writer thread after a successful append, so this is on the
/// write path: publish() must be fast and must never block. It is O(subscribers)
/// with one deque push each.
class Broadcaster {
public:
    /// A new subscriber sees only blocks published from now on. Backfill is the
    /// caller's job via GET /blocks — mixing history into the live feed would
    /// mean holding the chain lock here, on the write path.
    std::shared_ptr<Subscription> subscribe(
        std::size_t capacity = Subscription::kDefaultCapacity);

    /// Copy `block` into every live subscriber's buffer. A no-op when there are
    /// none, which is the normal state of a server nobody is watching.
    void publish(const Block& block);

    /// Close every subscription, so consumer threads waiting in next() wake and
    /// return. Used on shutdown.
    void close_all();

    /// Live subscribers, expired ones excluded. Mainly for tests and logging.
    std::size_t subscriber_count() const;

private:
    mutable std::mutex mtx_;

    /// weak_ptr, so a subscription deregisters itself simply by being destroyed.
    ///
    /// The alternative — the handle calling back into the Broadcaster from its
    /// destructor — needs the subscriber-list lock at exactly the moment
    /// publish() may be holding it, and a Broadcaster that outlives none of its
    /// handles becomes a dangling back-pointer. Expired entries are pruned
    /// during publish() and subscribe(), so nothing accumulates.
    std::vector<std::weak_ptr<Subscription>> subscribers_;
};

}  // namespace ledger
