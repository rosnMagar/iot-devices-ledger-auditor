#include <server.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <future>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <utility>
#include <vector>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <block.hpp>
#include <chain.hpp>
#include <error.hpp>
#include <log.hpp>
#include <storage.hpp>
#include <writer.hpp>

namespace ledger {

namespace {

/// Log every request once it has been handled, with the status it produced.
void install_logger(httplib::Server& srv) {
    srv.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        log::info(req.method + " " + req.path + " -> " +
                  std::to_string(res.status));
    });
}

/// Permissive CORS on every response, so the dashboard can call this directly
/// from a browser. Wide open on purpose — this service is expected to sit
/// behind the backend-api, not on the public internet.
void install_cors(httplib::Server& srv) {
    srv.set_post_routing_handler(
        [](const httplib::Request&, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "Content-Type");
        });

    // Browser preflight: answer OPTIONS on any path with the headers above.
    srv.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });
}

/// Turn anything a handler throws into the JSON error shape from error.hpp.
/// This is the single wrapper IOT-18 was written for.
void install_error_handling(httplib::Server& srv) {
    srv.set_exception_handler(
        [](const httplib::Request&, httplib::Response& res,
           std::exception_ptr ep) {
            ErrorResponse err{500, R"({"error":"internal error"})"};
            try {
                std::rethrow_exception(ep);
            } catch (const std::exception& e) {
                log::error("unhandled exception: " + std::string(e.what()));
                err = to_error_response(e);
            } catch (...) {
                log::error("unhandled non-standard exception");
            }
            res.status = err.status;
            res.set_content(err.body, "application/json");
        });
}

/// Parse a request body into an EventPayload. Every way this can fail — bad
/// JSON, wrong type, a missing field — is the caller's fault, so it all becomes
/// a 400. nlohmann's message names the offending line/column or key, which is
/// useful to a device author and leaks nothing about the server.
EventPayload parse_event(const std::string& body) {
    try {
        return nlohmann::json::parse(body).get<EventPayload>();
    } catch (const nlohmann::json::exception& e) {
        throw ApiError{400, "invalid event payload: " + std::string(e.what())};
    }
}

/// POST /events — append one event to the ledger.
///
/// The handler does no writing itself. It hands the event to the writer thread
/// and blocks on a future for the resulting block, so every append goes through
/// one owner and the chain can never be extended by two threads at once.
///
/// Blocking a handler thread is fine here: cpp-httplib runs one thread per
/// connection. When the queue is full that block *is* the backpressure — the
/// alternative is accepting events faster than the disk can take them.
void install_events(httplib::Server& srv, AppState& state) {
    srv.Post("/events", [&state](const httplib::Request& req,
                                 httplib::Response& res) {
        // Validation stays here and stays first, so bad input never occupies a
        // queue slot behind well-formed events.
        EventPayload event = parse_event(req.body);
        if (event.event_type.empty()) {
            throw ApiError{400, "event_type must not be empty"};
        }

        if (state.write_queue == nullptr) {
            log::error("POST /events with no writer thread attached");
            throw ApiError{500, "writer unavailable"};
        }

        WriteRequest request{std::move(event), std::promise<Block>{}};

        // Take the future BEFORE pushing. Once the request is on the queue the
        // writer may consume, fulfil and destroy it at any moment, so reaching
        // back into `request` afterwards is a use-after-move race.
        std::future<Block> result = request.respond_to.get_future();

        if (!state.write_queue->push(std::move(request))) {
            // Refused, which today only happens once the queue is closed during
            // shutdown. push() did not take ownership, so nothing is left
            // hanging on a promise no one will ever fulfil.
            throw ApiError{500, "server is shutting down"};
        }

        // Re-throws whatever the writer set. A StorageError is deliberately not
        // caught here: to_error_response maps it to a 500 already, and the
        // writer has logged the underlying cause.
        const Block created = result.get();

        res.status = 201;
        res.set_content(nlohmann::json(created).dump(), "application/json");
    });
}

/// Read one range query param as a block index, falling back when it is absent.
///
/// Everything a caller can get wrong here is a 400. The digits are checked by
/// hand first because stoull is too forgiving on its own: it happily accepts
/// "12abc" (returning 12) and wraps "-1" into a huge positive number, either of
/// which would turn a typo into a silently wrong range instead of an error.
std::uint64_t parse_index(const httplib::Request& req, const char* name,
                          std::uint64_t fallback) {
    if (!req.has_param(name)) {
        return fallback;
    }

    const std::string value = req.get_param_value(name);
    if (value.empty() ||
        value.find_first_not_of("0123456789") != std::string::npos) {
        throw ApiError{400,
                       std::string(name) + " must be a non-negative integer"};
    }

    try {
        return std::stoull(value);
    } catch (const std::exception&) {
        // All digits, so the only way left to fail is not fitting in a u64.
        throw ApiError{400, std::string(name) + " is out of range"};
    }
}

/// GET /blocks?from&to — return an inclusive slice of the chain.
///
/// A shared lock, so any number of readers run concurrently; only POST /events
/// takes the exclusive one.
void install_blocks(httplib::Server& srv, AppState& state) {
    srv.Get("/blocks", [&state](const httplib::Request& req,
                                httplib::Response& res) {
        const std::uint64_t from = parse_index(req, "from", 0);
        // Default: as far as the chain goes. Clamped to the real end below,
        // once the length is known under the lock.
        const std::uint64_t to_requested =
            parse_index(req, "to", std::numeric_limits<std::uint64_t>::max());

        std::vector<Block> slice;
        std::size_t chain_length = 0;
        {
            std::shared_lock lock(state.mtx);
            const std::vector<Block>& blocks = state.chain.blocks();
            chain_length = blocks.size();  // never 0 — genesis is always there

            const std::uint64_t last = chain_length - 1;
            // Clamp rather than reject: a caller asking for "everything from
            // here on" shouldn't have to know the length first, and the
            // Lambda's "last 50" walks off the end on a short chain.
            const std::uint64_t to = std::min(to_requested, last);

            // `to` is already <= last, so this one check also catches a `from`
            // past the end of the chain.
            if (from > to) {
                throw ApiError{400, "invalid range: from=" +
                                        std::to_string(from) + " exceeds to=" +
                                        std::to_string(to)};
            }

            // Copy the slice out. The response is serialized after the lock is
            // released, and a concurrent append can reallocate the vector.
            slice.assign(blocks.begin() + static_cast<std::ptrdiff_t>(from),
                         blocks.begin() + static_cast<std::ptrdiff_t>(to) + 1);
        }

        res.set_content(
            nlohmann::json{{"blocks", slice}, {"chain_length", chain_length}}
                .dump(),
            "application/json");
    });
}

/// GET /verify — recompute every hash and check the chain's linkage.
///
/// O(n) in the chain length and done under the shared lock, so a verify on a
/// large ledger delays writers for as long as it runs. Acceptable while the
/// ledger is small; if it grows this wants a snapshot-then-verify split.
void install_verify(httplib::Server& srv, AppState& state) {
    srv.Get("/verify", [&state](const httplib::Request&,
                                httplib::Response& res) {
        VerifyResult result{};
        {
            std::shared_lock lock(state.mtx);
            result = state.chain.verify();
        }

        // first_invalid_index is null on a valid chain — the field is always
        // present so consumers can read it without checking for its existence.
        nlohmann::json body{
            {"valid", result.valid},
            {"chain_length", result.chain_length},
            {"checked_blocks", result.checked_blocks},
            {"first_invalid_index", nullptr},
            {"verified_at", now_rfc3339()},
        };
        if (result.first_invalid_index.has_value()) {
            body["first_invalid_index"] = result.first_invalid_index.value();
        }

        res.set_content(body.dump(), "application/json");
    });
}

}  // namespace

void install_routes(httplib::Server& srv, AppState& state) {
    install_logger(srv);
    install_cors(srv);
    install_error_handling(srv);

    // Liveness only: the process is up and serving. It deliberately takes no
    // lock, so a health check still answers while a write is in flight.
    srv.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(nlohmann::json{{"status", "ok"}}.dump(),
                        "application/json");
    });

    install_events(srv, state);
    install_blocks(srv, state);
    install_verify(srv, state);
}

void ServerHandle::stop() {
    std::lock_guard lock(mtx_);
    stopped_ = true;
    // Null when serve() has not started yet, or has already returned. Recording
    // stopped_ is what makes the first case safe — attach() checks it.
    if (srv_ != nullptr) srv_->stop();
}

bool ServerHandle::attach(httplib::Server* srv) {
    std::lock_guard lock(mtx_);
    if (stopped_) return true;  // stop() beat us here; do not start listening
    srv_ = srv;
    return false;
}

void ServerHandle::detach() {
    std::lock_guard lock(mtx_);
    // Cleared under the lock before the Server is destroyed, so a concurrent
    // stop() either runs fully before this or sees null — never a dangling
    // pointer.
    srv_ = nullptr;
}

bool serve(AppState& state, const Config& config, ServerHandle* handle) {
    httplib::Server srv;

    // httplib defaults to SO_REUSEPORT on Linux, which lets a second instance
    // bind the same port and silently split traffic with the first. Use plain
    // SO_REUSEADDR so a port clash fails loudly instead.
    srv.set_socket_options([](socket_t sock) {
        int on = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    });

    install_routes(srv, state);

    log::info("listening on " + config.bind_host + ":" +
              std::to_string(config.bind_port));

    if (handle != nullptr && handle->attach(&srv)) {
        // Asked to stop before we ever listened — a signal during startup.
        log::info("stop requested before the server started listening");
        return true;
    }

    const bool ok = srv.listen(config.bind_host, config.bind_port);

    // Detach before srv goes out of scope, so a stop() arriving now cannot
    // touch a destroyed Server.
    if (handle != nullptr) handle->detach();

    if (!ok) {
        log::error("failed to bind " + config.bind_host + ":" +
                   std::to_string(config.bind_port));
        return false;
    }
    return true;
}

}  // namespace ledger
