#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <utility>

namespace mcp::protocol {

    // JSON-RPC 2.0 error codes
    namespace error_code {
        constexpr int PARSE_ERROR = -32700;     // Invalid JSON was received by the server
        constexpr int INVALID_REQUEST = -32600; // The JSON sent is not a valid Request object
        constexpr int METHOD_NOT_FOUND = -32601;// The method does not exist / is not available
        constexpr int INVALID_PARAMS = -32602;  // Invalid method parameter(s)
        constexpr int INTERNAL_ERROR = -32603;  // Internal JSON-RPC error

        // MCP extension error codes
        constexpr int TOOL_NOT_FOUND = -32000;    // Tool not found
        constexpr int RESOURCE_NOT_FOUND = -32001;// Resource not found
        constexpr int PERMISSION_DENIED = -32002; // Permission denied
        constexpr int RATE_LIMITED = -32003;      // Rate limited
        constexpr int TIMEOUT = -32004;           // Timeout
        constexpr int INVALID_TOOL_INPUT = -32005;// Invalid tool input
    }// namespace error_code

    // JSON-RPC 2.0 Request Object
    // https://www.jsonrpc.org/specification#request_object
    struct Request {
        std::string method;                      // A String containing the name of the method to be invoked
        nlohmann::json params = nlohmann::json{};// A Structured value that holds the parameter values
        std::optional<nlohmann::json> id;        // An identifier established by the Client (optional for notifications)

        // Constructors for easier initialization
        Request() = default;
        explicit Request(std::string method) : method(std::move(method)) {}
        Request(std::string method, nlohmann::json params) : method(std::move(method)), params(std::move(params)) {}
        Request(std::string method, nlohmann::json params, std::optional<nlohmann::json> id)
            : method(std::move(method)), params(std::move(params)), id(std::move(id)) {}
    };

    // JSON-RPC 2.0 Error Object
    // https://www.jsonrpc.org/specification#error_object
    struct Error {
        int code;                          // A Number that indicates the error type
        std::string message;               // A String providing a short description of the error
        std::optional<nlohmann::json> data;// Optional: Additional error information
        std::optional<nlohmann::json> id;  // The identifier established by the Client (or null for notifications)

        // Constructors with overloads for different parameter combinations
        Error(int code, std::string message) : code(code), message(std::move(message)) {}
        Error(int code, std::string message, std::optional<nlohmann::json> data)
            : code(code), message(std::move(message)), data(std::move(data)) {}
        Error(int code, std::string message, std::optional<nlohmann::json> data, std::optional<nlohmann::json> id)
            : code(code), message(std::move(message)), data(std::move(data)), id(std::move(id)) {}
    };

    // JSON-RPC 2.0 Response Object
    // https://www.jsonrpc.org/specification#response_object
    struct Response {
        nlohmann::json result = nlohmann::json{};// The result of the method invocation
        nlohmann::json id;                       // The identifier established by the Client (must match request id)
        std::optional<Error> error;              // The error object

        // Constructors for success and error responses

        Response() : result(nlohmann::json(nullptr)), id(nlohmann::json(nullptr)) {}

        Response(nlohmann::json result, nlohmann::json id) : result(std::move(result)), id(std::move(id)) {}
        Response(Error error_, nlohmann::json id_)
            : id(std::move(id_)), error(std::move(error_)) {}

        // Check if response is valid (can't have both result and error)
        bool is_valid() const {
            return !error.has_value() || result.is_null();
        }
    };

    /**
     * @brief Parses a JSON-RPC 2.0 request from text
     * @return Pair containing:
     *         - std::optional<Request>: Valid request if parsing succeeded
     *         - std::optional<Error>: Error if parsing failed (nullopt otherwise)
     */
    std::pair<std::optional<Request>, std::optional<Error>> parse_request(const std::string &text);

    /**
     * @brief Serializes a Response object into a JSON-RPC 2.0 compliant JSON string
     */
    std::string make_response(const Response &resp);

    // Overloaded make_response for common cases
    std::string make_response(const nlohmann::json &result, const nlohmann::json &id);
    std::string make_response(const Error &error, const nlohmann::json &id);

    /**
     * @brief Creates JSON-RPC 2.0 error response strings (various overloads)
     */
    std::string make_error(const Error &err);
    std::string make_error(int code, const std::string &message);
    std::string make_error(int code, const std::string &message, const std::optional<nlohmann::json> &data);
    std::string make_error(int code, const std::string &message, const nlohmann::json &id);
    std::string make_error(int code, const std::string &message, const nlohmann::json &id,
                           const std::optional<nlohmann::json> &data);

    /**
     * @brief Creates JSON-RPC 2.0 notification messages
     */
    std::string make_notification(const std::string &method);
    std::string make_notification(const std::string &method, const nlohmann::json &params);

    /**
     * @brief Helper to extract ID from request (returns nullopt for notifications)
     */
    nlohmann::json get_request_id(const Request &req);

    /** 
    * @brief For tools to pass the tool output to to transport
    * @note this function does not pass the version,only pass the result
    */
    std::string generate_result(const nlohmann::json &result);

    std::string generate_result(const nlohmann::json &result, const nlohmann::json &id);

    std::string generate_error(const Error &error);
    std::string generate_error(int code, const std::string &message);
    std::string generate_error(int code, const std::string &message, const std::optional<nlohmann::json> &data);
    std::string generate_error(int code, const std::string &message, const nlohmann::json &id);
    std::string generate_error(int code, const std::string &message, const nlohmann::json &id,
                               const std::optional<nlohmann::json> &data);

}// namespace mcp::protocol
