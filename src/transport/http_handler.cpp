#include "http_handler.h"
#include "core/logger.h"
#include "nlohmann/json.hpp"
#include "session.h"
#include "ssl_session.h"
#include "utils/utf8_encode.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>


using asio::awaitable;
using asio::use_awaitable;

namespace mcp::transport {

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
    std::optional<HttpHandler::HttpRequest> HttpHandler::parse_request(const std::string &raw_request) {
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
        // Map status codes to human-readable descriptions
        std::string status_str;
        switch (status_code) {
            case 200:
                status_str = "OK";
                break;
            case 202:
                status_str = "Accepted";// For notifications (no response body)
                break;
            case 204:
                status_str = "No Content";// For DELETE requests
                break;
            case 400:
                status_str = "Bad Request";
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
            default:
                status_str = "Unknown Status";
                break;
        }

        // Build HTTP response headers
        std::ostringstream oss;
        oss << "HTTP/1.1 " << status_code << " " << status_str << "\r\n";

        // Handle content type and length based on status code (MCP protocol requirement)
        if (status_code == 202 || status_code == 204) {
            // Notifications or no-content responses: no body, no JSON type
            oss << "Content-Length: 0\r\n";// Explicit empty content
        } else {
            // Regular responses with body: JSON content type
            oss << "Content-Type: application/json\r\n";
            oss << "Content-Length: " << body.size() << "\r\n";
        }

        // Determine connection persistence strategy
        const std::string client_connection = get_header_value(session->get_headers(), "Connection");
        bool keep_alive = true;

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
        if (status_code != 202 && status_code != 204) {
            oss << body;
        }

        // Ensure buffer is clean before sending
        co_await discard_existing_buffer(session);

        // Send the complete response
        const std::string response = oss.str();
        co_await session->write(response);
        MCP_DEBUG("Sent HTTP {} response (Session: {})", status_code, session->get_session_id());

        // Close connection only if explicitly requested
        if (!keep_alive) {
            MCP_DEBUG("Closing connection as requested (Session: {})", session->get_session_id());
            session->close();
        } else {
            MCP_DEBUG("Keeping connection alive (Session: {})", session->get_session_id());
        }

        co_return;
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
        // Map status codes to human-readable descriptions
        std::string status_str;
        switch (status_code) {
            case 200:
                status_str = "OK";
                break;
            case 202:
                status_str = "Accepted";// For notifications (no response body)
                break;
            case 204:
                status_str = "No Content";// For DELETE requests
                break;
            case 400:
                status_str = "Bad Request";
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
            default:
                status_str = "Unknown Status";
                break;
        }

        // Build HTTP response headers
        std::ostringstream oss;
        oss << "HTTP/1.1 " << status_code << " " << status_str << "\r\n";

        // Handle content type and length based on status code (MCP protocol requirement)
        if (status_code == 202 || status_code == 204) {
            // Notifications or no-content responses: no body, no JSON type
            oss << "Content-Length: 0\r\n";// Explicit empty content
        } else {
            // Regular responses with body: JSON content type
            oss << "Content-Type: application/json\r\n";
            oss << "Content-Length: " << body.size() << "\r\n";
        }

        // Determine connection persistence strategy
        const std::string client_connection = get_header_value(session->get_headers(), "Connection");
        bool keep_alive = true;

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
        if (status_code != 202 && status_code != 204) {
            oss << body;
        }

        // Ensure buffer is clean before sending
        co_await discard_existing_buffer(session);

        // Send the complete response
        const std::string response = oss.str();
        co_await session->write(response);
        MCP_DEBUG("Sent HTTPS {} response (Session: {})", status_code, session->get_session_id());

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

        bool error_occurred = false;
        std::string error_response = R"({"error":"Internal Server Error"})";

        try {
            // Key: process remaining request body regardless of request validity
            if (is_valid_request && req.headers.count("Content-Length")) {
                co_await discard_remaining_request_body(session, req, bytes_parsed);
            }

            // Handle invalid request
            if (!is_valid_request) {
                co_await send_http_response(session, R"({"error":"Invalid HTTP request"})", 400);
                co_return;
            }

            // Validate path - support both /mcp and MCP tool endpoints
            bool is_valid_path = (req.target == "/mcp") ||
                                 (req.target == "/tools/list") ||
                                 (req.target == "/tools/call");

            if (!is_valid_path) {
                co_await send_http_response(session, R"({"error":"Not Found"})", 404);
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
                    co_return;// Wait for business layer to send SSE header
                } else {
                    co_await send_http_response(session, R"({"error":"Method Not Allowed"})", 405);
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
                on_message_(req.body, session, session_id);

                // Core fix: send 202 response for notifications
                if (is_notification) {
                    MCP_DEBUG("Sending 202 Accepted for notification (Session: {})", session->get_session_id());
                    co_await send_http_response(session, "", 202);// Empty body, 202 status code
                }

                co_return;
            }
            // Handle DELETE request (end session)
            else if (req.method == "DELETE") {
                MCP_INFO("Session terminated: {}", session_id);
                co_await session->write("HTTP/1.1 204 No Content\r\n\r\n");
                session->close();
                co_return;
            }
            // Unsupported method
            else {
                co_await send_http_response(session, R"({"error":"Method Not Allowed"})", 405);
                co_return;
            }
        } catch (const std::exception &e) {
            MCP_ERROR("Error handling request: {}", e.what());
            error_occurred = true;
            session->close();
        }

        if (error_occurred) {
            co_await send_http_response(session, error_response, 500);
        }
    }

    // Main request handling logic for SSL sessions
    awaitable<void> HttpHandler::handle_request(
            std::shared_ptr<SslSession> session,
            const std::string &raw_request) {
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

        bool error_occurred = false;
        std::string error_response = R"({"error":"Internal Server Error"})";

        try {
            // Key: process remaining request body regardless of request validity
            if (is_valid_request && req.headers.count("Content-Length")) {
                co_await discard_remaining_request_body(session, req, bytes_parsed);
            }

            // Handle invalid request
            if (!is_valid_request) {
                co_await send_http_response(session, R"({"error":"Invalid HTTP request"})", 400);
                co_return;
            }

            // Validate path - support both /mcp and MCP tool endpoints
            bool is_valid_path = (req.target == "/mcp") ||
                                 (req.target == "/tools/list") ||
                                 (req.target == "/tools/call");

            if (!is_valid_path) {
                co_await send_http_response(session, R"({"error":"Not Found"})", 404);
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
                    co_return;// Wait for business layer to send SSE header
                } else {
                    co_await send_http_response(session, R"({"error":"Method Not Allowed"})", 405);
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
                // Create a dummy Session shared_ptr to match the callback signature
                std::shared_ptr<Session> dummy_session = nullptr;
                on_message_(req.body, dummy_session, session_id);

                // Core fix: send 202 response for notifications
                if (is_notification) {
                    MCP_DEBUG("Sending 202 Accepted for notification (Session: {})", session->get_session_id());
                    co_await send_http_response(session, "", 202);// Empty body, 202 status code
                }

                co_return;
            }
            // Handle DELETE request (end session)
            else if (req.method == "DELETE") {
                MCP_INFO("Session terminated: {}", session_id);
                co_await session->write("HTTP/1.1 204 No Content\r\n\r\n");
                session->close();
                co_return;
            }
            // Unsupported method
            else {
                co_await send_http_response(session, R"({"error":"Method Not Allowed"})", 405);
                co_return;
            }
        } catch (const std::exception &e) {
            MCP_ERROR("Error handling request: {}", e.what());
            error_occurred = true;
            session->close();
        }

        if (error_occurred) {
            co_await send_http_response(session, error_response, 500);
        }
    }

    // Helper function: fully read and discard remaining request body (avoid buffer residue)
    asio::awaitable<void> HttpHandler::discard_remaining_request_body(
            std::shared_ptr<Session> session,
            const HttpRequest &req,
            size_t already_read) {// Note parameter name changed to "total bytes read"
        try {
            auto it = req.headers.find("Content-Length");
            if (it == req.headers.end()) {
                co_return;
            }

            size_t content_len = std::stoull(it->second);

            // Calculate already read body length
            size_t already_read_body = req.body.size();

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
    asio::awaitable<void> HttpHandler::discard_remaining_request_body(
            std::shared_ptr<SslSession> session,
            const HttpRequest &req,
            size_t already_read) {// Note parameter name changed to "total bytes read"
        try {
            auto it = req.headers.find("Content-Length");
            if (it == req.headers.end()) {
                co_return;
            }

            size_t content_len = std::stoull(it->second);

            // Calculate already read body length
            size_t already_read_body = req.body.size();

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

    // Case-insensitive string comparison implementation
    int HttpHandler::strcasecmp(const char *s1, const char *s2) {
        while (*s1 && (tolower(*s1) == tolower(*s2))) {
            s1++;
            s2++;
        }
        return tolower(*s1) - tolower(*s2);
    }

}// namespace mcp::transport