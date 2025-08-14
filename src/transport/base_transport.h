#pragma once

#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0601
#endif

#include "transport_types.h"
#include <asio.hpp>
#include <memory>
#include <array>
#include "http_handler.h"

namespace mcp::transport {

    // Forward declaration
    //using MessageCallback = std::function<void(const std::string&)>;

    /**
     * @brief Base transport class that encapsulates common functionality
     * between HTTP and HTTPS transports.
     */
    class BaseTransport {
    public:
        virtual ~BaseTransport() = default;

        /**
         * @brief Start the transport server.
         * @param on_message Callback for processing received messages
         * @return True if startup successful
         */
        virtual bool start(MessageCallback on_message) = 0;

        /**
         * @brief Stop the transport server.
         */
        virtual void stop() = 0;

        std::array<char, 8192> &get_buffer() { return buffer_; }

        /**
         * @brief Get the underlying IO context.
         * @return Reference to IO context
         */
        asio::io_context &get_io_context() { return io_context_; }

    protected:
        BaseTransport(const std::string& address, unsigned short port);

        asio::io_context io_context_;                                          ///< IO context for async operations
        asio::ip::tcp::acceptor acceptor_;                                     ///< TCP acceptor for incoming connections
        std::unique_ptr<HttpHandler> handler_;                                 ///< HTTP request handler
        asio::executor_work_guard<asio::io_context::executor_type> work_guard_{asio::make_work_guard(io_context_)}; ///< Keep IO context running
        std::array<char, 8192> buffer_;                                        ///< Shared buffer
    };

}// namespace mcp::transport