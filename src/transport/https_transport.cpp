#include "https_transport.h"
#include "core/io_context_pool.hpp"
#include "core/logger.h"
#include "ssl_session.h"
#include <filesystem>


using asio::use_awaitable;

namespace mcp::transport {

    HttpsTransport::HttpsTransport(const std::string &address, unsigned short port,
                                   const std::string &cert_file, const std::string &private_key_file)
        : io_context_(),
          acceptor_(io_context_, asio::ip::tcp::endpoint(asio::ip::make_address(address), port)),
          ssl_context_(asio::ssl::context::tlsv12),
          work_guard_(asio::make_work_guard(io_context_)) {

        // Check if certificate and private key files exist
        if (!std::filesystem::exists(cert_file)) {
            throw std::runtime_error("SSL certificate file not found: " + cert_file);
        }
        if (!std::filesystem::exists(private_key_file)) {
            throw std::runtime_error("SSL private key file not found: " + private_key_file);
        }

        // Configure SSL context
        ssl_context_.set_options(
                asio::ssl::context::default_workarounds |
                asio::ssl::context::no_sslv2 |
                asio::ssl::context::no_sslv3 |
                asio::ssl::context::no_tlsv1 |
                asio::ssl::context::no_tlsv1_1 |
                asio::ssl::context::single_dh_use);

        // Load certificate and private key
        asio::error_code ec;
        ssl_context_.use_certificate_chain_file(cert_file, ec);
        if (ec) {
            MCP_ERROR("Failed to load certificate file: {} - Error: {}", cert_file, ec.message());
            throw std::runtime_error("Failed to load certificate file: " + cert_file + " - Error: " + ec.message());
        }

        ssl_context_.use_private_key_file(private_key_file, asio::ssl::context::pem, ec);
        if (ec) {
            MCP_ERROR("Failed to load private key file: {} - Error: {}", private_key_file, ec.message());
            throw std::runtime_error("Failed to load private key file: " + private_key_file + " - Error: " + ec.message());
        }

        MCP_INFO("HTTPS Transport initialized with cert: {}, key: {}", cert_file, private_key_file);
    }

    HttpsTransport::~HttpsTransport() {
        stop();
    }

    bool HttpsTransport::start(MessageCallback on_message) {
        handler_ = std::make_unique<HttpHandler>(std::move(on_message));
        MCP_INFO("Streamable HTTPS Transport started on {}:{}",
                 acceptor_.local_endpoint().address().to_string(),
                 acceptor_.local_endpoint().port());

        // launch the acceptor to handle incoming connections
        asio::co_spawn(io_context_, [this]() -> asio::awaitable<void> {
            try {
                while (true) {
                    auto socket = co_await acceptor_.async_accept(use_awaitable);
                    MCP_DEBUG("HTTPS client connected from {}:{}", 
                              socket.remote_endpoint().address().to_string(), 
                              socket.remote_endpoint().port());

                    // get the io_context from the AsioIOServicePool
                    auto& session_io_context = AsioIOServicePool::GetInstance()->GetIOService();
                    
                    // create SSL session for each accepted socket
                    auto session = std::make_shared<SslSession>(std::move(socket), ssl_context_);
                    
                    // launch the session to handle requests
                    asio::co_spawn(session_io_context, [session, handler = handler_.get()]() -> asio::awaitable<void> {
                        co_await session->start(handler);
                        co_return;
                    }, asio::detached);
                }
            } catch (const std::exception& e) {
                MCP_ERROR("Error accepting connections: {}", e.what());
            } }, asio::detached);

        // Run the io_context in a separate thread to avoid blocking
        std::thread([this]() {
            try {
                io_context_.run();
            } catch (const std::exception &e) {
                MCP_ERROR("Error in HTTPS io_context: {}", e.what());
            }
        }).detach();

        return true;
    }

    asio::awaitable<void> HttpsTransport::do_accept() {
        co_return;
    }

    void HttpsTransport::stop() {
        acceptor_.close();
        work_guard_.reset();
        io_context_.stop();
    }

}// namespace mcp::transport