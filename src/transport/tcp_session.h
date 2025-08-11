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
        void close() override;
        void clear_buffer() override;
        bool is_closed() const override;
        asio::ip::tcp::socket &get_socket() override { return socket_; }
        std::array<char, 8192> &get_buffer() override { return buffer_; }
        const std::string &get_session_id() const override { return session_id_; }

    private:
        asio::ip::tcp::socket socket_;///< Underlying TCP socket
    };

}// namespace mcp::transport