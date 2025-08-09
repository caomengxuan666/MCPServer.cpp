#pragma once


#include <unordered_map>
#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0601
#endif

#include "transport_types.h"
#include <asio.hpp>
#include <memory>


namespace mcp::transport {
    class HttpHandler;
    class SslSession;
}

namespace mcp::transport {

    /**
     * @brief manage single TCP session.
     * Only for IO,no business logic.
     */
    class Session : public std::enable_shared_from_this<Session> {
    public:
        virtual ~Session() = default;

        /**
         * @brief launch the session.
         */
        virtual asio::awaitable<void> start(HttpHandler *handler) = 0;

        /**
         * @brief send data to the client.
         * @param message the message to send
         */
        virtual asio::awaitable<void> write(const std::string &message) = 0;

        /**
         * @brief close the session.
         */
        virtual void close() = 0;

        /**
         * @brief clear the session buffer.
         */
        virtual void clear_buffer() = 0;

        /**
         * @brief check if the session is closed.
         */
        virtual bool is_closed() const = 0;

        /**
         * @brief get the underlying socket.
         */
        asio::ip::tcp::socket& get_socket() { return socket_; }

        void set_accept_header(const std::string &header) { accept_header_ = header; }
        const std::string &get_accept_header() const { return accept_header_; }

        virtual std::array<char, 8192> &get_buffer() = 0;

        virtual const std::string &get_session_id() const = 0;

        void set_headers(const std::unordered_map<std::string, std::string> &headers) { headers_ = headers; }
        const std::unordered_map<std::string, std::string> &get_headers() const { return headers_; }

    protected:
        explicit Session(asio::ip::tcp::socket socket);

        asio::ip::tcp::socket socket_;
        std::string session_id_;
        std::array<char, 8192> buffer_;
        bool closed_ = false;
        std::string accept_header_;
        std::unordered_map<std::string, std::string> headers_;
    };

    /**
     * @brief Regular TCP session implementation for HTTP connections
     */
    class TcpSession : public Session {
    public:
        explicit TcpSession(asio::ip::tcp::socket socket);
        ~TcpSession() = default;

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

        std::array<char, 8192> &get_buffer() override { return buffer_; }

        const std::string &get_session_id() const override { return session_id_; }

    private:
        // Using base class members: socket_, session_id_, buffer_, closed_, accept_header_, headers_
    };

}// namespace mcp::transport