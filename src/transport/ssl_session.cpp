#include "ssl_session.h"
#include "core/logger.h"
#include "http_handler.h"
#include <iomanip>
#include <random>

using asio::use_awaitable;

namespace mcp::transport {

    namespace {
        // generate random data for session id
        static std::string generate_session_id() noexcept {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<uint64_t> dist;

            uint64_t part1 = dist(gen);
            uint64_t part2 = dist(gen);

            std::stringstream ss;
            ss << std::hex << std::setw(16) << std::setfill('0') << part1
               << std::hex << std::setw(16) << std::setfill('0') << part2;
            return ss.str();
        }
    }// namespace


    SslSession::SslSession(asio::ip::tcp::socket socket, asio::ssl::context &ssl_context)
        : Session(std::move(socket)), ssl_stream_(std::move(socket), ssl_context) {
        session_id_ = generate_session_id();
    }


    asio::awaitable<void> SslSession::start(HttpHandler *handler) {
        try {
            // Perform SSL handshake
            co_await ssl_stream_.async_handshake(asio::ssl::stream_base::server, asio::use_awaitable);

            std::string request_buffer;
            while (ssl_stream_.lowest_layer().is_open()) {
                auto n = co_await ssl_stream_.async_read_some(asio::buffer(buffer_), asio::use_awaitable);
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

                    // Content Length header parsing
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
                std::string error_msg = e.what();
                error_msg.erase(std::remove_if(error_msg.begin(), error_msg.end(),
                                               [](unsigned char c) { return !std::isprint(c) && !std::isspace(c); }),
                                error_msg.end());

                MCP_WARN("SSL Session read error: {}", error_msg);
            }
        }
        close();
        co_return;
    }


    asio::awaitable<void> SslSession::write(const std::string &message) {
        if (!ssl_stream_.lowest_layer().is_open()) {
            co_return;
        }
        try {
            // make sure to clear any existing data in the buffer
            std::array<char, 8192> discard_buf;
            asio::error_code ec;
            while (ssl_stream_.lowest_layer().available(ec) > 0 && !ec) {
                ssl_stream_.next_layer().read_some(asio::buffer(discard_buf), ec);
            }

            co_await asio::async_write(ssl_stream_, asio::buffer(message), asio::use_awaitable);
        } catch (const std::exception &e) {
            MCP_ERROR("Failed to write to SSL socket: {}", e.what());
            close();
        }
        co_return;
    }

    void SslSession::close() {
        if (!closed_ && ssl_stream_.lowest_layer().is_open()) {
            asio::error_code ec;
            ssl_stream_.lowest_layer().cancel(ec);

            std::array<char, 8192> discard_buf;
            while (ssl_stream_.lowest_layer().available(ec) > 0 && !ec) {
                ssl_stream_.next_layer().read_some(asio::buffer(discard_buf), ec);
            }

            ssl_stream_.lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
            ssl_stream_.lowest_layer().close(ec);
            std::fill(buffer_.begin(), buffer_.end(), static_cast<char>(0));
            closed_ = true;
        }
    }

    void SslSession::clear_buffer() {
        std::fill(buffer_.begin(), buffer_.end(), static_cast<char>(0));
    }

    bool SslSession::is_closed() const {
        return !ssl_stream_.lowest_layer().is_open();
    }

    asio::ssl::stream<asio::ip::tcp::socket> &SslSession::get_stream() {
        return ssl_stream_;
    }

}// namespace mcp::transport