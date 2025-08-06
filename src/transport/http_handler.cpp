#include "http_handler.h"
#include "core/logger.h"
#include "session.h"
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

    // Send HTTP response (unique implementation, remove duplicate definitions)
    awaitable<void> HttpHandler::send_http_response(
            std::shared_ptr<Session> session,
            const std::string &body,
            int status_code) {
        // Status description string
        std::string status_str;
        switch (status_code) {
            case 200:
                status_str = "OK";
                break;
            case 202:
                status_str = "Accepted";
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
                status_str = "Unknown";
                break;
        }

        // Build response content
        std::ostringstream oss;
        oss << "HTTP/1.1 " << status_code << " " << status_str << "\r\n";
        oss << "Content-Type: application/json\r\n";
        oss << "Content-Length: " << body.size() << "\r\n";
        oss << "Connection: close\r\n";// Force close connection for non-streaming response
        oss << "\r\n";                 // End of headers marker
        oss << body;

        // Clean buffer before sending response
        co_await discard_existing_buffer(session);

        // Send response and close session
        co_await session->write(oss.str());
        session->close();
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
            bytes_parsed += 2;              // Empty line ending headers
            bytes_parsed += req.body.size();// Body length
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

            // Validate path
            if (req.target != "/mcp") {
                co_await send_http_response(session, R"({"error":"Not Found"})", 404);
                co_return;
            }

            // Get session ID (from header or generate)
            std::string session_id = get_header_value(req.headers, "Mcp-Session-Id");

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
                on_message_(req.body, session, session_id);// Delegate to business layer
                co_return;
            }
            // Handle DELETE request (end session)
            else if (req.method == "DELETE") {
                if (!session_id.empty()) {
                    MCP_INFO("Session terminated: {}", session_id);
                }
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

    // Case-insensitive string comparison implementation
    int HttpHandler::strcasecmp(const char *s1, const char *s2) {
        while (*s1 && (tolower(*s1) == tolower(*s2))) {
            s1++;
            s2++;
        }
        return tolower(*s1) - tolower(*s2);
    }

}// namespace mcp::transport