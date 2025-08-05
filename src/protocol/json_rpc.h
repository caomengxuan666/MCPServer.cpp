// src/protocol/json_rpc.h
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
        // -32000 to -32099: Reserved for implementation-defined server-errors
    }// namespace error_code

    // JSON-RPC 2.0 Request Object
    // https://www.jsonrpc.org/specification#request_object
    struct Request {
        std::string method;                      // A String containing the name of the method to be invoked
        nlohmann::json params = nlohmann::json{};// A Structured value that holds the parameter values
        std::optional<nlohmann::json> id;        // An identifier established by the Client
    };

    // JSON-RPC 2.0 Response Object
    // https://www.jsonrpc.org/specification#response_object
    struct Response {
        nlohmann::json result;// The result of the method invocation
        nlohmann::json id;    // The identifier established by the Client
    };

    // JSON-RPC 2.0 Error Object
    // https://www.jsonrpc.org/specification#error_object
    struct Error {
        int code;                        // A Number that indicates the error type
        std::string message;             // A String providing a short description of the error
        std::optional<nlohmann::json> id;// The identifier established by the Client
    };

    // Parse a JSON-RPC 2.0 request from text
    // Returns std::nullopt if parsing fails, and automatically sends error responses
    std::optional<Request> parse_request(const std::string &text);

    // Create a JSON-RPC 2.0 response string
    std::string make_response(const Response &resp);

    // Create a JSON-RPC 2.0 error response string
    std::string make_error(const Error &err);
    std::string make_error(int code, const std::string &message, const nlohmann::json &id);
    std::string make_error(int code, const std::string &message);

}// namespace mcp::protocol