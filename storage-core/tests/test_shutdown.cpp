// IOT-54 — graceful shutdown.
//
// This is the one suite that drives the real binary as a child process. Signal
// handling cannot be tested in-process: the behaviour under test is what the
// *process* does when the kernel delivers SIGTERM, including the exit status it
// reports to `docker stop`.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include <httplib.h>
#include <nlohmann/json.hpp>

using namespace std::chrono_literals;

namespace {

/// Path to the storage-core binary, passed in by CMake so the test does not
/// have to guess at the build layout.
const char* binary_path() {
    const char* path = std::getenv("STORAGE_CORE_BINARY");
    REQUIRE(path != nullptr);
    return path;
}

std::atomic<int> g_next_port{19500};

/// A real storage-core process, on its own ports and ledger.
struct Server {
    std::filesystem::path dir;
    pid_t pid = -1;
    int http_port = 0;
    int ws_port = 0;
    bool reaped = false;

    explicit Server(const std::string& tag)
        : dir(std::filesystem::temp_directory_path() / ("iot_shutdown_" + tag)) {
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        http_port = g_next_port.fetch_add(2);
        ws_port = http_port + 1;

        const std::string ledger = (dir / "ledger.log").string();
        const std::string bind = "127.0.0.1:" + std::to_string(http_port);
        const std::string ws_bind = "127.0.0.1:" + std::to_string(ws_port);

        pid = fork();
        REQUIRE(pid >= 0);
        if (pid == 0) {
            // Child. Only async-signal-safe work between fork and exec.
            setenv("LEDGER_PATH", ledger.c_str(), 1);
            setenv("BIND_ADDR", bind.c_str(), 1);
            setenv("WS_BIND_ADDR", ws_bind.c_str(), 1);
            setenv("LOG_LEVEL", "info", 1);
            execl(binary_path(), binary_path(), static_cast<char*>(nullptr));
            _exit(127);  // exec failed
        }
        REQUIRE(wait_until_healthy());
    }

    ~Server() {
        if (!reaped && pid > 0) {
            kill(pid, SIGKILL);
            int status = 0;
            waitpid(pid, &status, 0);
        }
        std::filesystem::remove_all(dir);
    }

    bool wait_until_healthy(std::chrono::milliseconds timeout = 10s) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            httplib::Client client("127.0.0.1", http_port);
            client.set_connection_timeout(0, 200000);
            auto res = client.Get("/health");
            if (res && res->status == 200) return true;
            std::this_thread::sleep_for(20ms);
        }
        return false;
    }

    nlohmann::json post_event(const std::string& actor) {
        httplib::Client client("127.0.0.1", http_port);
        client.set_read_timeout(10, 0);
        const std::string body = nlohmann::json{{"event_type", "DOOR_OPEN"},
                                                {"location_id", "loc-1"},
                                                {"actor", actor},
                                                {"description", "d"},
                                                {"metadata", nlohmann::json::object()}}
                                     .dump();
        auto res = client.Post("/events", body, "application/json");
        REQUIRE(res);
        REQUIRE(res->status == 201);
        return nlohmann::json::parse(res->body);
    }

    /// Send a signal and wait for the process to exit. Returns its exit code,
    /// or -1 if it had to be killed because it never stopped.
    int signal_and_wait(int signal_number,
                        std::chrono::milliseconds grace = 10s) {
        kill(pid, signal_number);

        const auto deadline = std::chrono::steady_clock::now() + grace;
        while (std::chrono::steady_clock::now() < deadline) {
            int status = 0;
            const pid_t result = waitpid(pid, &status, WNOHANG);
            if (result == pid) {
                reaped = true;
                if (WIFEXITED(status)) return WEXITSTATUS(status);
                return -2;  // killed by a signal: not a graceful exit
            }
            std::this_thread::sleep_for(10ms);
        }
        return -1;  // still running past the grace period
    }

    std::vector<std::string> ledger_lines() const {
        std::ifstream in(dir / "ledger.log");
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(in, line)) lines.push_back(line);
        return lines;
    }
};

}  // namespace

TEST_CASE("SIGTERM shuts the process down cleanly with exit code 0") {
    Server server("sigterm");

    const auto started = std::chrono::steady_clock::now();
    const int code = server.signal_and_wait(SIGTERM);
    const auto elapsed = std::chrono::steady_clock::now() - started;

    // -1 means it ignored the signal, -2 means the kernel killed it. Both were
    // the behaviour before this ticket.
    CHECK(code == 0);

    // docker stop allows 10s before SIGKILL. Finishing well inside that is the
    // difference between a graceful stop and a killed one in production.
    CHECK(elapsed < 5s);
}

TEST_CASE("SIGINT shuts down the same way, so Ctrl-C matches docker stop") {
    Server server("sigint");
    CHECK(server.signal_and_wait(SIGINT) == 0);
}

TEST_CASE("writes accepted before the signal are on disk afterwards") {
    Server server("drain");

    constexpr int kEvents = 20;
    for (int i = 0; i < kEvents; ++i) server.post_event("a" + std::to_string(i));

    REQUIRE(server.signal_and_wait(SIGTERM) == 0);

    // Genesis is persisted on a fresh boot, so the file holds genesis + events.
    const auto lines = server.ledger_lines();
    CHECK(lines.size() == kEvents + 1);

    // Every block a client was told about must still be there, in order and
    // still linked — a shutdown that truncated the ledger would be worse than
    // one that dropped requests.
    std::string previous_hash;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const auto block = nlohmann::json::parse(lines[i]);
        CHECK(block["index"] == i);
        if (!previous_hash.empty()) CHECK(block["prev_hash"] == previous_hash);
        previous_hash = block["hash"].get<std::string>();
    }
}

TEST_CASE("the ledger survives a shutdown and reloads on the next boot") {
    std::string last_hash;
    std::size_t line_count = 0;

    // Kept OUTSIDE the fixture's directory — that whole tree is removed when the
    // Server goes out of scope, which is exactly what this test needs to outlive.
    const std::string kept =
        (std::filesystem::temp_directory_path() / "iot_shutdown_reload_kept.log")
            .string();
    std::filesystem::remove(kept);

    {
        Server server("reload");
        for (int i = 0; i < 5; ++i) server.post_event("a" + std::to_string(i));
        last_hash = server.post_event("final")["hash"].get<std::string>();
        REQUIRE(server.signal_and_wait(SIGTERM) == 0);

        line_count = server.ledger_lines().size();
        CHECK(line_count == 7);  // genesis + 6

        std::filesystem::copy_file(server.dir / "ledger.log", kept);
    }

    // Restart against the kept ledger and confirm the chain came back whole.
    REQUIRE(std::filesystem::exists(kept));

    const int port = g_next_port.fetch_add(2);
    const pid_t pid = fork();
    REQUIRE(pid >= 0);
    if (pid == 0) {
        setenv("LEDGER_PATH", kept.c_str(), 1);
        setenv("BIND_ADDR", ("127.0.0.1:" + std::to_string(port)).c_str(), 1);
        setenv("WS_BIND_ADDR", ("127.0.0.1:" + std::to_string(port + 1)).c_str(), 1);
        execl(binary_path(), binary_path(), static_cast<char*>(nullptr));
        _exit(127);
    }

    bool healthy = false;
    for (int i = 0; i < 500 && !healthy; ++i) {
        httplib::Client client("127.0.0.1", port);
        client.set_connection_timeout(0, 200000);
        auto res = client.Get("/health");
        healthy = res && res->status == 200;
        if (!healthy) std::this_thread::sleep_for(20ms);
    }
    REQUIRE(healthy);

    httplib::Client client("127.0.0.1", port);
    auto verify = client.Get("/verify");
    REQUIRE(verify);
    const auto body = nlohmann::json::parse(verify->body);
    CHECK(body["valid"] == true);
    CHECK(body["chain_length"] == line_count);

    auto blocks = client.Get("/blocks");
    REQUIRE(blocks);
    const auto all = nlohmann::json::parse(blocks->body)["blocks"];
    CHECK(all.back()["hash"] == last_hash);

    kill(pid, SIGTERM);
    int status = 0;
    waitpid(pid, &status, 0);
    CHECK(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == 0);
    std::filesystem::remove(kept);
}

TEST_CASE("a connected WebSocket client does not prevent shutdown") {
    Server server("ws_client");

    // A live connection means an open socket, a connection thread and a pump
    // thread. If any of them fails to be joined, the process hangs here instead
    // of exiting.
    httplib::Client probe("127.0.0.1", server.http_port);
    server.post_event("before");

    CHECK(server.signal_and_wait(SIGTERM) == 0);
}

TEST_CASE("shutdown is clean with no requests ever served") {
    // The path where the writer thread has nothing queued and no client ever
    // connected — the case most likely to deadlock on an unnecessary join.
    Server server("idle");
    CHECK(server.signal_and_wait(SIGTERM) == 0);
    CHECK(server.ledger_lines().size() == 1);  // genesis only
}
