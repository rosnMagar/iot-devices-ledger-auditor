#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

#include "block.hpp"
#include "writer.hpp"

using namespace ledger;
using namespace std::chrono_literals;

namespace {

WriteRequest make_request(const std::string& actor = "alice") {
    return WriteRequest{
        EventPayload{"DOOR_OPEN", "loc-1", actor, "door opened",
                     nlohmann::json::object()},
        std::promise<Block>{}};
}

}  // namespace

TEST_CASE("push then pop returns the same request") {
    WriteQueue q;
    auto req = make_request("alice");
    auto fut = req.respond_to.get_future();

    REQUIRE(q.push(std::move(req)));
    CHECK(q.size() == 1);

    auto got = q.pop();
    REQUIRE(got.has_value());
    CHECK(got->event.actor == "alice");
    CHECK(q.size() == 0);

    // The promise survived the two moves and still drives the original future.
    got->respond_to.set_value(genesis());
    CHECK(fut.get().event.event_type == "GENESIS");
}

TEST_CASE("FIFO order is preserved") {
    WriteQueue q;
    for (int i = 0; i < 5; ++i) {
        REQUIRE(q.push(make_request("actor-" + std::to_string(i))));
    }
    for (int i = 0; i < 5; ++i) {
        auto got = q.pop();
        REQUIRE(got.has_value());
        CHECK(got->event.actor == "actor-" + std::to_string(i));
    }
}

TEST_CASE("push blocks while the queue is full, and resumes when drained") {
    WriteQueue q(2);
    REQUIRE(q.push(make_request("a")));
    REQUIRE(q.push(make_request("b")));
    CHECK(q.size() == 2);

    std::atomic<bool> pushed{false};
    std::thread producer([&] {
        q.push(make_request("c"));  // must block: capacity is 2
        pushed = true;
    });

    // Give the producer time to park on the condition variable. This sleep is a
    // *negative* check — it asserts nothing happened — so it cannot cause a
    // false pass the way sleeping to wait for something can.
    std::this_thread::sleep_for(100ms);
    CHECK_FALSE(pushed.load());

    auto drained = q.pop();  // frees a slot
    REQUIRE(drained.has_value());
    producer.join();
    CHECK(pushed.load());
    CHECK(q.size() == 2);
}

TEST_CASE("pop blocks while empty and wakes on a push") {
    WriteQueue q;
    std::promise<std::string> got_actor;
    auto fut = got_actor.get_future();

    std::thread consumer([&] {
        auto req = q.pop();  // must block: queue is empty
        got_actor.set_value(req.has_value() ? req->event.actor : "<none>");
    });

    CHECK(fut.wait_for(100ms) == std::future_status::timeout);  // still blocked
    REQUIRE(q.push(make_request("late")));

    REQUIRE(fut.wait_for(2s) == std::future_status::ready);
    CHECK(fut.get() == "late");
    consumer.join();
}

TEST_CASE("close() wakes a blocked pop so the writer thread can exit") {
    WriteQueue q;
    std::promise<bool> returned;
    auto fut = returned.get_future();

    std::thread consumer([&] {
        auto req = q.pop();
        returned.set_value(req.has_value());
    });

    CHECK(fut.wait_for(100ms) == std::future_status::timeout);
    q.close();

    REQUIRE(fut.wait_for(2s) == std::future_status::ready);
    CHECK_FALSE(fut.get());  // nullopt: closed and empty
    consumer.join();
}

TEST_CASE("close() wakes a blocked push, which then reports failure") {
    WriteQueue q(1);
    REQUIRE(q.push(make_request("a")));

    std::promise<bool> accepted;
    auto fut = accepted.get_future();
    std::thread producer([&] { accepted.set_value(q.push(make_request("b"))); });

    CHECK(fut.wait_for(100ms) == std::future_status::timeout);
    q.close();

    REQUIRE(fut.wait_for(2s) == std::future_status::ready);
    CHECK_FALSE(fut.get());  // refused, so the caller still owns its promise
    producer.join();
}

TEST_CASE("a closed queue drains before reporting nullopt") {
    WriteQueue q;
    REQUIRE(q.push(make_request("a")));
    REQUIRE(q.push(make_request("b")));

    q.close();

    // Requests already accepted are still served — dropping them on shutdown
    // would leave their futures broken.
    auto first = q.pop();
    REQUIRE(first.has_value());
    CHECK(first->event.actor == "a");
    auto second = q.pop();
    REQUIRE(second.has_value());
    CHECK(second->event.actor == "b");

    CHECK_FALSE(q.pop().has_value());
    CHECK(q.is_closed());
}

TEST_CASE("push on a closed queue is refused and leaves the promise usable") {
    WriteQueue q;
    q.close();

    auto req = make_request("a");
    auto fut = req.respond_to.get_future();
    CHECK_FALSE(q.push(std::move(req)));

    // push() returning false means it never took ownership, so the caller can
    // still fail the request itself rather than leaving the future hanging.
    req.respond_to.set_value(genesis());
    CHECK(fut.wait_for(1s) == std::future_status::ready);
}

TEST_CASE("close() is idempotent") {
    WriteQueue q;
    q.close();
    q.close();
    CHECK(q.is_closed());
}

TEST_CASE("many producers and one consumer lose nothing") {
    constexpr int kProducers = 8;
    constexpr int kPerProducer = 50;
    WriteQueue q(16);  // deliberately smaller than the load, to force blocking

    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&q, p] {
            for (int i = 0; i < kPerProducer; ++i) {
                q.push(make_request("p" + std::to_string(p)));
            }
        });
    }

    int received = 0;
    std::thread consumer([&] {
        while (received < kProducers * kPerProducer) {
            if (q.pop().has_value()) ++received;
        }
    });

    for (auto& t : producers) t.join();
    consumer.join();
    q.close();

    CHECK(received == kProducers * kPerProducer);
    CHECK(q.size() == 0);
}
