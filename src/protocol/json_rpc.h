#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>

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
    };


    // JSON-RPC 2.0 Error Object
    // https://www.jsonrpc.org/specification#error_object
    struct Error {
        int code;                          // A Number that indicates the error type
        std::string message;               // A String providing a short description of the error
        std::optional<nlohmann::json> data;// Optional: Additional error information
        std::optional<nlohmann::json> id;  // The identifier established by the Client (or null for notifications)
    };

    // JSON-RPC 2.0 Response Object
    // https://www.jsonrpc.org/specification#response_object

    struct Response {
        nlohmann::json result;     // The result of the method invocation
        nlohmann::json id;         // The identifier established by the Client (must match request id)
        std::optional<Error> error;// The error object
    };

    // Parse a JSON-RPC 2.0 request from text
    // Returns std::nullopt if parsing fails, and automatically sends error responses
    std::optional<Request> parse_request(const std::string &text);

    /**
    * @brief Serializes a Response object into a JSON-RPC 2.0 compliant JSON string.
    *
    * This function converts a C++ Response struct into a properly formatted
    * JSON string according to the JSON-RPC 2.0 specification. It ensures that
    * either the 'result' or 'error' field is present, but never both, and always
    * includes the 'jsonrpc' version identifier.
    *
    * @param resp The Response object to serialize. It may contain either a result,
    *             an error, or neither (in the case of a notification).
    * @return A JSON string representing the Response in JSON-RPC 2.0 format.
    *         Example success: {"jsonrpc":"2.0","result":{...},"id":"req_123"}
    *         Example error:   {"jsonrpc":"2.0","error":{"code":-32000,"message":"File not found"},"id":"req_123"}
    *         Example notification: {"jsonrpc":"2.0","id":null}
    */
    std::string make_response(const Response &resp);

    // Create a JSON-RPC 2.0 error response string
    std::string make_error(const Error &err);
    std::string make_error(int code, const std::string &message, const nlohmann::json &id,
                           const std::optional<nlohmann::json> &data = std::nullopt);
    std::string make_error(int code, const std::string &message,
                           const std::optional<nlohmann::json> &data = std::nullopt);

    std::string make_notification(const std::string &method, const nlohmann::json &params = nlohmann::json{});

}// namespace mcp::protocol