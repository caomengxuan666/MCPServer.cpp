#pragma once

#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0601
#endif

#include "http_handler.h"
#include "transport_types.h"
#include <asio.hpp>
#include <memory>

namespace mcp::transport {

    /**
     * @brief HTTP transport implementation using plain TCP sockets.
     * Manages incoming HTTP connections and delegates to sessions.
     */
    class HttpTransport {
    public:
        explicit HttpTransport(const std::string &address, unsigned short port);
        ~HttpTransport();

        /**
         * @brief Start the HTTP server.
         * @param on_message Callback for processing received messages
         * @return True if startup successful
         */
        bool start(MessageCallback on_message);

        /**
         * @brief Stop the HTTP server.
         */
        void stop();

        std::array<char, 8192> &get_buffer() { return buffer_; }

        /**
         * @brief Get the underlying IO context.
         * @return Reference to IO context
         */
        asio::io_context &get_io_context() { return io_context_; }

    private:
        asio::awaitable<void> do_accept();                                     // Legacy placeholder, not used
        asio::io_context io_context_;                                          ///< IO context for async operations
        asio::ip::tcp::acceptor acceptor_;                                     ///< TCP acceptor for incoming connections
        std::unique_ptr<HttpHandler> handler_;                                 ///< HTTP request handler
        asio::executor_work_guard<asio::io_context::executor_type> work_guard_;///< Keep IO context running
        std::array<char, 8192> buffer_;                                        ///< Shared buffer
    };

}// namespace mcp::transport