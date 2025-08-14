#pragma once

#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0601
#endif

#include "base_transport.h"
#include "transport_types.h"
#include "Auth/AuthManager.hpp"
#include <asio.hpp>
#include <asio/ssl.hpp>
namespace mcp::transport {

    /**
     * @brief HTTPS transport implementation using SSL/TLS encryption.
     * Manages incoming HTTPS connections with encrypted IO.
     */
    class HttpsTransport : public BaseTransport {
    public:
        explicit HttpsTransport(const std::string &address, unsigned short port,
                                const std::string &cert_file, const std::string &private_key_file, const std::string &dh_params_file,
                                std::shared_ptr<AuthManagerBase> auth_manager = nullptr);
        ~HttpsTransport() override;

        /**
         * @brief Start the HTTPS server.
         * @param on_message Callback for processing received messages
         * @return True if startup successful
         */
        bool start(MessageCallback on_message) override;

        /**
         * @brief Stop the HTTPS server.
         */
        void stop() override;

    private:
        asio::awaitable<void> accept_loop();                                              ///< Main loop for accepting connections
        void load_certificates(const std::string &cert_file, const std::string &key_file);///< Load SSL certificates

        asio::awaitable<void> do_accept();                                     // Legacy placeholder, not used
        asio::ssl::context ssl_context_;                                       ///< SSL context with security configuration
        bool is_running_ = false;                                              ///< Transport running flag
        std::shared_ptr<AuthManagerBase> auth_manager_;                        ///< Authentication manager
    };

}// namespace mcp::transport