#include "ssl_session.h"
#include "core/logger.h"
#include "http_handler.h"
#include <asio/ssl/error.hpp>

namespace mcp::transport {


    /**
     * @brief Construct an SslSession with a socket and SSL context.
     * @param socket TCP socket to wrap with SSL
     * @param ssl_context SSL context with certificate configuration
     */
    SslSession::SslSession(asio::ip::tcp::socket socket, asio::ssl::context &ssl_context)
        : ssl_stream_(std::move(socket), ssl_context) {
        session_id_ = generate_session_id();// Generate unique session ID

        // Validate socket state after construction
        if (!ssl_stream_.lowest_layer().is_open()) {
            MCP_ERROR("SslSession constructed with invalid socket (ID: {})", session_id_);
            throw std::runtime_error("Invalid socket passed to SslSession");
        }
        MCP_DEBUG("Created new SSL session with ID: {}", session_id_);
    }

    /**
     * @brief Start the SSL session, perform handshake, and begin reading requests.
     * @param handler HTTP handler for processing requests
     */
    asio::awaitable<void> SslSession::start(HttpHandler *handler) {
        // Validate session state before starting
        if (!ssl_stream_.lowest_layer().is_open() || closed_) {
            MCP_ERROR("Cannot start SSL session - socket is invalid (ID: {})", session_id_);
            close();
            co_return;
        }

        try {
            MCP_DEBUG("Initiating SSL handshake for session: {}", session_id_);

            // Perform SSL/TLS handshake in server mode
            co_await ssl_stream_.async_handshake(asio::ssl::stream_base::server, asio::use_awaitable);
            MCP_DEBUG("SSL handshake successful for session: {}", session_id_);

            std::string request_buffer;
            // Read and process requests
            while (!closed_ && ssl_stream_.lowest_layer().is_open()) {
                auto n = co_await ssl_stream_.async_read_some(asio::buffer(buffer_), asio::use_awaitable);
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
                MCP_WARN("SSL Session error (ID: {}): {}", session_id_, e.what());
            }
        }

        close();// Ensure session is closed
        co_return;
    }

    /**
     * @brief Write encrypted data to the SSL stream.
     * @param message Data to send to client
     */
    asio::awaitable<void> SslSession::write(const std::string &message) {
        if (closed_ || !ssl_stream_.lowest_layer().is_open()) {
            MCP_DEBUG("Attempted write to closed SSL session (ID: {})", session_id_);
            co_return;
        }

        try {
            // Clear any pending data in the socket buffer
            std::array<char, 8192> discard_buf;
            asio::error_code ec;
            while (ssl_stream_.lowest_layer().available(ec) > 0 && !ec) {
                ssl_stream_.next_layer().read_some(asio::buffer(discard_buf), ec);
            }

            // Send encrypted message
            co_await asio::async_write(ssl_stream_, asio::buffer(message), asio::use_awaitable);
            MCP_DEBUG("Successfully wrote {} bytes to SSL session (ID: {})", message.size(), session_id_);
        } catch (const std::exception &e) {
            MCP_ERROR("Failed to write to SSL socket (session ID: {}): {}", session_id_, e.what());
            close();
        }
        co_return;
    }

    /**
     * @brief Close the SSL session and release resources.
     */
    void SslSession::close() {
        if (closed_) {
            MCP_DEBUG("Attempted to close already closed SSL session (ID: {})", session_id_);
            return;
        }

        asio::error_code ec;
        closed_ = true;// Mark as closed first to prevent race conditions

        MCP_DEBUG("Closing SSL session (ID: {})", session_id_);

        // 1. Shutdown SSL layer gracefully
        ssl_stream_.shutdown(ec);
        if (ec && ec != asio::error::not_connected && ec != asio::ssl::error::stream_truncated) {
            MCP_WARN("SSL shutdown error (session ID: {}): {}", session_id_, ec.message());
        }

        // 2. Cancel pending operations
        ssl_stream_.lowest_layer().cancel(ec);
        if (ec) {
            MCP_WARN("Socket cancel error (session ID: {}): {}", session_id_, ec.message());
        }

        // 3. Read remaining data
        std::array<char, 8192> discard_buf;
        ec.clear();
        while (ssl_stream_.lowest_layer().available(ec) > 0 && !ec) {
            ssl_stream_.next_layer().read_some(asio::buffer(discard_buf), ec);
        }

        // 4. Shutdown and close underlying socket
        ssl_stream_.lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        if (ec && ec != asio::error::not_connected) {
            MCP_WARN("Socket shutdown error (session ID: {}): {}", session_id_, ec.message());
        }

        ssl_stream_.lowest_layer().close(ec);
        if (ec) {
            MCP_WARN("Socket close error (session ID: {}): {}", session_id_, ec.message());
        }

        // Clear buffer for security
        std::fill(buffer_.begin(), buffer_.end(), static_cast<char>(0));

        MCP_DEBUG("SSL session (ID: {}) closed successfully", session_id_);
    }

    /**
     * @brief Clear the read buffer.
     */
    void SslSession::clear_buffer() {
        std::fill(buffer_.begin(), buffer_.end(), static_cast<char>(0));
    }

    /**
     * @brief Check if the session is closed.
     * @return True if closed, false otherwise
     */
    bool SslSession::is_closed() const {
        return closed_ || !ssl_stream_.lowest_layer().is_open();
    }

    /**
     * @brief Get the underlying SSL stream.
     * @return Reference to SSL stream
     */
    asio::ssl::stream<asio::ip::tcp::socket> &SslSession::get_stream() {
        return ssl_stream_;
    }

}// namespace mcp::transport