#include "http_transport.h"
#include "base_transport.h"
#include "core/io_context_pool.hpp"
#include "core/logger.h"
#include "http_handler.h"
#include "session.h"
#include "tcp_session.h"


using asio::use_awaitable;

namespace mcp::transport {

    /**
     * @brief Construct HTTP transport with specified address and port.
     * @param address IP address to bind to
     * @param port Port number to listen on
     * @param auth_manager Authentication manager
     */
    HttpTransport::HttpTransport(const std::string &address, unsigned short port, std::shared_ptr<AuthManagerBase> auth_manager)
        : BaseTransport(address, port),
          is_running_(false),
          auth_manager_(auth_manager) {
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
        if (auth_manager_) {
            MCP_DEBUG("HTTP transport auth manager initialized with type: {}", auth_manager_->type());
        } else {
            MCP_DEBUG("HTTP transport auth manager not initialized (auth disabled)");
        }

        handler_ = std::make_unique<HttpHandler>(std::move(on_message), auth_manager_);
        is_running_ = true;
        MCP_INFO("Streamable HTTP Transport started on {}:{}",
                 acceptor_.local_endpoint().address().to_string(),
                 acceptor_.local_endpoint().port());

        // Launch acceptor loop to handle incoming connections
        asio::co_spawn(get_io_context(), [this]() -> asio::awaitable<void> {
            try {
                while (is_running_) {
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

        // Run IO context in dedicated thread
        std::thread([this]() {
            try {
                get_io_context().run();
            } catch (const std::exception &e) {
                MCP_ERROR("Error in HTTP io_context: {}", e.what());
            }
        }).detach();

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
        is_running_ = false;
        acceptor_.close();
        work_guard_.reset();
        get_io_context().stop();
    }

}// namespace mcp::transport