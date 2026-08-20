#include <server.hpp>

#include <exception>
#include <string>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <error.hpp>
#include <log.hpp>

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

    // POST /events (IOT-21), GET /blocks (IOT-22) and GET /verify (IOT-23) are
    // added here; they lock `state.mtx` shared for reads, unique for writes.
    (void)state;

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
