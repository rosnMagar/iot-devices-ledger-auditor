#include <server.hpp>

#include <exception>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <utility>
#include <vector>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <block.hpp>
#include <error.hpp>
#include <log.hpp>
#include <storage.hpp>

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
/// The whole append is one critical section: the block must land in the chain
/// and on disk before any other request can observe either, or a concurrent
/// write would chain onto a block that is not yet persisted.
void install_events(httplib::Server& srv, AppState& state) {
    srv.Post("/events", [&state](const httplib::Request& req,
                                 httplib::Response& res) {
        EventPayload event = parse_event(req.body);
        if (event.event_type.empty()) {
            throw ApiError{400, "event_type must not be empty"};
        }

        Block created{};
        {
            std::unique_lock lock(state.mtx);
            // append() returns a reference into the chain's vector; copy it out
            // so the response does not depend on the lock still being held.
            created = state.chain.append(std::move(event));

            try {
                append_block(state.log, created);
            } catch (const StorageError& e) {
                // The block is in memory but not on disk. Drop it — otherwise
                // every later block would chain onto something a restart will
                // never see, and the ledger would fail verification on reload.
                std::vector<Block> kept = state.chain.blocks();
                kept.pop_back();
                state.chain = Chain::load(std::move(kept));

                log::error("failed to persist block: " + std::string(e.what()));
                throw ApiError{500, "failed to persist event"};
            }
        }

        res.status = 201;
        res.set_content(nlohmann::json(created).dump(), "application/json");
    });
}

}  // namespace

bool serve(AppState& state, const Config& config) {
    httplib::Server srv;

    // httplib defaults to SO_REUSEPORT on Linux, which lets a second instance
    // bind the same port and silently split traffic with the first. Use plain
    // SO_REUSEADDR so a port clash fails loudly instead.
    srv.set_socket_options([](socket_t sock) {
        int on = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    });

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

    // GET /blocks (IOT-22) and GET /verify (IOT-23) are added here; they take a
    // shared lock on `state.mtx`, where the write above takes a unique one.

    log::info("listening on " + config.bind_host + ":" +
              std::to_string(config.bind_port));

    if (!srv.listen(config.bind_host, config.bind_port)) {
        log::error("failed to bind " + config.bind_host + ":" +
                   std::to_string(config.bind_port));
        return false;
    }
    return true;
}

}  // namespace ledger
