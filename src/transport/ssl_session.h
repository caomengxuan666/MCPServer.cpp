#pragma once

#include <unordered_map>
#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0601
#endif

#include "session.h"
#include "transport_types.h"
#include <asio.hpp>
#include <asio/ssl.hpp>


namespace mcp::transport {
    class HttpHandler;
}

namespace mcp::transport {

    /**
     * @brief Manages a single SSL/TLS TCP session for HTTPS connections.
     * Handles encrypted IO operations only, no business logic.
     */
    class SslSession : public Session {
    public:
        explicit SslSession(asio::ip::tcp::socket socket, asio::ssl::context &ssl_context);
        ~SslSession() = default;

        asio::awaitable<void> start(HttpHandler *handler) override;
        asio::awaitable<void> write(const std::string &message) override;
        asio::awaitable<void> stream_write(const std::string &message, bool flush = true) override;
        void close() override;
        void clear_buffer() override;
        bool is_closed() const override;
        asio::ip::tcp::socket &get_socket() override { return ssl_stream_.next_layer(); }

        /**
         * @brief Get the underlying SSL stream.
         * @return Reference to SSL stream object
         */
        asio::ssl::stream<asio::ip::tcp::socket> &get_stream();

        std::array<char, 8192> &get_buffer() override { return buffer_; }
        const std::string &get_session_id() const override { return session_id_; }

    private:
        asio::ssl::stream<asio::ip::tcp::socket> ssl_stream_;///< SSL-wrapped socket
    };

}// namespace mcp::transport