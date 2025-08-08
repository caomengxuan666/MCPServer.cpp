#include "json_rpc.h"
#include <cstdio>

namespace mcp::protocol {

    // ==================== Helper Functions ====================
    namespace {
        // Helper function: send error response to stdout (temporary, will be handled by transport later)
        void send_error(const std::string &error_json) {
            // Use printf instead of iostream for better performance
            std::printf("%s\n", error_json.c_str());
            std::fflush(stdout);
        }

        // Check if jsonrpc field is valid ("2.0")
        bool has_valid_jsonrpc(const nlohmann::json &j) {
            if (!j.contains("jsonrpc")) {
                return false;
            }
            return j["jsonrpc"].get<std::string>() == "2.0";
        }
    }// namespace

    // ==================== parse_request implementation ====================
    std::optional<Request> parse_request(const std::string &text) {
        nlohmann::json j;
        try {
            j = nlohmann::json::parse(text);
        } catch (const nlohmann::json::parse_error &e) {
            // JSON parsing failed (PARSE_ERROR)
            auto err_json = make_error(
                    error_code::PARSE_ERROR,
                    "Parse error: " + std::string(e.what()),
                    nlohmann::json(nullptr),           // Notification id is null
                    nlohmann::json{{"details", e.byte}}// Add parse error position as data
            );
            send_error(err_json);
            return std::nullopt;
        }

        // Check if request is a JSON object
        if (!j.is_object()) {
            send_error(make_error(
                    error_code::INVALID_REQUEST,
                    "Request must be a JSON object",
                    nlohmann::json(nullptr)));
            return std::nullopt;
        }

        // Check jsonrpc version
        if (!has_valid_jsonrpc(j)) {
            send_error(make_error(
                    error_code::INVALID_REQUEST,
                    "'jsonrpc' must be '2.0'",
                    nlohmann::json(nullptr)));
            return std::nullopt;
        }

        // Check method field (must be a string)
        if (!j.contains("method") || !j["method"].is_string()) {
            send_error(make_error(
                    error_code::INVALID_REQUEST,
                    "'method' must be a string",
                    j.contains("id") ? j["id"] : nlohmann::json(nullptr)));
            return std::nullopt;
        }

        // Build request object
        Request req;
        req.method = j["method"].get<std::string>();
        req.params = j.value("params", nlohmann::json{});// Optional params

        // Process request id (if present)
        if (j.contains("id")) {
            const auto &id = j["id"];
            if (id.is_number() || id.is_string() || id.is_null()) {
                req.id = id;
            } else {
                // Invalid id type
                send_error(make_error(
                        error_code::INVALID_REQUEST,
                        "'id' must be number, string, or null",
                        nlohmann::json(nullptr),
                        nlohmann::json{{"received_type", id.type_name()}}));
                return std::nullopt;
            }
        }
        // If no id, req.id remains std::nullopt (notification message)

        return req;
    }

    // ==================== make_response implementation ====================
    std::string make_response(const Response &resp) {
        // Create a JSON object to hold the response
        nlohmann::json j;

        // Always include the JSON-RPC version
        j["jsonrpc"] = "2.0";

        // Check if the response contains an error
        if (resp.error.has_value()) {
            // Add the error object to the JSON response
            j["error"] = {
                    {"code", resp.error->code},     // The error code (e.g., -32601 for method not found)
                    {"message", resp.error->message}// A human-readable error message
            };

            // If additional error data is provided, include it in the response
            if (resp.error->data.has_value()) {
                j["error"]["data"] = resp.error->data.value();
            }

            // Set the 'id' field from the response. This can be any JSON value (string, number, null).
            j["id"] = resp.id;
        }
        // Check if the response contains a result (and no error)
        else if (!resp.result.is_null()) {
            // Add the result data to the JSON response
            j["result"] = resp.result;

            // Set the 'id' field from the response for correlation with the request
            j["id"] = resp.id;
        } else {
            // This branch handles the case of a Notification.
            // In JSON-RPC, notifications have no 'result' or 'error' and their 'id' is null.
            j["id"] = nullptr;
        }

        // Serialize the JSON object to a formatted string and return it
        return j.dump();
    }

    // ==================== make_error implementation ====================
    std::string make_error(const Error &err) {
        nlohmann::json j;
        j["jsonrpc"] = "2.0";// Mandatory protocol version

        // Build error object
        nlohmann::json error_obj;
        error_obj["code"] = err.code;      // Error code
        error_obj["message"] = err.message;// Error message
        if (err.data.has_value()) {        // Optional error data
            error_obj["data"] = err.data.value();
        }
        j["error"] = error_obj;

        // Set id (must match request id, or null for notifications)
        j["id"] = err.id.has_value() ? err.id.value() : nullptr;

        return j.dump();
    }

    std::string make_error(int code, const std::string &message, const nlohmann::json &id,
                           const std::optional<nlohmann::json> &data) {
        return make_error(Error{code, message, data, std::optional<nlohmann::json>{id}});
    }

    std::string make_error(int code, const std::string &message, const std::optional<nlohmann::json> &data) {
        return make_error(Error{code, message, data, std::nullopt});
    }

    std::string make_notification(const std::string &method, const nlohmann::json &params) {
        nlohmann::json j;
        j["jsonrpc"] = "2.0";
        j["method"] = method;
        if (!params.is_null() && !params.empty()) {
            j["params"] = params;
        }
        return j.dump();
    }

}// namespace mcp::protocol