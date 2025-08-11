#pragma once

#include "transport_types.h"
#include <asio.hpp>
#include <memory>
#include <unordered_map>

namespace mcp::transport {
    class HttpHandler;
}// namespace mcp::transport

namespace mcp::transport {

    /**
     * @brief Base class for managing single TCP sessions.
     * Handles IO operations only, no business logic.
     */
    class Session : public std::enable_shared_from_this<Session> {
    public:
        virtual ~Session() = default;

        /**
         * @brief Launch the session.
         */
        virtual asio::awaitable<void> start(HttpHandler *handler) = 0;

        /**
         * @brief Send data to the client.
         * @param message The message to send
         */
        virtual asio::awaitable<void> write(const std::string &message) = 0;

        /**
         * @brief Close the session.
         */
        virtual void close() = 0;

        /**
         * @brief Clear the session buffer.
         */
        virtual void clear_buffer() = 0;

        /**
         * @brief Check if the session is closed.
         */
        virtual bool is_closed() const = 0;

        /**
         * @brief Get the underlying socket (implemented by subclasses).
         */
        virtual asio::ip::tcp::socket &get_socket() = 0;

        std::string generate_session_id() noexcept;

        void set_accept_header(const std::string &header) { accept_header_ = header; }
        const std::string &get_accept_header() const { return accept_header_; }

        virtual std::array<char, 8192> &get_buffer() = 0;

        virtual const std::string &get_session_id() const = 0;

        void set_headers(const std::unordered_map<std::string, std::string> &headers) { headers_ = headers; }
        const std::unordered_map<std::string, std::string> &get_headers() const { return headers_; }

    protected:
        /**
         * @brief Protected constructor (base class can't be instantiated directly).
         */
        Session() = default;

        std::string session_id_;                              ///< Unique session identifier
        std::array<char, 8192> buffer_;                       ///< Buffer for reading data
        bool closed_ = false;                                 ///< Flag indicating if session is closed
        std::string accept_header_;                           ///< Accepted content type header
        std::unordered_map<std::string, std::string> headers_;///< Request headers
    };

}// namespace mcp::transport