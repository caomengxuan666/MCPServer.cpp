#include "tcp_session.h"
#include "core/logger.h"
#include "http_handler.h"
#include "session.h"
#include <iomanip>


using asio::use_awaitable;

namespace mcp::transport {

    /**
     * @brief Construct a TcpSession with a socket.
     * @param socket TCP socket to take ownership of
     */
    TcpSession::TcpSession(asio::ip::tcp::socket socket)
        : socket_(std::move(socket)) {
        session_id_ = generate_session_id();// Generate ID from base class helper
    }

    /**
     * @brief Start the TCP session and begin reading requests.
     * @param handler HTTP handler for processing requests
     */
    asio::awaitable<void> TcpSession::start(HttpHandler *handler) {
        try {
            std::string request_buffer;
            while (socket_.is_open()) {
                // Read data from socket
                auto n = co_await socket_.async_read_some(asio::buffer(buffer_), use_awaitable);
                if (n == 0) break;// Connection closed gracefully

                // Append new data to request buffer
                request_buffer.append(buffer_.data(), n);

                // Check for complete HTTP requests
                while (!request_buffer.empty()) {
                    size_t header_end = request_buffer.find("\r\n\r\n");
                    if (header_end == std::string::npos) {
                        break;// Incomplete headers - wait for more data
                    }

                    // Parse Content-Length header
                    size_t content_length = 0;
                    size_t content_length_pos = request_buffer.find("Content-Length:");
                    if (content_length_pos != std::string::npos && content_length_pos < header_end) {
                        size_t value_start = content_length_pos + 16;// After "Content-Length:"
                        size_t value_end = request_buffer.find_first_of("\r\n", value_start);
                        if (value_end != std::string::npos) {
                            std::string length_str = request_buffer.substr(value_start, value_end - value_start);
                            length_str.erase(0, length_str.find_first_not_of(" \t"));
                            length_str.erase(length_str.find_last_not_of(" \t") + 1);
                            try {
                                content_length = std::stoull(length_str);
                            } catch (...) {
                                content_length = 0;// Handle invalid length
                            }
                        }
                    }

                    // Check if request is complete
                    size_t total_required = header_end + 4 + content_length;// +4 for "\r\n\r\n"
                    if (request_buffer.length() >= total_required) {
                        std::string complete_request = request_buffer.substr(0, total_required);
                        co_await handler->handle_request(shared_from_this(), complete_request);
                        request_buffer.erase(0, total_required);// Remove processed data
                    } else {
                        break;// Incomplete request - wait for more data
                    }
                }
            }
        } catch (const std::exception &e) {
            if (!closed_) {
                MCP_WARN("TCP Session read error: {}", e.what());
            }
        }
        close();// Ensure session is closed
        co_return;
    }

    /**
     * @brief Write data to the TCP socket.
     * @param message Data to send to client
     */
    asio::awaitable<void> TcpSession::write(const std::string &message) {
        if (!socket_.is_open()) {
            co_return;
        }
        try {
            // Clear any pending data in the socket buffer
            std::array<char, 8192> discard_buf;
            asio::error_code ec;
            while (socket_.available(ec) > 0 && !ec) {
                socket_.read_some(asio::buffer(discard_buf), ec);
            }

            // Send the message
            co_await asio::async_write(socket_, asio::buffer(message), use_awaitable);
        } catch (const std::exception &e) {
            MCP_ERROR("Failed to write to TCP socket: {}", e.what());
            close();
        }
        co_return;
    }

    /**
     * @brief Close the TCP session and release resources.
     */
    void TcpSession::close() {
        if (!closed_ && socket_.is_open()) {
            asio::error_code ec;

            // Cancel pending operations
            socket_.cancel(ec);

            // Read remaining data
            std::array<char, 8192> discard_buf;
            while (socket_.available(ec) > 0 && !ec) {
                socket_.read_some(asio::buffer(discard_buf), ec);
            }

            // Shutdown and close socket
            socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
            socket_.close(ec);

            // Clear buffer for security
            std::fill(buffer_.begin(), buffer_.end(), static_cast<char>(0));
            closed_ = true;
        }
    }

    /**
     * @brief Clear the read buffer.
     */
    void TcpSession::clear_buffer() {
        std::fill(buffer_.begin(), buffer_.end(), static_cast<char>(0));
    }

    /**
     * @brief Check if the session is closed.
     * @return True if closed, false otherwise
     */
    bool TcpSession::is_closed() const {
        return closed_ || !socket_.is_open();
    }

}// namespace mcp::transport