#pragma once

#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0601
#endif

#include "Auth/AuthManager.hpp"
#include "session.h"
#include "ssl_session.h"
#include "transport_types.h"
#include "metrics/rate_limiter.h"
#include <asio.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>


// Forward declarations
namespace mcp {
    namespace protocol {
        struct Request;
        struct Response;
    }// namespace protocol
    namespace metrics{
        class MetricsManager;
        class RateLimiter;
    }
}// namespace mcp

namespace mcp::transport {

    // Forward declarations
    class Session;
    class SslSession;

    /**
     * @brief HTTP request structure for parsing incoming requests.
     */
    struct HttpRequest {
        std::string method;                                  ///< HTTP method (GET, POST, etc.)
        std::string target;                                  ///< Request target/URL
        std::string version;                                 ///< HTTP version
        std::unordered_map<std::string, std::string> headers;///< HTTP headers
        std::string body;                                    ///< Request body
    };

    /**
     * @brief Handles HTTP requests and responses for transport layers.
     * Provides common functionality for both regular and SSL sessions.
     */
    class HttpHandler {
    public:
        /**
         * @brief Construct HTTP handler with message callback.
         * @param on_message Callback for processing complete messages
         */
        explicit HttpHandler(MessageCallback on_message, std::shared_ptr<AuthManagerBase> auth_manager = nullptr);

        /**
         * @brief Set callback to be called before handling a request.
         * @param callback Function to call before request handling
         */
        void set_before_request_callback(std::function<void(const HttpRequest &, const std::string &)> callback);

        /**
         * @brief Set callback to be called after handling a request.
         * @param callback Function to call after request handling
         */
        void set_after_request_callback(std::function<void(const HttpRequest &, const std::string &, int, const std::string &)> callback);

        /**
         * @brief Set callback to be called when an error occurs.
         * @param callback Function to call on error
         */
        void set_on_error_callback(std::function<void(const std::string &, const std::string &)> callback);

        /**
         * @brief Process an HTTP request from a regular session.
         * @param session Active session
         * @param raw_request Raw HTTP request string
         */
        asio::awaitable<void> handle_request(std::shared_ptr<Session> session, const std::string &raw_request);

        /**
         * @brief Process an HTTP request from an SSL session.
         * @param session Active SSL session
         * @param raw_request Raw HTTP request string
         */
        asio::awaitable<void> handle_request(std::shared_ptr<SslSession> session, const std::string &raw_request);

        /**
         * @brief Send an HTTP response to a regular session.
         * @param session Active session
         * @param body Response body
         * @param status_code HTTP status code
         */
        asio::awaitable<void> send_http_response(std::shared_ptr<Session> session, const std::string &body, int status_code = 200);

        /**
         * @brief Send an HTTP response to an SSL session.
         * @param session Active SSL session
         * @param body Response body
         * @param status_code HTTP status code
         */
        asio::awaitable<void> send_http_response(std::shared_ptr<SslSession> session, const std::string &body, int status_code = 200);

        /**
         * @brief Send a chunk of streaming data to a regular session.
         * @param session Active session
         * @param data Data chunk to send
         */
        asio::awaitable<void> send_chunk(std::shared_ptr<Session> session, const std::string &data);

        /**
         * @brief Send a chunk of streaming data to an SSL session.
         * @param session Active SSL session
         * @param data Data chunk to send
         */
        asio::awaitable<void> send_chunk(std::shared_ptr<SslSession> session, const std::string &data);

        /**
         * @brief Send the final chunk to end streaming to a regular session.
         * @param session Active session
         */
        asio::awaitable<void> send_chunk_end(std::shared_ptr<Session> session);

        /**
         * @brief Send the final chunk to end streaming to an SSL session.
         * @param session Active SSL session
         */
        asio::awaitable<void> send_chunk_end(std::shared_ptr<SslSession> session);

        /**
         * @brief Start a streaming response for a regular session.
         * @param session Active session
         * @param content_type Content type for the response
         */
        asio::awaitable<void> start_streaming_response(std::shared_ptr<Session> session, const std::string &content_type = "application/json");

        /**
         * @brief Start a streaming response for an SSL session.
         * @param session Active SSL session
         * @param content_type Content type for the response
         */
        asio::awaitable<void> start_streaming_response(std::shared_ptr<SslSession> session, const std::string &content_type = "application/json");

        /**
         * @brief Send a chunk of streaming data to a regular session.
         * @param session Active session
         * @param chunk Data chunk to send
         */
        asio::awaitable<void> send_streaming_chunk(std::shared_ptr<Session> session, const std::string &chunk);

        /**
         * @brief Send a chunk of streaming data to an SSL session.
         * @param session Active SSL session
         * @param chunk Data chunk to send
         */
        asio::awaitable<void> send_streaming_chunk(std::shared_ptr<SslSession> session, const std::string &chunk);

        /**
         * @brief End a streaming response for a regular session.
         * @param session Active session
         */
        asio::awaitable<void> end_streaming_response(std::shared_ptr<Session> session);

        /**
         * @brief End a streaming response for an SSL session.
         * @param session Active SSL session
         */
        asio::awaitable<void> end_streaming_response(std::shared_ptr<SslSession> session);

    private:
        MessageCallback on_message_;
        std::shared_ptr<AuthManagerBase> auth_manager_;
        std::shared_ptr<mcp::metrics::MetricsManager> metrics_manager_;
        std::shared_ptr<mcp::metrics::RateLimiter> rate_limiter_;

        std::function<void(const HttpRequest &, const std::string &)> before_request_callback_;
        std::function<void(const HttpRequest &, const std::string &, int, const std::string &)> after_request_callback_;
        std::function<void(const std::string &, const std::string &)> on_error_callback_;

        /**
         * @brief Apply flow control policies to an incoming request.
         * @param session Active session
         * @param req HTTP request structure
         * @return true if the request conforms to flow control policies, false otherwise
         */
        bool apply_flow_control(std::shared_ptr<Session> session, const HttpRequest &req);

        /**
         * @brief Apply flow control policies to an incoming SSL request.
         * @param session Active SSL session
         * @param req HTTP request structure
         * @return true if the request conforms to flow control policies, false otherwise
         */
        bool apply_flow_control(std::shared_ptr<SslSession> session, const HttpRequest &req);

        /**
         * @brief Parse raw HTTP request into structured data.
         * @param raw_request Raw HTTP request string
         * @return Optional HttpRequest structure
         */
        std::optional<HttpRequest> parse_request(const std::string &raw_request);

        /**
         * @brief Get header value from headers map (case-insensitive).
         * @param headers Headers map
         * @param key Header key to find
         * @return Header value or empty string
         */
        static std::string get_header_value(const std::unordered_map<std::string, std::string> &headers, const std::string &key);

        /**
         * @brief Get header value from raw headers string (case-insensitive).
         * @param headers_str Raw headers string
         * @param key Header key to find
         * @return Header value or empty string
         */
        static std::string get_header_value(const std::string &headers_str, const std::string &key);

        /**
         * @brief Discard existing buffer data for a regular session.
         * @param session Active session
         */
        asio::awaitable<void> discard_existing_buffer(std::shared_ptr<Session> session);

        /**
         * @brief Discard existing buffer data for an SSL session.
         * @param session Active SSL session
         */
        asio::awaitable<void> discard_existing_buffer(std::shared_ptr<SslSession> session);

        /**
         * @brief Discard remaining request body data for a regular session.
         * @param session Active session
         * @param req HTTP request structure
         * @param already_read Number of bytes already read
         */
        asio::awaitable<void> discard_remaining_request_body(std::shared_ptr<Session> session, const HttpRequest &req, size_t already_read);

        /**
         * @brief Discard remaining request body data for an SSL session.
         * @param session Active SSL session
         * @param req HTTP request structure
         * @param already_read Number of bytes already read
         */
        asio::awaitable<void> discard_remaining_request_body(std::shared_ptr<SslSession> session, const HttpRequest &req, size_t already_read);

        /**
         * @brief Send HTTP response implementation (template method).
         * @param session Active session
         * @param body Response body
         * @param status_code HTTP status code
         * @param is_chunked Whether to use chunked transfer encoding
         */
        template<typename SessionType>
        asio::awaitable<void> send_http_response_impl(std::shared_ptr<SessionType> session, const std::string &body, int status_code, bool is_chunked = false);

        /**
         * @brief Handle request implementation (template method).
         * @param session Active session
         * @param raw_request Raw HTTP request string
         */
        template<typename SessionType>
        asio::awaitable<void> handle_request_impl(std::shared_ptr<SessionType> session, const std::string &raw_request);

        /**
         * @brief Set the maximum allowed request size.
         * @param size Maximum request size in bytes
         */
        void set_max_request_size(size_t size);

        /**
         * @brief Set the maximum number of requests allowed per second.
         * @param count Maximum requests per second
         */
        void set_max_requests_per_second(size_t count);

        /**
         * @brief Start a streaming response implementation (template method).
         * @param session Active session
         * @param content_type Content type for the response
         */
        template<typename SessionType>
        asio::awaitable<void> start_streaming_response_impl(std::shared_ptr<SessionType> session, const std::string &content_type);

        /**
         * @brief Send a streaming chunk implementation (template method).
         * @param session Active session
         * @param chunk Data chunk to send
         */
        template<typename SessionType>
        asio::awaitable<void> send_streaming_chunk_impl(std::shared_ptr<SessionType> session, const std::string &chunk);

        /**
         * @brief End a streaming response implementation (template method).
         * @param session Active session
         */
        template<typename SessionType>
        asio::awaitable<void> end_streaming_response_impl(std::shared_ptr<SessionType> session);

        /**
         * @brief Case-insensitive string comparison.
         * @param s1 First string
         * @param s2 Second string
         * @return Comparison result
         */
        static int strcasecmp(const char *s1, const char *s2);

        /**
         * @brief Send SSE event to client.
         * @param session Active session
         * @param event_type Event type
         * @param event_id Event ID
         * @param data Event data
         */
        template<typename SessionType>
        asio::awaitable<void> send_sse_event(std::shared_ptr<SessionType> session, const std::string &event_type, int event_id, const std::string &data);
    };

}// namespace mcp::transport