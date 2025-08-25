#include "json_rpc.h"
#include <string>

namespace mcp::protocol {

    // ==================== Helper Functions ====================
    namespace {
        // Check if jsonrpc field is valid ("2.0")
        bool has_valid_jsonrpc(const nlohmann::json &j) {
            return j.contains("jsonrpc") && j["jsonrpc"].is_string() &&
                   j["jsonrpc"].get<std::string>() == "2.0";
        }
    }// namespace

    // ==================== parse_request implementation ====================
    std::pair<std::optional<Request>, std::optional<Error>> parse_request(const std::string &text) {
        nlohmann::json j;
        try {
            j = nlohmann::json::parse(text);
        } catch (const nlohmann::json::parse_error &e) {
            // JSON parsing failed (PARSE_ERROR)
            return {
                    std::nullopt,
                    Error{
                            error_code::PARSE_ERROR,
                            "Parse error: " + std::string(e.what()),
                            std::make_optional(nlohmann::json{{"details", e.byte}}),
                            std::make_optional(nlohmann::json(nullptr))}};
        }

        // Check if request is a JSON object
        if (!j.is_object()) {
            return {
                    std::nullopt,
                    Error{
                            error_code::INVALID_REQUEST,
                            "Request must be a JSON object",
                            std::nullopt,
                            std::make_optional(nlohmann::json(nullptr))}};
        }

        // Check jsonrpc version
        if (!has_valid_jsonrpc(j)) {
            return {
                    std::nullopt,
                    Error{
                            error_code::INVALID_REQUEST,
                            "'jsonrpc' must be '2.0'",
                            std::nullopt,
                            j.contains("id") ? std::make_optional(j["id"]) : std::make_optional(nlohmann::json(nullptr))}};
        }

        // Check method field (must be a string)
        if (!j.contains("method") || !j["method"].is_string()) {
            return {
                    std::nullopt,
                    Error{
                            error_code::INVALID_REQUEST,
                            "'method' must be a string",
                            std::nullopt,
                            j.contains("id") ? std::make_optional(j["id"]) : std::make_optional(nlohmann::json(nullptr))}};
        }

        // Process request id (if present)
        std::optional<nlohmann::json> req_id;
        if (j.contains("id")) {
            const auto &id = j["id"];
            if (id.is_number() || id.is_string() || id.is_null()) {
                req_id = std::make_optional(id);
            } else {
                // Invalid id type
                return {
                        std::nullopt,
                        Error{
                                error_code::INVALID_REQUEST,
                                "'id' must be number, string, or null",
                                std::make_optional(nlohmann::json{{"received_type", id.type_name()}}),
                                std::make_optional(nlohmann::json(nullptr))}};
            }
        }

        // Build and return valid request
        Request req(
                j["method"].get<std::string>(),
                j.value("params", nlohmann::json{}),
                req_id);
        return {req, std::nullopt};
    }

    // ==================== make_response implementations ====================
    std::string make_response(const Response &resp) {
        if (!resp.is_valid()) {
            return make_error(
                    error_code::INTERNAL_ERROR,
                    "Invalid response: contains both result and error",
                    resp.id);
        }

        nlohmann::json j;
        j["jsonrpc"] = "2.0";
        j["id"] = resp.id;

        if (resp.error.has_value()) {
            // Add error object
            j["error"] = nlohmann::json{
                    {"code", resp.error->code},
                    {"message", resp.error->message}};
            if (resp.error->data.has_value()) {
                j["error"]["data"] = resp.error->data.value();
            }
        } else if (!resp.result.is_null()) {
            // Add result object
            j["result"] = resp.result;
        }

        return j.dump();
    }

    std::string make_response(const nlohmann::json &result, const nlohmann::json &id) {
        return make_response(Response{result, id});
    }

    std::string make_response(const Error &error, const nlohmann::json &id) {
        return make_response(Response{error, id});
    }

    // ==================== make_error implementations ====================
    std::string make_error(const Error &err) {
        nlohmann::json j;
        j["jsonrpc"] = "2.0";

        // Build error object
        nlohmann::json error_obj{{"code", err.code}, {"message", err.message}};
        if (err.data.has_value()) {
            error_obj["data"] = err.data.value();
        }
        j["error"] = error_obj;

        // Set id (use null if not provided)
        j["id"] = err.id.has_value() ? err.id.value() : nlohmann::json(nullptr);

        return j.dump();
    }

    std::string make_error(int code, const std::string &message) {
        return make_error(Error{code, message});
    }

    std::string make_error(int code, const std::string &message, const std::optional<nlohmann::json> &data) {
        return make_error(Error{code, message, data});
    }

    std::string make_error(int code, const std::string &message, const nlohmann::json &id) {
        return make_error(Error{code, message, std::nullopt, std::make_optional(id)});
    }

    std::string make_error(int code, const std::string &message, const nlohmann::json &id,
                           const std::optional<nlohmann::json> &data) {
        return make_error(Error{code, message, data, std::make_optional(id)});
    }

    // ==================== make_notification implementations ====================
    std::string make_notification(const std::string &method) {
        return make_notification(method, nlohmann::json{});
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

    // ==================== Helper function implementations ====================
    nlohmann::json get_request_id(const Request &req) {
        return req.id.has_value() ? req.id.value() : nlohmann::json(nullptr);
    }

    // ===================== For Plugin Tools ================================
    std::string generate_result(const nlohmann::json &result) {
        nlohmann::json j;
        j["result"] = result;
        return j.dump();
    }

    std::string generate_result(const nlohmann::json &result, const nlohmann::json &id) {
        nlohmann::json j;
        j["id"] = id;
        j["result"] = result;
        return j.dump();
    }

    std::string generate_error(const Error &error) {
        nlohmann::json j;
        j["error"] = nlohmann::json{
                {"code", error.code},
                {"message", error.message}};
        if (error.data.has_value()) {
            j["error"]["data"] = error.data.value();
        }
        return j.dump();
    }

    std::string generate_error(int code, const std::string &message) {
        return generate_error(Error{code, message});
    }

    std::string generate_error(int code, const std::string &message, const std::optional<nlohmann::json> &data) {
        return generate_error(Error{code, message, data});
    }

    std::string generate_error(int code, const std::string &message, const nlohmann::json &id) {
        nlohmann::json j;
        j["id"] = id;
        j["error"] = nlohmann::json{
                {"code", code},
                {"message", message}};
        return j.dump();
    }

    std::string generate_error(int code, const std::string &message, const nlohmann::json &id,
                               const std::optional<nlohmann::json> &data) {
        nlohmann::json j;
        j["id"] = id;
        j["error"] = nlohmann::json{
                {"code", code},
                {"message", message}};
        if (data.has_value()) {
            j["error"]["data"] = data.value();
        }
        return j.dump();
    }

}// namespace mcp::protocol
