#pragma once

#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0601
#endif

#include "http_handler.h"
#include "transport_types.h"
#include <asio.hpp>
#include <asio/ssl.hpp>
#include <memory>

namespace mcp::transport {

    /**
     * @brief HTTPS transport implementation using SSL/TLS encryption.
     * Manages incoming HTTPS connections with encrypted IO.
     */
    class HttpsTransport {
    public:
        explicit HttpsTransport(const std::string &address, unsigned short port,
                                const std::string &cert_file, const std::string &private_key_file, const std::string &dh_params_file);
        ~HttpsTransport();

        /**
         * @brief Start the HTTPS server.
         * @param on_message Callback for processing received messages
         * @return True if startup successful
         */
        bool start(MessageCallback on_message);

        /**
         * @brief Stop the HTTPS server.
         */
        void stop();

        std::array<char, 8192> &get_buffer() { return buffer_; }

        /**
         * @brief Get the underlying IO context.
         * @return Reference to IO context
         */
        asio::io_context &get_io_context() { return io_context_; }

    private:
        asio::awaitable<void> accept_loop();                                              ///< Main loop for accepting connections
        void load_certificates(const std::string &cert_file, const std::string &key_file);///< Load SSL certificates

        asio::awaitable<void> do_accept();                                     // Legacy placeholder, not used
        asio::io_context io_context_;                                          ///< IO context for async operations
        asio::ip::tcp::acceptor acceptor_;                                     ///< TCP acceptor for incoming connections
        asio::ssl::context ssl_context_;                                       ///< SSL context with security configuration
        std::unique_ptr<HttpHandler> handler_;                                 ///< HTTP request handler
        asio::executor_work_guard<asio::io_context::executor_type> work_guard_;///< Keep IO context running
        std::array<char, 8192> buffer_;                                        ///< Shared buffer
        bool is_running_ = false;                                              ///< Transport running flag
    };

}// namespace mcp::transport