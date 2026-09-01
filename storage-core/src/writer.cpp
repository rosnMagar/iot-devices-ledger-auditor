#include <writer.hpp>

#include <exception>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <utility>

#include <broadcast.hpp>
#include <log.hpp>
#include <server.hpp>
#include <storage.hpp>

namespace ledger {

WriteQueue::WriteQueue(std::size_t capacity) : capacity_(capacity) {}

bool WriteQueue::push(WriteRequest&& request) {
    {
        std::unique_lock lock(mtx_);
        // Predicate form, never a bare wait(): condition variables wake
        // spuriously, and a bare wait would let a pusher proceed into a full
        // queue.
        not_full_.wait(lock, [this] { return closed_ || queue_.size() < capacity_; });

        // Move only once acceptance is certain — see the header: a refused push
        // must leave the caller's promise intact.
        if (closed_) return false;
        queue_.push(std::move(request));
    }
    not_empty_.notify_one();
    return true;
}

std::optional<WriteRequest> WriteQueue::pop() {
    WriteRequest request;
    {
        std::unique_lock lock(mtx_);
        not_empty_.wait(lock, [this] { return closed_ || !queue_.empty(); });

        // Drain before reporting closure, so requests already accepted are still
        // served rather than silently dropped on shutdown.
        if (queue_.empty()) return std::nullopt;

        // front() hands back a reference; move out of it before popping, since
        // WriteRequest cannot be copied.
        request = std::move(queue_.front());
        queue_.pop();
    }
    not_full_.notify_one();
    return request;
}

void WriteQueue::close() {
    {
        std::lock_guard lock(mtx_);
        closed_ = true;
    }
    // notify_all, not notify_one: every blocked pusher and popper has to learn
    // about this, or shutdown hangs on whichever thread wasn't woken.
    not_empty_.notify_all();
    not_full_.notify_all();
}

bool WriteQueue::is_closed() const {
    std::lock_guard lock(mtx_);
    return closed_;
}

std::size_t WriteQueue::size() const {
    std::lock_guard lock(mtx_);
    return queue_.size();
}

namespace {

/// Hash the next block from the current tail. Takes a shared lock only to read
/// the tail: the writer thread is the sole mutator, so nothing can move it
/// underneath, and other readers run concurrently.
Block build_next(AppState& state, EventPayload event) {
    std::shared_lock lock(state.mtx);
    return make_block(state.chain.size(), std::move(event),
                      state.chain.latest().hash);
}

}  // namespace

void writer_loop(WriteQueue& queue, AppState& state, std::ofstream log) {
    log::info("writer thread started");

    try {
        while (auto request = queue.pop()) {
            // Move the promise out first, so it is fulfilled exactly once no
            // matter which branch below runs.
            std::promise<Block> respond_to = std::move(request->respond_to);

            try {
                Block created = build_next(state, std::move(request->event));

                // Persist BEFORE publishing. If this throws, the block was never
                // in the chain, so there is nothing to roll back — unlike the
                // handler in IOT-21, which appends first and has to undo it.
                append_block(log, created);

                Block reply;
                {
                    std::unique_lock lock(state.mtx);
                    reply = state.chain.push_persisted(std::move(created));
                }

                // Fan out only after the block is both on disk and visible to
                // readers, so a subscriber can never be told about a block that
                // GET /blocks would not yet return.
                //
                // Outside the write lock: publish() is bounded and never
                // blocks, but there is no reason to make readers wait on it.
                if (state.broadcaster != nullptr) {
                    state.broadcaster->publish(reply);
                }

                respond_to.set_value(std::move(reply));
            } catch (const StorageError& e) {
                log::error("failed to persist block: " + std::string(e.what()));
                // Hand the failure to the waiting handler, which maps it to a
                // 500. Rethrowing here would kill the thread and strand every
                // later request.
                try {
                    respond_to.set_exception(std::current_exception());
                } catch (...) {
                }
            } catch (...) {
                log::error("writer: unexpected error handling a write request");
                try {
                    respond_to.set_exception(std::current_exception());
                } catch (...) {
                }
            }
        }
    } catch (const std::exception& e) {
        // Only reachable if the queue itself fails. Nothing left to reply on.
        log::error("writer thread aborting: " + std::string(e.what()));
    } catch (...) {
        log::error("writer thread aborting on an unknown error");
    }

    log::info("writer thread stopped");
}

}  // namespace ledger
