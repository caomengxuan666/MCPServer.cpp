#include "http_transport.h"
#include "core/io_context_pool.hpp"
#include "core/logger.h"
#include "session.h"
#include "tcp_session.h"


using asio::use_awaitable;

namespace mcp::transport {

    /**
     * @brief Construct HTTP transport with specified address and port.
     * @param address IP address to bind to
     * @param port Port number to listen on
     */
    HttpTransport::HttpTransport(const std::string &address, unsigned short port)
        : io_context_(),
          acceptor_(io_context_, asio::ip::tcp::endpoint(asio::ip::make_address(address), port)),
          work_guard_(asio::make_work_guard(io_context_)) {
        MCP_INFO("HTTP Transport initialized on {}:{}", address, port);
    }

    /**
     * @brief Destructor - stops the transport.
     */
    HttpTransport::~HttpTransport() {
        stop();
    }

    /**
     * @brief Start accepting connections and processing requests.
     * @param on_message Message processing callback
     * @return True if successful
     */
    bool HttpTransport::start(MessageCallback on_message) {
        handler_ = std::make_unique<HttpHandler>(std::move(on_message));
        MCP_INFO("Streamable HTTP Transport started on {}:{}",
                 acceptor_.local_endpoint().address().to_string(),
                 acceptor_.local_endpoint().port());

        // Launch acceptor loop to handle incoming connections
        asio::co_spawn(io_context_, [this]() -> asio::awaitable<void> {
            try {
                while (true) {
                    // Accept new TCP connection
                    auto socket = co_await acceptor_.async_accept(use_awaitable);
                    MCP_DEBUG("HTTP client connected from {}:{}", 
                              socket.remote_endpoint().address().to_string(), 
                              socket.remote_endpoint().port());

                    // Get IO context from thread pool
                    auto& session_io_context = AsioIOServicePool::GetInstance()->GetIOService();
                    
                    // Create TCP session for the connection
                    auto session = std::make_shared<TcpSession>(std::move(socket));
                    
                    // Launch session handler in thread pool context
                    asio::co_spawn(session_io_context, 
                        [session, handler = handler_.get()]() -> asio::awaitable<void> {
                            co_await session->start(handler);
                            co_return;
                        }, 
                        asio::detached);
                }
            } catch (const std::exception& e) {
                MCP_ERROR("Error accepting HTTP connections: {}", e.what());
            } }, asio::detached);

        return true;
    }

    /**
     * @brief Legacy acceptor method (not used).
     */
    asio::awaitable<void> HttpTransport::do_accept() {
        co_return;
    }

    /**
     * @brief Stop the HTTP transport and clean up resources.
     */
    void HttpTransport::stop() {
        acceptor_.close();
        work_guard_.reset();
        io_context_.stop();
    }

}// namespace mcp::transport