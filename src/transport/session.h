#pragma once

// 确保 ASIO 在 Windows 上正确配置 WinSock
#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0601
#endif

#include "transport_types.h"
#include <asio.hpp>
#include <memory>

// 前置声明（解决与 HttpHandler 的循环依赖）
namespace mcp::transport {
    class HttpHandler;
}

namespace mcp::transport {

    /**
     * @brief manage single TCP session.
     * Only for IO,no business logic.
     */
    class Session : public std::enable_shared_from_this<Session> {
    public:
        explicit Session(asio::ip::tcp::socket socket);
        ~Session() = default;

        /**
         * @brief launch the session.
         */
        asio::awaitable<void> start(HttpHandler *handler);

        /**
         * @brief send data to the client.
         * @param message the message to send
         */
        asio::awaitable<void> write(const std::string &message);

        /**
         * @brief close the session.
         */
        void close();

        /**
         * @brief clear the session buffer.
         */
        void clear_buffer();

        /**
         * @brief check if the session is closed.
         */
        bool is_closed() const { return !socket_.is_open(); }

        /**
         * @brief get the underlying socket.
         */
        asio::ip::tcp::socket &get_socket() { return socket_; }

        void set_accept_header(const std::string &header) { accept_header_ = header; }
        const std::string &get_accept_header() const { return accept_header_; }

        std::array<char, 8192> &get_buffer() { return buffer_; }

    private:
        asio::ip::tcp::socket socket_;
        std::array<char, 8192> buffer_;
        bool closed_ = false;
        std::string accept_header_;
    };

}// namespace mcp::transport