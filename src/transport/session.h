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
         * @brief Send data to the client in a streaming fashion.
         * @param message The message to send
         * @param flush Whether to flush the data immediately
         */
        virtual asio::awaitable<void> stream_write(const std::string &message, bool flush = true) {
            // Default implementation just calls regular write
            co_await write(message);
            co_return;
        }

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
         * @brief Set streaming state.
         * @param streaming Whether the session is in streaming mode
         */
        void set_streaming(bool streaming) { is_streaming_ = streaming; }

        /**
         * @brief Check if the session is in streaming mode.
         * @return True if the session is in streaming mode
         */
        bool is_streaming() const { return is_streaming_; }

        /**
         * @brief Get the underlying socket (implemented by subclasses).
         */
        virtual asio::ip::tcp::socket &get_socket() = 0;

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
        std::unordered_map<std::string, std::string> headers_;///< HTTP headers
        std::string accept_header_;                           ///< Accept header value
        bool is_streaming_ = false;
        bool closed_ = false;
    };

}// namespace mcp::transport