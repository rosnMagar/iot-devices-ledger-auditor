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

// ---------------------------------------------------------------------------
// writer_loop (IOT-25)
// ---------------------------------------------------------------------------

#include <filesystem>
#include <shared_mutex>
#include <string>

#include "broadcast.hpp"
#include "server.hpp"
#include "storage.hpp"

namespace {

/// A writer thread on a scratch ledger, stopped and joined on destruction so no
/// case leaks a thread into the next one.
///
/// `working == false` hands the thread an already-closed log handle, so every
/// append_block fails — that is how the persist-failure path is exercised
/// without depending on filesystem permissions.
struct WriterFixture {
    std::filesystem::path dir;
    AppState state;
    WriteQueue queue;
    Broadcaster broadcaster;
    std::thread thread;

    explicit WriterFixture(const std::string& tag, bool working = true,
                           std::size_t capacity = 256)
        : dir(std::filesystem::temp_directory_path() / ("iot_writer_test_" + tag)),
          queue(capacity) {
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        state.chain = load_chain(path());

        state.broadcaster = &broadcaster;

        std::ofstream log = open_append(path());
        if (!working) log.close();
        thread = std::thread(writer_loop, std::ref(queue), std::ref(state),
                             std::move(log));
    }

    ~WriterFixture() {
        queue.close();
        if (thread.joinable()) thread.join();
        std::filesystem::remove_all(dir);
    }

    std::filesystem::path path() const { return dir / "ledger.log"; }

    std::future<Block> submit(const std::string& actor) {
        auto req = make_request(actor);
        auto fut = req.respond_to.get_future();
        REQUIRE(queue.push(std::move(req)));
        return fut;
    }

    std::size_t chain_size() {
        std::shared_lock lock(state.mtx);
        return state.chain.size();
    }

    bool chain_valid() {
        std::shared_lock lock(state.mtx);
        return state.chain.verify().valid;
    }

    std::size_t lines_on_disk() const {
        std::ifstream in(path());
        std::string line;
        std::size_t n = 0;
        while (std::getline(in, line)) ++n;
        return n;
    }
};

}  // namespace

TEST_CASE("writer_loop persists a block, publishes it, and replies") {
    WriterFixture w("basic");
    auto fut = w.submit("alice");

    REQUIRE(fut.wait_for(2s) == std::future_status::ready);
    const Block got = fut.get();

    CHECK(got.index == 1);  // 0 is the in-memory genesis
    CHECK(got.event.actor == "alice");
    CHECK(got.prev_hash != std::string(64, '0'));  // chains onto genesis

    CHECK(w.chain_size() == 2);
    CHECK(w.chain_valid());
    CHECK(w.lines_on_disk() == 1);  // only the appended block; genesis was
                                    // seeded in memory by load_chain
}

TEST_CASE("writer_loop keeps the chain linked across many appends") {
    WriterFixture w("many");
    std::vector<std::future<Block>> futures;
    for (int i = 0; i < 20; ++i) futures.push_back(w.submit("a" + std::to_string(i)));

    for (std::size_t i = 0; i < futures.size(); ++i) {
        REQUIRE(futures[i].wait_for(2s) == std::future_status::ready);
        CHECK(futures[i].get().index == i + 1);
    }

    CHECK(w.chain_size() == 21);
    CHECK(w.chain_valid());
    CHECK(w.lines_on_disk() == 20);
}

TEST_CASE("concurrent submitters each get a distinct block, chain stays valid") {
    WriterFixture w("concurrent", true, 8);  // capacity below the load, so pushes block
    constexpr int kThreads = 6;
    constexpr int kEach = 10;

    std::mutex results_mtx;
    std::vector<Block> results;
    std::vector<std::thread> submitters;

    for (int t = 0; t < kThreads; ++t) {
        submitters.emplace_back([&, t] {
            for (int i = 0; i < kEach; ++i) {
                auto req = make_request("t" + std::to_string(t));
                auto fut = req.respond_to.get_future();
                w.queue.push(std::move(req));
                Block b = fut.get();
                std::lock_guard g(results_mtx);
                results.push_back(std::move(b));
            }
        });
    }
    for (auto& t : submitters) t.join();

    REQUIRE(results.size() == kThreads * kEach);

    // Every index 1..N handed out exactly once — two submitters must never be
    // given the same slot in the chain.
    std::vector<bool> seen(kThreads * kEach + 1, false);
    for (const auto& b : results) {
        REQUIRE(b.index >= 1);
        REQUIRE(b.index <= kThreads * kEach);
        CHECK_FALSE(seen[b.index]);
        seen[b.index] = true;
    }

    CHECK(w.chain_size() == kThreads * kEach + 1);
    CHECK(w.chain_valid());
}

TEST_CASE("a persist failure is reported to the caller and never enters the chain") {
    WriterFixture w("persist_fail", /*working=*/false);

    auto first = w.submit("alice");
    REQUIRE(first.wait_for(2s) == std::future_status::ready);
    CHECK_THROWS_AS(first.get(), StorageError);

    // The block must not be published — a chain entry with nothing on disk would
    // make every later block chain onto something a restart never sees.
    CHECK(w.chain_size() == 1);
    CHECK(w.chain_valid());

    // The thread is still alive and still serving: a second request gets its own
    // failure rather than hanging forever on a dead writer.
    auto second = w.submit("bob");
    REQUIRE(second.wait_for(2s) == std::future_status::ready);
    CHECK_THROWS_AS(second.get(), StorageError);
    CHECK(w.chain_size() == 1);
}

TEST_CASE("requests queued before close() are still served") {
    WriterFixture w("drain");
    auto a = w.submit("a");
    auto b = w.submit("b");

    w.queue.close();

    REQUIRE(a.wait_for(2s) == std::future_status::ready);
    REQUIRE(b.wait_for(2s) == std::future_status::ready);
    CHECK(a.get().index == 1);
    CHECK(b.get().index == 2);

    w.thread.join();  // the loop returned on its own once drained
    CHECK(w.lines_on_disk() == 2);
}

TEST_CASE("close() ends the loop so the thread can be joined") {
    WriterFixture w("shutdown");
    REQUIRE(w.submit("a").wait_for(2s) == std::future_status::ready);

    w.queue.close();
    w.thread.join();
    CHECK_FALSE(w.thread.joinable());
}

TEST_CASE("writer_loop publishes each appended block to the live feed") {
    WriterFixture w("publish");
    auto sub = w.broadcaster.subscribe();

    auto fut = w.submit("alice");
    REQUIRE(fut.wait_for(2s) == std::future_status::ready);
    const Block replied = fut.get();

    auto item = sub->next();
    REQUIRE(item.has_value());
    CHECK(item->lagged == 0);

    // The subscriber must be told about exactly the block the caller got back,
    // not a re-derived one.
    CHECK(item->block.index == replied.index);
    CHECK(item->block.hash == replied.hash);
    CHECK(item->block.event.actor == "alice");
}

TEST_CASE("a block that failed to persist is never published") {
    WriterFixture w("no_publish_on_failure", /*working=*/false);
    auto sub = w.broadcaster.subscribe();

    auto fut = w.submit("alice");
    REQUIRE(fut.wait_for(2s) == std::future_status::ready);
    CHECK_THROWS_AS(fut.get(), StorageError);

    // Publishing a block that is not on disk would show subscribers something a
    // restart would erase.
    CHECK(sub->buffered() == 0);
    CHECK_FALSE(sub->try_next().has_value());
}

TEST_CASE("a writer with no broadcaster attached still appends") {
    WriterFixture w("no_broadcaster");
    w.state.broadcaster = nullptr;  // nothing listening

    auto fut = w.submit("alice");
    REQUIRE(fut.wait_for(2s) == std::future_status::ready);
    CHECK(fut.get().index == 1);
    CHECK(w.chain_size() == 2);
}

TEST_CASE("a stalled subscriber does not stop the writer from accepting writes") {
    WriterFixture w("slow_subscriber");
    auto stalled = w.broadcaster.subscribe(4);  // never drained

    // Well past the subscriber's capacity: if publish() waited for room, the
    // writer thread would wedge and these futures would never resolve.
    std::vector<std::future<Block>> futures;
    for (int i = 0; i < 50; ++i) futures.push_back(w.submit("a"));

    for (auto& f : futures) REQUIRE(f.wait_for(5s) == std::future_status::ready);
    for (std::size_t i = 0; i < futures.size(); ++i) {
        CHECK(futures[i].get().index == i + 1);
    }

    CHECK(w.chain_size() == 51);
    CHECK(w.lines_on_disk() == 50);
    CHECK(stalled->buffered() == 4);
    CHECK(stalled->pending_lag() == 46);
}
