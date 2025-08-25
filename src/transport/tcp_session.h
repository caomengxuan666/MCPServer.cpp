#pragma once

#include "session.h"

namespace mcp::transport {

    /**
     * @brief Regular TCP session implementation for HTTP connections.
     */
    class TcpSession : public Session {
    public:
        explicit TcpSession(asio::ip::tcp::socket socket);
        ~TcpSession() = default;

        asio::awaitable<void> start(HttpHandler *handler) override;
        asio::awaitable<void> write(const std::string &message) override;
        asio::awaitable<void> stream_write(const std::string &message, bool flush = true) override;

        asio::awaitable<void> write_chunk(const std::string &chunk);
        asio::awaitable<void> start_streaming(const std::string &content_type = "application/json");

        void close() override;
        void clear_buffer() override;
        bool is_closed() const override;
        asio::ip::tcp::socket &get_socket() override { return socket_; }
        std::array<char, 8192> &get_buffer() override { return buffer_; }
        const std::string &get_session_id() const override { return session_id_; }

    private:
        asio::ip::tcp::socket socket_;///< Underlying TCP socket
        bool streaming_ = false;      ///< Flag indicating if session is in streaming mode
    };

}// namespace mcp::transport