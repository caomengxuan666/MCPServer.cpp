// src/protocol/json_rpc.cpp
#include "json_rpc.h"
#include <cstdio>


namespace mcp::protocol {

    // ==================== tool function ====================

    namespace {
        // Helper function: send error response to stdout (temporary, will be handled by transport later)
        void send_error(const std::string &error_json) {
            // Use printf instead of iostream for better performance
            std::printf("%s\n", error_json.c_str());
            std::fflush(stdout);
        }

        // Check jsonrpc field
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
            // JSON parsing failed, return Parse Error
            auto err_json = make_error(error_code::PARSE_ERROR, "Parse error: " + std::string(e.what()),
                                       nlohmann::json(nullptr)// notification id is null
            );
            send_error(err_json);
            return std::nullopt;
        }

        // Check if it's an object
        if (!j.is_object()) {
            send_error(make_error(error_code::INVALID_REQUEST, "Request must be a JSON object"));
            return std::nullopt;
        }

        // Check jsonrpc version
        if (!has_valid_jsonrpc(j)) {
            send_error(make_error(error_code::INVALID_REQUEST, "'jsonrpc' must be '2.0'"));
            return std::nullopt;
        }

        // Check method
        if (!j.contains("method") || !j["method"].is_string()) {
            send_error(make_error(error_code::INVALID_REQUEST, "'method' must be a string"));
            return std::nullopt;
        }

        Request req;
        req.method = j["method"].get<std::string>();
        req.params = j.value("params", nlohmann::json{});

        // Process id
        if (j.contains("id")) {
            const auto &id = j["id"];
            if (id.is_number() || id.is_string() || id.is_null()) {
                req.id = id;
            } else {
                // Invalid id type
                send_error(make_error(error_code::INVALID_REQUEST, "'id' must be number, string, or null"));
                return std::nullopt;
            }
        }
        // If there's no id, req.id remains std::nullopt, indicating a notification

        return req;
    }

    // ==================== make_response implementation ====================

    std::string make_response(const Response &resp) {
        nlohmann::json j;
        j["jsonrpc"] = "2.0";
        j["result"] = resp.result;
        j["id"] = resp.id;// Note: resp.id is a required nlohmann::json
        return j.dump();
    }

    // ==================== make_error implementation ====================

    std::string make_error(const Error &err) {
        nlohmann::json j;
        j["jsonrpc"] = "2.0";

        nlohmann::json error_obj;
        error_obj["code"] = err.code;
        error_obj["message"] = err.message;
        j["error"] = error_obj;

        if (err.id.has_value()) {
            j["id"] = err.id.value();
        } else {
            j["id"] = nullptr;// Error response for notification, id is null
        }

        return j.dump();
    }

    std::string make_error(int code, const std::string &message, const nlohmann::json &id) {
        return make_error(Error{code, message, std::optional<nlohmann::json>{id}});
    }

    std::string make_error(int code, const std::string &message) {
        return make_error(Error{code, message, std::nullopt});
    }
}// namespace mcp::protocol