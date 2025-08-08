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

        //void set_session_id(const std::string &session_id) { session_id_ = session_id; }

        const std::string &get_session_id() const { return session_id_; }

        void set_headers(const std::unordered_map<std::string, std::string> &headers) { headers_ = headers; }
        const std::unordered_map<std::string, std::string> &get_headers() const { return headers_; }

    private:
        asio::ip::tcp::socket socket_;
        std::string session_id_;
        std::array<char, 8192> buffer_;
        bool closed_ = false;
        std::string accept_header_;
        std::unordered_map<std::string, std::string> headers_;
    };

}// namespace mcp::transport