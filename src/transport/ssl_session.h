#pragma once

#include <unordered_map>
#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0601
#endif

#include "session.h"
#include "transport_types.h"
#include <asio.hpp>
#include <asio/ssl.hpp>
#include <memory>



namespace mcp::transport {
    class HttpHandler;
}

namespace mcp::transport {

    /**
     * @brief manage single SSL TCP session.
     * Only for IO,no business logic.
     */
    class SslSession : public Session {
    public:
        explicit SslSession(asio::ip::tcp::socket socket, asio::ssl::context &ssl_context);
        ~SslSession() = default;

        /**
         * @brief launch the session.
         */
        asio::awaitable<void> start(HttpHandler *handler) override;

        /**
         * @brief send data to the client.
         * @param message the message to send
         */
        asio::awaitable<void> write(const std::string &message) override;

        /**
         * @brief close the session.
         */
        void close() override;

        /**
         * @brief clear the session buffer.
         */
        void clear_buffer() override;

        /**
         * @brief check if the session is closed.
         */
        bool is_closed() const override;

        /**
         * @brief get the underlying socket.
         */
        asio::ssl::stream<asio::ip::tcp::socket> &get_stream();

        void set_accept_header(const std::string &header) { accept_header_ = header; }
        const std::string &get_accept_header() const { return accept_header_; }

        std::array<char, 8192> &get_buffer() override { return buffer_; }

        const std::string &get_session_id() const override { return session_id_; }

        void set_headers(const std::unordered_map<std::string, std::string> &headers) { headers_ = headers; }
        const std::unordered_map<std::string, std::string> &get_headers() const { return headers_; }

    private:
        asio::ssl::stream<asio::ip::tcp::socket> ssl_stream_;
        std::string session_id_;
        std::array<char, 8192> buffer_;
        bool closed_ = false;
        std::string accept_header_;
        std::unordered_map<std::string, std::string> headers_;
    };

}// namespace mcp::transport