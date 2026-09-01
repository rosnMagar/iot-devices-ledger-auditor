#include <broadcast.hpp>

#include <algorithm>
#include <utility>

namespace ledger {

Subscription::Subscription(std::size_t capacity) : capacity_(capacity) {}

void Subscription::push(const Block& block) {
    {
        std::lock_guard lock(mtx_);
        if (closed_) return;

        // Full buffer means this subscriber is behind. Drop the oldest and
        // count it rather than waiting for room — waiting here would stall the
        // writer thread, which is the one thing this class must never do.
        if (buffer_.size() >= capacity_) {
            buffer_.pop_front();
            ++lagged_;
        }
        buffer_.push_back(block);
    }
    not_empty_.notify_one();
}

std::optional<Subscription::Item> Subscription::next() {
    std::unique_lock lock(mtx_);
    // Predicate form: a bare wait would let a spurious wakeup fall through to an
    // empty buffer.
    not_empty_.wait(lock, [this] { return closed_ || !buffer_.empty(); });

    // Drain before reporting closure, so blocks already published are still
    // delivered on shutdown.
    if (buffer_.empty()) return std::nullopt;

    Item item{std::move(buffer_.front()), lagged_};
    buffer_.pop_front();
    lagged_ = 0;  // reported exactly once
    return item;
}

std::optional<Subscription::Item> Subscription::try_next() {
    std::lock_guard lock(mtx_);
    if (buffer_.empty()) return std::nullopt;

    Item item{std::move(buffer_.front()), lagged_};
    buffer_.pop_front();
    lagged_ = 0;
    return item;
}

void Subscription::close() {
    {
        std::lock_guard lock(mtx_);
        closed_ = true;
    }
    not_empty_.notify_all();
}

bool Subscription::is_closed() const {
    std::lock_guard lock(mtx_);
    return closed_;
}

std::size_t Subscription::buffered() const {
    std::lock_guard lock(mtx_);
    return buffer_.size();
}

std::size_t Subscription::pending_lag() const {
    std::lock_guard lock(mtx_);
    return lagged_;
}

std::shared_ptr<Subscription> Broadcaster::subscribe(std::size_t capacity) {
    auto sub = std::make_shared<Subscription>(capacity);

    std::lock_guard lock(mtx_);
    // Prune here too: a server with subscribers coming and going but nothing
    // being published would otherwise grow the list forever.
    std::erase_if(subscribers_,
                  [](const std::weak_ptr<Subscription>& w) { return w.expired(); });
    subscribers_.push_back(sub);
    return sub;
}

void Broadcaster::publish(const Block& block) {
    std::lock_guard lock(mtx_);

    // Held across the fan-out, which is safe because push() never blocks: it
    // takes one subscriber's mutex, appends, and returns. A subscriber's next()
    // only ever takes its own mutex, so there is no lock-order inversion.
    for (auto it = subscribers_.begin(); it != subscribers_.end();) {
        if (auto sub = it->lock()) {
            sub->push(block);
            ++it;
        } else {
            // The client disconnected and dropped its handle.
            it = subscribers_.erase(it);
        }
    }
}

void Broadcaster::close_all() {
    std::lock_guard lock(mtx_);
    for (auto& weak : subscribers_) {
        if (auto sub = weak.lock()) sub->close();
    }
    subscribers_.clear();
}

std::size_t Broadcaster::subscriber_count() const {
    std::lock_guard lock(mtx_);
    return static_cast<std::size_t>(
        std::count_if(subscribers_.begin(), subscribers_.end(),
                      [](const std::weak_ptr<Subscription>& w) {
                          return !w.expired();
                      }));
}

}  // namespace ledger
