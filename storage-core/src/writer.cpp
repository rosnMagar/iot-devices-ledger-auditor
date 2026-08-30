#include <writer.hpp>

#include <utility>

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

}  // namespace ledger
