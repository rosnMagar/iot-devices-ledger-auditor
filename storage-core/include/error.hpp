#pragma once
#include <exception>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace ledger {

/// An error meant for the API consumer: an HTTP status plus a *public* message
/// that is safe to send. Handlers throw this; the server turns it into JSON.
struct ApiError : std::exception {
    int status;
    std::string message;

    ApiError(int status_code, std::string public_message)
        : status(status_code), message(std::move(public_message)) {}

    const char* what() const noexcept override { return message.c_str(); }
};

/// An HTTP status plus the JSON body to send. Kept httplib-free so the mapping
/// below can be tested without starting a server.
struct ErrorResponse {
    int status;
    std::string body;  // serialized {"error": "..."}
};

/// An ApiError keeps its own status and message; anything else collapses to a
/// generic 500 so no internal detail (paths, library text) reaches the caller.
inline ErrorResponse to_error_response(const std::exception& e) {
    if (const auto* api = dynamic_cast<const ApiError*>(&e)) {
        return {api->status, nlohmann::json{{"error", api->message}}.dump()};
    }
    return {500, nlohmann::json{{"error", "internal error"}}.dump()};
}

}  // namespace ledger
