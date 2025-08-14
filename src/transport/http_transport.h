#pragma once

#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0601
#endif

#include "base_transport.h"
#include "transport_types.h"
#include "Auth/AuthManager.hpp"
#include <asio.hpp>
#include <memory>

namespace mcp::transport {

    /**
     * @brief HTTP transport implementation using plain TCP sockets.
     * Manages incoming HTTP connections and delegates to sessions.
     */
    class HttpTransport : public BaseTransport {
    public:
        explicit HttpTransport(const std::string &address, unsigned short port, std::shared_ptr<AuthManagerBase> auth_manager = nullptr);
        ~HttpTransport() override;

        /**
         * @brief Start the HTTP server.
         * @param on_message Callback for processing received messages
         * @return True if startup successful
         */
        bool start(MessageCallback on_message) override;

        /**
         * @brief Stop the HTTP server.
         */
        void stop() override;

    private:
        asio::awaitable<void> do_accept();                                     // Legacy placeholder, not used
        bool is_running_ = false;                                              ///< Transport running flag
        std::shared_ptr<AuthManagerBase> auth_manager_;                        ///< Authentication manager
    };

}// namespace mcp::transport