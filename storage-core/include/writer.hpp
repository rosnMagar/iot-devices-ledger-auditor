#pragma once
#include <condition_variable>
#include <cstddef>
#include <future>
#include <mutex>
#include <optional>
#include <queue>

#include <fstream>

#include <block.hpp>

namespace ledger {

// Defined in server.hpp, which includes this header — forward-declared to keep
// the include one-directional.
struct AppState;

/// One pending append, plus the channel its result travels back on.
///
/// Move-only: std::promise is not copyable, so neither is this. The queue below
/// moves requests in and out rather than copying them.
struct WriteRequest {
    EventPayload event;
    std::promise<Block> respond_to;
};

/// A bounded, blocking, closable queue of write requests.
///
/// Bounded on purpose. A mutex on the write path gives you an unbounded implicit
/// queue of blocked threads; a fixed capacity makes the limit visible and turns
/// overload into backpressure that a caller can observe. See the 2026-08-30
/// amendment to docs/decisions/0003-ownership-based-concurrency.md — this is not
/// a correctness fix, the existing unique_lock is already correct.
class WriteQueue {
public:
    static constexpr std::size_t kDefaultCapacity = 256;

    explicit WriteQueue(std::size_t capacity = kDefaultCapacity);

    /// Enqueue, blocking while the queue is full.
    ///
    /// Takes an rvalue reference rather than by value so that a *refused* push
    /// leaves the caller's request untouched. By value, `push(std::move(req))`
    /// would strip the promise whether or not the queue accepted it, and the
    /// caller's future would throw `broken_promise` instead of letting the
    /// handler report a clean failure.
    ///
    /// Returns false if the queue is closed; the request is then still yours.
    bool push(WriteRequest&& request);

    /// Dequeue, blocking while the queue is empty.
    /// Returns std::nullopt once the queue is closed *and* drained, which is how
    /// the writer thread learns to exit.
    std::optional<WriteRequest> pop();

    /// Refuse further pushes and wake every blocked thread.
    ///
    /// Without this a thread parked in pop() would never return and main() could
    /// not join the writer. Idempotent.
    void close();

    bool is_closed() const;
    std::size_t size() const;

private:
    mutable std::mutex mtx_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::queue<WriteRequest> queue_;
    std::size_t capacity_;
    bool closed_ = false;
};

/// The single writer. Owns `log` for its whole lifetime and is the only thread
/// that ever mutates `state.chain`.
///
/// Runs until the queue is closed *and* drained, then returns so main() can join
/// it. Never throws: an exception escaping a std::thread's entry point calls
/// std::terminate and takes the process down, so every failure is reported back
/// through the requesting handler's future instead.
///
/// Takes the write lock only for the vector push. The hash and the file write
/// happen outside it, so a slow disk delays writers but never readers.
void writer_loop(WriteQueue& queue, AppState& state, std::ofstream log);

}  // namespace ledger
