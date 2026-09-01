#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "block.hpp"
#include "broadcast.hpp"

using namespace ledger;
using namespace std::chrono_literals;

namespace {

/// A block with a recognisable actor, so a test can tell deliveries apart.
/// The hashes are not meaningful here — the broadcaster never inspects them.
Block block_n(std::uint64_t n) {
    return make_block(n,
                      EventPayload{"DOOR_OPEN", "loc-1", "a" + std::to_string(n),
                                   "d" + std::to_string(n),
                                   nlohmann::json::object()},
                      std::string(64, '0'));
}

std::string actor_of(const Subscription::Item& item) {
    return item.block.event.actor;
}

}  // namespace

TEST_CASE("publish with no subscribers is a no-op") {
    Broadcaster b;
    b.publish(block_n(1));  // must not throw
    CHECK(b.subscriber_count() == 0);
}

TEST_CASE("one block fans out to every subscriber") {
    Broadcaster b;
    auto first = b.subscribe();
    auto second = b.subscribe();
    CHECK(b.subscriber_count() == 2);

    b.publish(block_n(1));

    auto a = first->next();
    auto c = second->next();
    REQUIRE(a.has_value());
    REQUIRE(c.has_value());
    CHECK(actor_of(*a) == "a1");
    CHECK(actor_of(*c) == "a1");
    CHECK(a->lagged == 0);
    CHECK(c->lagged == 0);
}

TEST_CASE("subscribers are independent — draining one leaves the other buffered") {
    Broadcaster b;
    auto fast = b.subscribe();
    auto slow = b.subscribe();

    b.publish(block_n(1));
    b.publish(block_n(2));

    REQUIRE(fast->next().has_value());
    REQUIRE(fast->next().has_value());
    CHECK(fast->buffered() == 0);

    // The slow one still has both: one consumer's progress is not the other's.
    CHECK(slow->buffered() == 2);
}

TEST_CASE("a late subscriber sees only blocks published after it subscribed") {
    Broadcaster b;
    auto early = b.subscribe();

    b.publish(block_n(1));
    b.publish(block_n(2));

    auto late = b.subscribe();
    CHECK(late->buffered() == 0);  // no backfill — that is GET /blocks' job

    b.publish(block_n(3));

    auto got = late->next();
    REQUIRE(got.has_value());
    CHECK(actor_of(*got) == "a3");
    CHECK_FALSE(late->try_next().has_value());

    // The early subscriber has all three, in order.
    for (int i = 1; i <= 3; ++i) {
        auto item = early->next();
        REQUIRE(item.has_value());
        CHECK(actor_of(*item) == "a" + std::to_string(i));
    }
}

TEST_CASE("a full buffer drops the OLDEST block and reports the lag once") {
    Broadcaster b;
    auto sub = b.subscribe(3);  // tiny capacity, so overflow is easy to reach

    for (std::uint64_t i = 1; i <= 5; ++i) b.publish(block_n(i));

    CHECK(sub->buffered() == 3);      // capped
    CHECK(sub->pending_lag() == 2);   // 1 and 2 were dropped

    // Drop-oldest: the newest blocks survive, because the live feed's value is
    // recency. The lag is reported on the first delivery, then cleared.
    auto first = sub->next();
    REQUIRE(first.has_value());
    CHECK(actor_of(*first) == "a3");
    CHECK(first->lagged == 2);

    auto second = sub->next();
    REQUIRE(second.has_value());
    CHECK(actor_of(*second) == "a4");
    CHECK(second->lagged == 0);  // reported once, not on every later block

    auto third = sub->next();
    REQUIRE(third.has_value());
    CHECK(actor_of(*third) == "a5");
}

TEST_CASE("a slow subscriber never blocks the publisher") {
    Broadcaster b;
    auto slow = b.subscribe(4);
    auto fast = b.subscribe(1024);

    // Far more than the slow buffer holds. If publish() waited for room this
    // would deadlock, since nothing is draining `slow`.
    std::atomic<bool> done{false};
    std::thread publisher([&] {
        for (std::uint64_t i = 1; i <= 500; ++i) b.publish(block_n(i));
        done = true;
    });
    publisher.join();

    CHECK(done.load());
    CHECK(slow->buffered() == 4);
    CHECK(slow->pending_lag() == 496);

    // The subscriber that kept up lost nothing.
    CHECK(fast->buffered() == 500);
    CHECK(fast->pending_lag() == 0);
}

TEST_CASE("next() blocks until a block arrives") {
    Broadcaster b;
    auto sub = b.subscribe();

    std::promise<std::string> got;
    auto fut = got.get_future();
    std::thread consumer([&] {
        auto item = sub->next();
        got.set_value(item.has_value() ? actor_of(*item) : "<none>");
    });

    CHECK(fut.wait_for(100ms) == std::future_status::timeout);  // still waiting
    b.publish(block_n(7));

    REQUIRE(fut.wait_for(2s) == std::future_status::ready);
    CHECK(fut.get() == "a7");
    consumer.join();
}

TEST_CASE("close() wakes a blocked next() so the consumer thread can exit") {
    Broadcaster b;
    auto sub = b.subscribe();

    std::promise<bool> returned;
    auto fut = returned.get_future();
    std::thread consumer([&] { returned.set_value(sub->next().has_value()); });

    CHECK(fut.wait_for(100ms) == std::future_status::timeout);
    sub->close();

    REQUIRE(fut.wait_for(2s) == std::future_status::ready);
    CHECK_FALSE(fut.get());
    consumer.join();
}

TEST_CASE("a closed subscription still delivers what was already buffered") {
    Broadcaster b;
    auto sub = b.subscribe();

    b.publish(block_n(1));
    b.publish(block_n(2));
    sub->close();

    // Shutdown must not throw away blocks the consumer had already been given.
    auto first = sub->next();
    REQUIRE(first.has_value());
    CHECK(actor_of(*first) == "a1");
    auto second = sub->next();
    REQUIRE(second.has_value());
    CHECK(actor_of(*second) == "a2");

    CHECK_FALSE(sub->next().has_value());
    CHECK(sub->is_closed());
}

TEST_CASE("publishing to a closed subscription is dropped, not buffered") {
    Broadcaster b;
    auto sub = b.subscribe();
    sub->close();

    b.publish(block_n(1));
    CHECK(sub->buffered() == 0);
}

TEST_CASE("close_all() releases every consumer") {
    Broadcaster b;
    auto first = b.subscribe();
    auto second = b.subscribe();

    std::vector<std::thread> consumers;
    std::atomic<int> exited{0};
    for (auto* sub : {&first, &second}) {
        consumers.emplace_back([sub, &exited] {
            while ((*sub)->next().has_value()) {
            }
            ++exited;
        });
    }

    b.close_all();
    for (auto& t : consumers) t.join();
    CHECK(exited.load() == 2);
}

TEST_CASE("a dropped handle deregisters itself") {
    Broadcaster b;
    auto keep = b.subscribe();
    {
        auto temporary = b.subscribe();
        CHECK(b.subscriber_count() == 2);
    }
    CHECK(b.subscriber_count() == 1);

    // Publishing prunes the expired entry rather than tripping over it.
    b.publish(block_n(1));
    CHECK(keep->buffered() == 1);
    CHECK(b.subscriber_count() == 1);
}

TEST_CASE("subscribing and unsubscribing while publishing is safe") {
    Broadcaster b;
    std::atomic<bool> stop{false};

    // A publisher hammering away while clients come and go — the shape of a
    // dashboard being opened and closed during live traffic.
    std::thread publisher([&] {
        for (std::uint64_t i = 1; !stop.load(); ++i) {
            b.publish(block_n(i % 100));
            std::this_thread::yield();
        }
    });

    std::vector<std::thread> churn;
    for (int t = 0; t < 4; ++t) {
        churn.emplace_back([&] {
            for (int i = 0; i < 200; ++i) {
                auto sub = b.subscribe(8);
                sub->try_next();
                // sub goes out of scope here, deregistering mid-publish
            }
        });
    }
    for (auto& t : churn) t.join();
    stop = true;
    publisher.join();

    CHECK(b.subscriber_count() == 0);
}

TEST_CASE("many concurrent publishers deliver every block to a subscriber") {
    Broadcaster b;
    constexpr int kPublishers = 4;
    constexpr int kEach = 100;
    auto sub = b.subscribe(kPublishers * kEach);  // large enough to lose nothing

    std::vector<std::thread> publishers;
    for (int p = 0; p < kPublishers; ++p) {
        publishers.emplace_back([&] {
            for (int i = 0; i < kEach; ++i) b.publish(block_n(1));
        });
    }
    for (auto& t : publishers) t.join();

    CHECK(sub->buffered() == kPublishers * kEach);
    CHECK(sub->pending_lag() == 0);
}
