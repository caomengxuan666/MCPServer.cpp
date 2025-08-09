#pragma once

#include "session.h"
#include "ssl_session.h"
#include <asio.hpp>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <memory>

namespace mcp::transport {

    // Forward declaration
    class Session;
    class SslSession;

    class HttpHandler {
    public:
        // Define HTTP request structure
        struct HttpRequest {
            std::string method;                                  // Request method (GET/POST/etc)
            std::string target;                                  // Request path
            std::string version;                                 // HTTP version
            std::unordered_map<std::string, std::string> headers;// Header key-value pairs
            std::string body;                                    // Request body
        };

        // Message callback function type
        using MessageCallback = std::function<void(
                const std::string &json_message,
                std::shared_ptr<Session> session,
                const std::string &session_id)>;

        // Constructor
        explicit HttpHandler(MessageCallback on_message)
            : on_message_(std::move(on_message)) {}

        // Parse HTTP request
        std::optional<HttpRequest> parse_request(const std::string &raw_request);

        // Main request handling logic
        asio::awaitable<void> handle_request(
                std::shared_ptr<Session> session,
                const std::string &raw_request);
                
        // Main request handling logic for SSL sessions
        asio::awaitable<void> handle_request(
                std::shared_ptr<SslSession> session,
                const std::string &raw_request);

        // Send HTTP response
        asio::awaitable<void> send_http_response(
                std::shared_ptr<Session> session,
                const std::string &body,
                int status_code);
                
        // Send HTTPS response
        asio::awaitable<void> send_http_response(
                std::shared_ptr<SslSession> session,
                const std::string &body,
                int status_code);


        // Get value from headers map (handling unordered_map type headers)
        static std::string get_header_value(
                const std::unordered_map<std::string, std::string> &headers,
                const std::string &key);

        // Get value from raw headers string (compatibility with old logic)
        static std::string get_header_value(
                const std::string &headers_str,
                const std::string &key);

    private:
        MessageCallback on_message_;// Message callback function


        asio::awaitable<void> discard_existing_buffer(std::shared_ptr<Session> session);
        asio::awaitable<void> discard_existing_buffer(std::shared_ptr<SslSession> session);

        // Fully read and discard remaining request body
        asio::awaitable<void> discard_remaining_request_body(
                std::shared_ptr<Session> session,
                const HttpRequest &req,
                size_t already_read);
                
        // Fully read and discard remaining request body for SSL sessions
        asio::awaitable<void> discard_remaining_request_body(
                std::shared_ptr<SslSession> session,
                const HttpRequest &req,
                size_t already_read);

        // Case-insensitive string comparison (internal helper function)
        static int strcasecmp(const char *s1, const char *s2);
    };

}// namespace mcp::transport