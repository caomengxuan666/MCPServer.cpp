#pragma once

#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0601
#endif

#include "http_handler.h"
#include "transport_types.h"
#include <asio.hpp>
#include <memory>

namespace mcp::transport {

    class HttpTransport {
    public:
        explicit HttpTransport(const std::string &address, unsigned short port);
        ~HttpTransport();

        /**
         * @brief launch the HTTP server.
         * @param on_message JSON-RPC message callback, called when a new message is received.
         */
        bool start(MessageCallback on_message);

        /**
         * @brief stop the HTTP server.
         */
        void stop();

        std::array<char, 8192> &get_buffer() { return buffer_; }

        /**
         * @brief get the underlying io_context for custom operations.
         */
        asio::io_context &get_io_context() { return io_context_; }

    private:
        asio::awaitable<void> do_accept();
        asio::io_context io_context_;
        asio::ip::tcp::acceptor acceptor_;
        std::unique_ptr<HttpHandler> handler_;
        asio::executor_work_guard<asio::io_context::executor_type> work_guard_;
        std::array<char, 8192> buffer_;
    };

}// namespace mcp::transport