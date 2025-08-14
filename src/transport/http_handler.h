#pragma once

#include "session.h"
#include "ssl_session.h"
#include <asio.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include "Auth/AuthManager.hpp"
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

        // AOP callback function types
        using BeforeRequestCallback = std::function<void(
                const HttpRequest &request,
                const std::string &session_id)>;

        using AfterRequestCallback = std::function<void(
                const HttpRequest &request,
                const std::string &response,
                int status_code,
                const std::string &session_id)>;

        using OnErrorCallback = std::function<void(
                const std::string &error_message,
                const std::string &session_id)>;

        // Constructor
        explicit HttpHandler(
                MessageCallback on_message,
                std::shared_ptr<AuthManagerBase> auth_manager = nullptr)
            : on_message_(std::move(on_message)), auth_manager_(std::move(auth_manager)) {}

        // AOP callback setters
        void set_before_request_callback(BeforeRequestCallback callback) {
            before_request_callback_ = std::move(callback);
        }

        void set_after_request_callback(AfterRequestCallback callback) {
            after_request_callback_ = std::move(callback);
        }

        void set_on_error_callback(OnErrorCallback callback) {
            on_error_callback_ = std::move(callback);
        }

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
        MessageCallback on_message_;                   // Message callback function
        BeforeRequestCallback before_request_callback_;// AOP: Before request callback
        AfterRequestCallback after_request_callback_;  // AOP: After request callback
        OnErrorCallback on_error_callback_;            // AOP: Error callback
        std::shared_ptr<AuthManagerBase> auth_manager_;

        // Template function to handle common logic for both Session and SslSession
        template<typename SessionType>
        asio::awaitable<void> handle_request_impl(
                std::shared_ptr<SessionType> session,
                const std::string &raw_request);

        asio::awaitable<void> discard_existing_buffer(std::shared_ptr<Session> session);
        asio::awaitable<void> discard_existing_buffer(std::shared_ptr<SslSession> session);

        // Template function to send HTTP response for both Session and SslSession
        template<typename SessionType>
        asio::awaitable<void> send_http_response_impl(
                std::shared_ptr<SessionType> session,
                const std::string &body,
                int status_code);

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