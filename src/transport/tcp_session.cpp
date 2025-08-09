#include "core/logger.h"
#include "http_handler.h"
#include "session.h"
#include <iomanip>
#include <random>

using asio::use_awaitable;

namespace mcp::transport {

    TcpSession::TcpSession(asio::ip::tcp::socket socket) : Session(std::move(socket)) {
    }

    asio::awaitable<void> TcpSession::start(HttpHandler *handler) {
        try {
            std::string request_buffer;
            while (socket_.is_open()) {
                auto n = co_await socket_.async_read_some(asio::buffer(buffer_), use_awaitable);
                if (n == 0) break;

                // append the newly read data to the request buffer
                request_buffer.append(buffer_.data(), n);

                // check continously for complete HTTP requests
                while (!request_buffer.empty()) {
                    size_t header_end = request_buffer.find("\r\n\r\n");
                    if (header_end == std::string::npos) {
                        // not yet, wait for more data
                        break;
                    }

                    // Content Lenth header parsing
                    size_t content_length = 0;
                    size_t content_length_pos = request_buffer.find("Content-Length:");
                    if (content_length_pos != std::string::npos && content_length_pos < header_end) {
                        size_t value_start = content_length_pos + 16;// the pos after "Content-Length:"
                        size_t value_end = request_buffer.find_first_of("\r\n", value_start);
                        if (value_end != std::string::npos) {
                            std::string length_str = request_buffer.substr(value_start, value_end - value_start);
                            length_str.erase(0, length_str.find_first_not_of(" \t"));
                            length_str.erase(length_str.find_last_not_of(" \t") + 1);
                            try {
                                content_length = std::stoull(length_str);
                            } catch (...) {
                                content_length = 0;
                            }
                        }
                    }
                    size_t total_required = header_end + 4 + content_length;// 4 header+body lenth

                    // check if the request is complete
                    if (request_buffer.length() >= total_required) {
                        std::string complete_request = request_buffer.substr(0, total_required);
                        co_await handler->handle_request(shared_from_this(), complete_request);

                        // remove the processed request from the buffer
                        request_buffer.erase(0, total_required);
                    } else {
                        // the request is not complete, wait for more data
                        break;
                    }
                }
            }
        } catch (const std::exception &e) {
            if (!closed_) {
                MCP_WARN("Session read error: {}", e.what());
            }
        }
        close();
        co_return;
    }


    asio::awaitable<void> TcpSession::write(const std::string &message) {
        if (!socket_.is_open()) {
            co_return;
        }
        try {
            // make sure to clear any existing data in the buffer
            std::array<char, 8192> discard_buf;
            asio::error_code ec;
            while (socket_.available(ec) > 0 && !ec) {
                socket_.read_some(asio::buffer(discard_buf), ec);
            }

            co_await asio::async_write(socket_, asio::buffer(message), use_awaitable);
        } catch (const std::exception &e) {
            MCP_ERROR("Failed to write to socket: {}", e.what());
            close();
        }
        co_return;
    }

    void TcpSession::close() {
        if (!closed_ && socket_.is_open()) {
            asio::error_code ec;
            socket_.cancel(ec);

            std::array<char, 8192> discard_buf;
            while (socket_.available(ec) > 0 && !ec) {
                socket_.read_some(asio::buffer(discard_buf), ec);
            }

            socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
            socket_.close(ec);
            std::fill(buffer_.begin(), buffer_.end(), static_cast<char>(0));
            closed_ = true;
        }
    }

    void TcpSession::clear_buffer() {
        std::fill(buffer_.begin(), buffer_.end(), static_cast<char>(0));
    }

    bool TcpSession::is_closed() const {
        return !socket_.is_open();
    }

}// namespace mcp::transport