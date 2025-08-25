#include "http_handler.h"
#include "core/logger.h"
#include "metrics/metrics_manager.h"
#include "metrics/performance_metrics.h"
#include "metrics/rate_limiter.h"
#include "nlohmann/json.hpp"
#include "session.h"
#include "ssl_session.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>


using asio::awaitable;
using asio::use_awaitable;

namespace mcp::transport {

    HttpHandler::HttpHandler(MessageCallback on_message, std::shared_ptr<AuthManagerBase> auth_manager)
        : on_message_(std::move(on_message)), auth_manager_(std::move(auth_manager)) {
        metrics_manager_ = mcp::metrics::MetricsManager::getInstance();
        rate_limiter_ = mcp::metrics::RateLimiter::getInstance();
    }

    // Helper function: get value from unordered_map headers
    std::string HttpHandler::get_header_value(
            const std::unordered_map<std::string, std::string> &headers,
            const std::string &key) {
        auto it = headers.find(key);
        return (it != headers.end()) ? it->second : "";
    }

    // Helper function: get value from raw string headers
    std::string HttpHandler::get_header_value(
            const std::string &headers_str,
            const std::string &key) {
        std::istringstream hss(headers_str);
        std::string line;
        while (std::getline(hss, line)) {
            auto pos = line.find(':');
            if (pos != std::string::npos) {
                std::string header_name = line.substr(0, pos);
                if (strcasecmp(header_name.c_str(), key.c_str()) == 0) {
                    std::string value = line.substr(pos + 1);
                    value.erase(0, value.find_first_not_of(" \t\r\n"));
                    value.erase(value.find_last_not_of(" \t\r\n") + 1);
                    return value;
                }
            }
        }
        return "";
    }

    // Parse HTTP request (unique implementation, remove duplicate definitions)
    std::optional<HttpRequest> HttpHandler::parse_request(const std::string &raw_request) {
        HttpRequest req;
        std::istringstream iss(raw_request);
        std::string line;

        // Parse request line (method, target, version)
        if (!std::getline(iss, line)) {
            return std::nullopt;
        }
        // Remove trailing \r character
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        std::istringstream request_line(line);
        request_line >> req.method >> req.target >> req.version;
        if (request_line.fail()) {
            return std::nullopt;
        }

        // Parse headers
        while (std::getline(iss, line)) {
            // Remove trailing \r character
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            // Headers end marker (empty line)
            if (line.empty()) {
                break;
            }
            size_t colon_pos = line.find(':');
            if (colon_pos == std::string::npos) {
                continue;// Invalid header format, skip
            }
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            // Trim whitespace from value
            size_t start = value.find_first_not_of(" ");
            size_t end = value.find_last_not_of(" ");
            if (start != std::string::npos && end != std::string::npos) {
                req.headers[key] = value.substr(start, end - start + 1);
            }
        }

        // Parse body (based on Content-Length)
        if (req.headers.count("Content-Length")) {
            try {
                size_t content_len = std::stoull(req.headers["Content-Length"]);
                // Read remaining request body data
                char *body_buf = new char[content_len + 1];
                iss.read(body_buf, content_len);
                body_buf[content_len] = '\0';
                req.body = std::string(body_buf, content_len);
                delete[] body_buf;
            } catch (...) {
                return std::nullopt;// Parsing failed
            }
        }

        return req;
    }

    // Clean existing buffer
    asio::awaitable<void> HttpHandler::discard_existing_buffer(std::shared_ptr<Session> session) {
        // Check if there is pending data to send, discard if exists
        if (session->get_socket().available()) {
            std::array<char, 4096> buffer;
            while (session->get_socket().available()) {
                // Asynchronously read and discard data
                co_await session->get_socket().async_read_some(
                        asio::buffer(buffer),
                        use_awaitable);
            }
        }
        co_return;
    }

    // Clean existing buffer for SSL sessions
    asio::awaitable<void> HttpHandler::discard_existing_buffer(std::shared_ptr<SslSession> session) {
        // Check if there is pending data to send, discard if exists
        asio::error_code ec;
        if (session->get_stream().lowest_layer().available(ec) && !ec) {
            std::array<char, 4096> buffer;
            while (session->get_stream().lowest_layer().available(ec) && !ec) {
                // Asynchronously read and discard data
                co_await session->get_stream().async_read_some(
                        asio::buffer(buffer),
                        use_awaitable);
            }
        }
        co_return;
    }

    /**
    * @brief Sends an HTTP response to the client over the specified session.
    * 
    * This function constructs a valid HTTP response with appropriate headers,
    * handles connection persistence based on client hints, and manages the 
    * underlying TCP connection lifecycle. Strictly follows MCP protocol requirements.
    * 
    * @param session Shared pointer to the active Session object
    * @param body The response body content (typically JSON for requests, empty for notifications)
    * @param status_code HTTP status code (e.g., 200 for OK, 202 for Accepted notifications)
    * @return asio::awaitable<void> Awaitable task for async operation
    */
    asio::awaitable<void> HttpHandler::send_http_response(
            std::shared_ptr<Session> session,
            const std::string &body,
            int status_code) {
        return send_http_response_impl(session, body, status_code, false);
    }

    /**
    * @brief Sends an HTTPS response to the client over the specified SSL session.
    * 
    * This function constructs a valid HTTP response with appropriate headers,
    * handles connection persistence based on client hints, and manages the 
    * underlying TCP connection lifecycle. Strictly follows MCP protocol requirements.
    * 
    * @param session Shared pointer to the active SslSession object
    * @param body The response body content (typically JSON for requests, empty for notifications)
    * @param status_code HTTP status code (e.g., 200 for OK, 202 for Accepted notifications)
    * @return asio::awaitable<void> Awaitable task for async operation
    */
    asio::awaitable<void> HttpHandler::send_http_response(
            std::shared_ptr<SslSession> session,
            const std::string &body,
            int status_code) {
        return send_http_response_impl(session, body, status_code, false);
    }

    // Explicit instantiation of send_http_response_impl for both session types
    template asio::awaitable<void> HttpHandler::send_http_response_impl(
            std::shared_ptr<Session> session,
            const std::string &body,
            int status_code,
            bool is_chunked);

    template asio::awaitable<void> HttpHandler::send_http_response_impl(
            std::shared_ptr<SslSession> session,
            const std::string &body,
            int status_code,
            bool is_chunked);

    template<typename SessionType>
    asio::awaitable<void> HttpHandler::send_http_response_impl(
            std::shared_ptr<SessionType> session,
            const std::string &body,
            int status_code,
            bool is_chunked) {
        // Map status codes to human-readable descriptions
        std::string status_str;
        switch (status_code) {
            case 200:
                status_str = "OK";
                break;
            case 201:
                status_str = "Created";
                break;
            case 202:
                status_str = "Accepted";
                break;
            case 204:
                status_str = "No Content";
                break;
            case 400:
                status_str = "Bad Request";
                break;
            case 401:
                status_str = "Unauthorized";
                break;
            case 403:
                status_str = "Forbidden";
                break;
            case 404:
                status_str = "Not Found";
                break;
            case 405:
                status_str = "Method Not Allowed";
                break;
            case 500:
                status_str = "Internal Server Error";
                break;
            case 501:
                status_str = "Not Implemented";
                break;
            case 503:
                status_str = "Service Unavailable";
                break;
            default:
                status_str = "Unknown";
        }

        // Build HTTP response
        std::ostringstream oss;
        oss << "HTTP/1.1 " << status_code << " " << status_str << "\r\n";

        // Add standard headers
        oss << "Content-Type: application/json\r\n";
        oss << "Server: MCPServer++\r\n";

        // Add Content-Length header (except for chunked responses)
        if (!is_chunked) {
            oss << "Content-Length: " << body.size() << "\r\n";
        } else {
            oss << "Transfer-Encoding: chunked\r\n";
        }

        // Connection management based on client request and server policy
        std::string client_connection = get_header_value(session->get_headers(), "Connection");
        bool keep_alive = (client_connection.empty() || strcasecmp(client_connection.c_str(), "close") != 0);

        if (!client_connection.empty()) {
            // Respect client's connection preference if specified
            keep_alive = (strcasecmp(client_connection.c_str(), "keep-alive") == 0);
        }

        // Set connection header and keep-alive parameters
        if (keep_alive) {
            oss << "Connection: keep-alive\r\n";
            oss << "Keep-Alive: timeout=300, max=100\r\n";// 5-minute timeout, max 100 requests
        } else {
            oss << "Connection: close\r\n";
        }

        // End of headers marker (CRLF)
        oss << "\r\n";

        // Append response body only for non-202/204 status codes
        if (status_code != 202 && status_code != 204 && !is_chunked) {
            oss << body;
        }

        // Ensure buffer is clean before sending
        co_await discard_existing_buffer(session);

        // Send the complete response
        std::string response = oss.str();

        if (is_chunked) {
            // For chunked responses, we send the headers first
            co_await session->write(response);

            // Then send the body as a chunk
            if (!body.empty()) {
                std::ostringstream chunk_stream;
                chunk_stream << std::hex << body.size() << "\r\n"
                             << body << "\r\n";
                std::string chunk = chunk_stream.str();
                co_await session->write(chunk);
            }
        } else {
            // For regular responses, send everything at once
            if (status_code != 202 && status_code != 204) {
                response += body;
            }
            co_await session->write(response);
        }

        if constexpr (std::is_same_v<SessionType, Session>) {
            MCP_DEBUG("Sent HTTP {} response (Session: {})", status_code, session->get_session_id());
        } else {
            MCP_DEBUG("Sent HTTPS {} response (Session: {})", status_code, session->get_session_id());
        }

        // Close connection only if explicitly requested
        if (!keep_alive) {
            MCP_DEBUG("Closing connection as requested (Session: {})", session->get_session_id());
            session->close();
        } else {
            MCP_DEBUG("Keeping connection alive (Session: {})", session->get_session_id());
        }

        co_return;
    }

    // Main request handling logic
    awaitable<void> HttpHandler::handle_request(
            std::shared_ptr<Session> session,
            const std::string &raw_request) {
        return handle_request_impl(session, raw_request);
    }

    // Main request handling logic for SSL sessions
    awaitable<void> HttpHandler::handle_request(
            std::shared_ptr<SslSession> session,
            const std::string &raw_request) {
        return handle_request_impl(session, raw_request);
    }

    template<typename SessionType>
    awaitable<void> HttpHandler::handle_request_impl(
            std::shared_ptr<SessionType> session,
            const std::string &raw_request) {
        // Start performance tracking
        auto metrics = mcp::metrics::PerformanceTracker::start_tracking(raw_request.size());

        // Parse HTTP request
        size_t bytes_parsed = 0;// New variable to record parsed bytes
        auto req_opt = parse_request(raw_request);
        bool is_valid_request = req_opt.has_value();
        HttpRequest req;
        if (is_valid_request) {
            req = *req_opt;
            // Calculate parsed bytes
            bytes_parsed = req.method.size() + 1 + req.target.size() + 1 + req.version.size() + 2;// Request line length
            for (const auto &[key, value]: req.headers) {
                bytes_parsed += key.size() + 2 + value.size() + 2;// Each header length
            }
            bytes_parsed += 2;                // Empty line ending headers
            bytes_parsed += req.body.size();  // Body length
            session->set_headers(req.headers);//save headers
        }

        if (auth_manager_) {
            if (!auth_manager_->validate(req.headers)) {
                MCP_WARN("Auth failed: invalid token (Session: {})", session->get_session_id());
                co_await send_http_response(session, R"({"error":"Unauthorized"})", 401);
                session->close();
                co_return;
            }
            MCP_DEBUG("Auth passed: {} (Session: {})", auth_manager_->type(), session->get_session_id());
        }

        bool error_occurred = false;
        std::string error_response = R"({"error":"Internal Server Error"})";

        try {
            // Key: process remaining request body regardless of request validity
            if (is_valid_request && req.headers.count("Content-Length")) {
                co_await discard_remaining_request_body(session, req, bytes_parsed);
            }

            // Handle invalid request
            if (!is_valid_request) {
                // AOP: Before request callback for invalid requests
                if (before_request_callback_) {
                    HttpRequest empty_req;
                    before_request_callback_(empty_req, session->get_session_id());
                }

                co_await send_http_response_impl(session, R"({"error":"Invalid HTTP request"})", 400);

                // End performance tracking for invalid requests
                mcp::metrics::PerformanceTracker::end_tracking(metrics, error_response.size());
                metrics_manager_->report_performance(
                        mcp::metrics::TrackedHttpRequest{},
                        metrics,
                        session->get_session_id());

                co_return;
            }

            // Validate path - support both /mcp and MCP tool endpoints
            bool is_valid_path = (req.target == "/mcp") ||
                                 (req.target == "/tools/list") ||
                                 (req.target == "/tools/call");

            if (!is_valid_path) {
                // AOP: Before request callback for invalid path requests
                if (before_request_callback_) {
                    before_request_callback_(req, session->get_session_id());
                }

                std::string not_found_response = R"({"error":"Not Found"})";
                co_await send_http_response_impl(session, not_found_response, 404);

                // End performance tracking for not found requests
                mcp::metrics::PerformanceTracker::end_tracking(metrics, not_found_response.size());
                mcp::metrics::TrackedHttpRequest tracked_req;
                tracked_req.method = req.method;
                tracked_req.target = req.target;
                tracked_req.version = req.version;
                tracked_req.headers = req.headers;
                tracked_req.body = req.body;

                metrics_manager_->report_performance(
                        tracked_req,
                        metrics,
                        session->get_session_id());

                co_return;
            }

            // AOP: Before request callback
            if (before_request_callback_) {
                before_request_callback_(req, session->get_session_id());
            }

            // Rate limiting check
            rate_limiter_->report_request_started(session->get_session_id());
            mcp::metrics::TrackedHttpRequest tracked_req_for_rate_limiting;
            tracked_req_for_rate_limiting.method = req.method;
            tracked_req_for_rate_limiting.target = req.target;
            tracked_req_for_rate_limiting.version = req.version;
            tracked_req_for_rate_limiting.headers = req.headers;
            tracked_req_for_rate_limiting.body = req.body;

            auto rate_limit_decision = rate_limiter_->check_request_allowed(
                    tracked_req_for_rate_limiting,
                    session->get_session_id());

            if (rate_limit_decision != mcp::metrics::RateLimitDecision::ALLOW) {
                std::string rate_limit_response;
                int status_code = 429;// Too Many Requests

                switch (rate_limit_decision) {
                    case mcp::metrics::RateLimitDecision::RATE_LIMITED:
                        rate_limit_response = R"({"error":"Rate limit exceeded"})";
                        break;
                    case mcp::metrics::RateLimitDecision::TOO_LARGE:
                        rate_limit_response = R"({"error":"Request too large"})";
                        status_code = 413;// Payload Too Large
                        break;
                    default:
                        rate_limit_response = R"({"error":"Request not allowed"})";
                        break;
                }

                co_await send_http_response_impl(session, rate_limit_response, status_code);

                // End performance tracking for rate limited requests
                mcp::metrics::PerformanceTracker::end_tracking(metrics, rate_limit_response.size());
                mcp::metrics::TrackedHttpRequest tracked_req;
                tracked_req.method = req.method;
                tracked_req.target = req.target;
                tracked_req.version = req.version;
                tracked_req.headers = req.headers;
                tracked_req.body = req.body;

                metrics_manager_->report_performance(
                        tracked_req,
                        metrics,
                        session->get_session_id());

                rate_limiter_->report_request_completed(session->get_session_id());

                co_return;
            }

            std::string session_id = session->get_session_id();
            MCP_DEBUG("Using session from TCP connection: {}", session_id);
            MCP_DEBUG("Request method: {}, target: {}", req.method, req.target);

            // Handle GET request (SSE connection initialization)
            if (req.method == "GET") {
                std::string accept_header = get_header_value(req.headers, "Accept");
                if (accept_header.find("text/event-stream") != std::string::npos) {
                    session->set_accept_header(accept_header);

                    // End performance tracking for SSE requests
                    mcp::metrics::PerformanceTracker::end_tracking(metrics, 0);
                    mcp::metrics::TrackedHttpRequest tracked_req;
                    tracked_req.method = req.method;
                    tracked_req.target = req.target;
                    tracked_req.version = req.version;
                    tracked_req.headers = req.headers;
                    tracked_req.body = req.body;

                    metrics_manager_->report_performance(
                            tracked_req,
                            metrics,
                            session->get_session_id());

                    co_return;// Wait for business layer to send SSE header
                } else {
                    std::string method_not_allowed_response = R"({"error":"Method Not Allowed"})";
                    co_await send_http_response_impl(session, method_not_allowed_response, 405);

                    // End performance tracking for method not allowed requests
                    mcp::metrics::PerformanceTracker::end_tracking(metrics, method_not_allowed_response.size());
                    mcp::metrics::TrackedHttpRequest tracked_req;
                    tracked_req.method = req.method;
                    tracked_req.target = req.target;
                    tracked_req.version = req.version;
                    tracked_req.headers = req.headers;
                    tracked_req.body = req.body;

                    metrics_manager_->report_performance(
                            tracked_req,
                            metrics,
                            session->get_session_id());

                    co_return;
                }
            }
            // Handle POST request (JSON-RPC)
            else if (req.method == "POST") {
                std::string accept_header = get_header_value(req.headers, "Accept");
                session->set_accept_header(accept_header);

                // Parse JSON-RPC request to determine if it's a notification (no id)
                bool is_notification = false;
                try {
                    nlohmann::json rpc_request = nlohmann::json::parse(req.body);
                    is_notification = !rpc_request.contains("id");// Notification has no id
                } catch (...) {
                    // Parsing failed, handle as regular request
                }

                // Business layer processes the message
                // For SslSession, we need to pass a dummy Session shared_ptr to match the callback signature
                if constexpr (std::is_same_v<SessionType, Session>) {
                    on_message_(req.body, session, session_id);
                } else {
                    std::shared_ptr<Session> dummy_session = nullptr;
                    on_message_(req.body, dummy_session, session_id);
                }

                // Core fix: send 202 response for notifications
                if (is_notification) {
                    MCP_DEBUG("Sending 202 Accepted for notification (Session: {})", session->get_session_id());
                    co_await send_http_response_impl(session, "", 202);// Empty body, 202 status code

                    // AOP: After request callback for notifications
                    if (after_request_callback_) {
                        after_request_callback_(req, "", 202, session_id);
                    }

                    // End performance tracking for notifications
                    mcp::metrics::PerformanceTracker::end_tracking(metrics, 0);
                    mcp::metrics::TrackedHttpRequest tracked_req;
                    tracked_req.method = req.method;
                    tracked_req.target = req.target;
                    tracked_req.version = req.version;
                    tracked_req.headers = req.headers;
                    tracked_req.body = req.body;

                    metrics_manager_->report_performance(
                            tracked_req,
                            metrics,
                            session->get_session_id());
                }

                co_return;
            }
            // Handle DELETE request (end session)
            else if (req.method == "DELETE") {
                MCP_INFO("Session terminated: {}", session_id);
                co_await session->write("HTTP/1.1 204 No Content\r\n\r\n");
                session->close();

                // AOP: After request callback for DELETE requests
                if (after_request_callback_) {
                    after_request_callback_(req, "", 204, session_id);
                }

                // End performance tracking for DELETE requests
                mcp::metrics::PerformanceTracker::end_tracking(metrics, 0);
                mcp::metrics::TrackedHttpRequest tracked_req;
                tracked_req.method = req.method;
                tracked_req.target = req.target;
                tracked_req.version = req.version;
                tracked_req.headers = req.headers;
                tracked_req.body = req.body;

                metrics_manager_->report_performance(
                        tracked_req,
                        metrics,
                        session->get_session_id());

                rate_limiter_->report_request_completed(session->get_session_id());

                co_return;
            }
            // Unsupported method
            else {
                std::string method_not_allowed_response = R"({"error":"Method Not Allowed"})";
                co_await send_http_response_impl(session, method_not_allowed_response, 405);

                // End performance tracking for unsupported method requests
                mcp::metrics::PerformanceTracker::end_tracking(metrics, method_not_allowed_response.size());
                mcp::metrics::TrackedHttpRequest tracked_req;
                tracked_req.method = req.method;
                tracked_req.target = req.target;
                tracked_req.version = req.version;
                tracked_req.headers = req.headers;
                tracked_req.body = req.body;

                metrics_manager_->report_performance(
                        tracked_req,
                        metrics,
                        session->get_session_id());

                rate_limiter_->report_request_completed(session->get_session_id());

                co_return;
            }
        } catch (const std::exception &e) {
            MCP_ERROR("Error handling request: {}", e.what());
            error_occurred = true;

            // AOP: Error callback
            if (on_error_callback_) {
                on_error_callback_(e.what(), session->get_session_id());
            }

            // Report error to metrics manager
            metrics_manager_->report_error(e.what(), session->get_session_id());

            rate_limiter_->report_request_completed(session->get_session_id());

            session->close();
        }

        if (error_occurred) {
            co_await send_http_response_impl(session, error_response, 500);
        }

        // End performance tracking for successful requests
        mcp::metrics::PerformanceTracker::end_tracking(metrics, error_response.size());
        mcp::metrics::TrackedHttpRequest tracked_req;
        tracked_req.method = req.method;
        tracked_req.target = req.target;
        tracked_req.version = req.version;
        tracked_req.headers = req.headers;
        tracked_req.body = req.body;

        metrics_manager_->report_performance(
                tracked_req,
                metrics,
                session->get_session_id());

        rate_limiter_->report_request_completed(session->get_session_id());
    }

    // Helper function: fully read and discard remaining request body (avoid buffer residue)
    asio::awaitable<void> HttpHandler::discard_remaining_request_body(std::shared_ptr<Session> session, const HttpRequest &request, [[maybe_unused]] size_t already_read) {
        try {
            auto it = request.headers.find("Content-Length");
            if (it == request.headers.end()) {
                co_return;
            }

            size_t content_len = std::stoull(it->second);

            // Calculate already read body length
            size_t already_read_body = request.body.size();

            // Key: add underflow protection to avoid negative numbers (unsigned underflow)
            if (already_read_body >= content_len) {
                co_return;
            }

            size_t remaining = content_len - already_read_body;
            if (remaining <= 0) {
                co_return;
            }

            // Continue reading logic...
            // Loop to read and discard remaining data
            std::array<char, 4096> buf;
            while (remaining > 0) {
                size_t to_read = std::min(remaining, buf.size());
                size_t n = co_await session->get_socket().async_read_some(
                        asio::buffer(buf, to_read),
                        use_awaitable);
                remaining -= n;
            }
        } catch (const std::exception &e) {
            MCP_WARN("Failed to discard remaining request body: {}", e.what());
        }
        co_return;
    }

    // Helper function: fully read and discard remaining request body for SSL sessions (avoid buffer residue)
    asio::awaitable<void> HttpHandler::discard_remaining_request_body(std::shared_ptr<SslSession> session, const HttpRequest &request, [[maybe_unused]] size_t already_read) {
        try {
            auto it = request.headers.find("Content-Length");
            if (it == request.headers.end()) {
                co_return;
            }

            size_t content_len = std::stoull(it->second);

            // Calculate already read body length
            size_t already_read_body = request.body.size();

            // Key: add underflow protection to avoid negative numbers (unsigned underflow)
            if (already_read_body >= content_len) {
                co_return;
            }

            size_t remaining = content_len - already_read_body;
            if (remaining <= 0) {
                co_return;
            }

            // Continue reading logic...
            // Loop to read and discard remaining data
            std::array<char, 4096> buf;
            while (remaining > 0) {
                size_t to_read = std::min(remaining, buf.size());
                size_t n = co_await session->get_stream().async_read_some(
                        asio::buffer(buf, to_read),
                        use_awaitable);
                remaining -= n;
            }
        } catch (const std::exception &e) {
            MCP_WARN("Failed to discard remaining request body: {}", e.what());
        }
        co_return;
    }

    // Case-insensitive string comparison
    int HttpHandler::strcasecmp(const char *s1, const char *s2) {
        while (*s1 && (tolower(*s1) == tolower(*s2))) {
            s1++;
            s2++;
        }
        return tolower(*s1) - tolower(*s2);
    }

    // Send SSE event to client
    template<typename SessionType>
    asio::awaitable<void> HttpHandler::send_sse_event(
            std::shared_ptr<SessionType> session,
            const std::string &event_type,
            int event_id,
            const std::string &data) {
        std::ostringstream oss;
        oss << "event: " << event_type << "\n";
        oss << "id: " << event_id << "\n";
        oss << "data: " << data << "\n\n";

        std::string event = oss.str();
        co_await session->stream_write(event);
        co_return;
    }

    // Explicit instantiation of send_sse_event for both session types
    template asio::awaitable<void> HttpHandler::send_sse_event(std::shared_ptr<Session> session, const std::string &event_type, int event_id, const std::string &data);
    template asio::awaitable<void> HttpHandler::send_sse_event(std::shared_ptr<SslSession> session, const std::string &event_type, int event_id, const std::string &data);

    // Streaming response methods
    asio::awaitable<void> HttpHandler::start_streaming_response(std::shared_ptr<Session> session, const std::string &content_type) {
        return start_streaming_response_impl(session, content_type);
    }

    asio::awaitable<void> HttpHandler::start_streaming_response(std::shared_ptr<SslSession> session, const std::string &content_type) {
        return start_streaming_response_impl(session, content_type);
    }

    template<typename SessionType>
    asio::awaitable<void> HttpHandler::start_streaming_response_impl(std::shared_ptr<SessionType> session, const std::string &content_type) {
        std::ostringstream oss;
        oss << "HTTP/1.1 200 OK\r\n";
        oss << "Content-Type: " << content_type << "\r\n";
        oss << "Transfer-Encoding: chunked\r\n";
        oss << "Connection: keep-alive\r\n";
        oss << "\r\n";

        co_await session->write(oss.str());
        session->set_streaming(true);
        co_return;
    }

    asio::awaitable<void> HttpHandler::send_streaming_chunk(std::shared_ptr<Session> session, const std::string &chunk) {
        return send_streaming_chunk_impl(session, chunk);
    }

    asio::awaitable<void> HttpHandler::send_streaming_chunk(std::shared_ptr<SslSession> session, const std::string &chunk) {
        return send_streaming_chunk_impl(session, chunk);
    }

    template<typename SessionType>
    asio::awaitable<void> HttpHandler::send_streaming_chunk_impl(std::shared_ptr<SessionType> session, const std::string &chunk) {
        if (!session->is_streaming()) {
            throw std::runtime_error("Session is not in streaming mode");
        }

        std::ostringstream oss;
        oss << std::hex << chunk.size() << "\r\n"
            << chunk << "\r\n";
        co_await session->write(oss.str());
        co_return;
    }

    asio::awaitable<void> HttpHandler::end_streaming_response(std::shared_ptr<Session> session) {
        return end_streaming_response_impl(session);
    }

    asio::awaitable<void> HttpHandler::end_streaming_response(std::shared_ptr<SslSession> session) {
        return end_streaming_response_impl(session);
    }

    template<typename SessionType>
    asio::awaitable<void> HttpHandler::end_streaming_response_impl(std::shared_ptr<SessionType> session) {
        if (!session->is_streaming()) {
            throw std::runtime_error("Session is not in streaming mode");
        }

        // Send final chunk
        co_await session->write("0\r\n\r\n");
        session->set_streaming(false);
        co_return;
    }

    // Send chunk methods
    asio::awaitable<void> HttpHandler::send_chunk(std::shared_ptr<Session> session, const std::string &data) {
        return send_streaming_chunk_impl(session, data);
    }

    asio::awaitable<void> HttpHandler::send_chunk(std::shared_ptr<SslSession> session, const std::string &data) {
        return send_streaming_chunk_impl(session, data);
    }

    asio::awaitable<void> HttpHandler::send_chunk_end(std::shared_ptr<Session> session) {
        return end_streaming_response_impl(session);
    }

    asio::awaitable<void> HttpHandler::send_chunk_end(std::shared_ptr<SslSession> session) {
        return end_streaming_response_impl(session);
    }

}// namespace mcp::transport