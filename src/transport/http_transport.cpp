#include "http_transport.h"
#include "core/io_context_pool.hpp"
#include "core/logger.h"
#include "session.h"
#include <filesystem>


using asio::use_awaitable;

namespace mcp::transport {

    HttpTransport::HttpTransport(const std::string &address, unsigned short port)
        : io_context_(),
          acceptor_(io_context_, asio::ip::tcp::endpoint(asio::ip::make_address(address), port)),
          work_guard_(asio::make_work_guard(io_context_)) {
        MCP_INFO("HTTP Transport initialized on {}:{}", address, port);
    }

    HttpTransport::~HttpTransport() {
        stop();
    }

    bool HttpTransport::start(MessageCallback on_message) {
        handler_ = std::make_unique<HttpHandler>(std::move(on_message));
        MCP_INFO("Streamable HTTP Transport started on {}:{}", 
                 acceptor_.local_endpoint().address().to_string(), 
                 acceptor_.local_endpoint().port());

        // launch the acceptor to handle incoming connections
        asio::co_spawn(io_context_, [this]() -> asio::awaitable<void> {
            try {
                while (true) {
                    auto socket = co_await acceptor_.async_accept(use_awaitable);
                    MCP_DEBUG("HTTP client connected from {}:{}", 
                              socket.remote_endpoint().address().to_string(), 
                              socket.remote_endpoint().port());

                    // get the io_context from the AsioIOServicePool
                    auto& session_io_context = AsioIOServicePool::GetInstance()->GetIOService();
                    
                    // create TCP session for each accepted socket
                    auto session = std::make_shared<TcpSession>(std::move(socket));
                    
                    // launch the session to handle requests
                    asio::co_spawn(session_io_context, [session, handler = handler_.get()]() -> asio::awaitable<void> {
                        co_await session->start(handler);
                        co_return;
                    }, asio::detached);
                }
            } catch (const std::exception& e) {
                MCP_ERROR("Error accepting connections: {}", e.what());
            } }, asio::detached);

        return true;
    }

    asio::awaitable<void> HttpTransport::do_accept() {
        co_return;
    }

    void HttpTransport::stop() {
        acceptor_.close();
        work_guard_.reset();
        io_context_.stop();
    }

}// namespace mcp::transport