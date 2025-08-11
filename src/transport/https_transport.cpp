#include "https_transport.h"
#include "core/executable_path.h"
#include "core/io_context_pool.hpp"
#include "core/logger.h"
#include "ssl_session.h"
#include <asio/ssl/context.hpp>
#include <filesystem>
#include <fstream>

namespace mcp::transport {

    /**
     * @brief Construct HTTPS transport with SSL configuration.
     * @param address IP address to bind to
     * @param port Port number to listen on
     * @param cert_file Path to SSL certificate file
     * @param private_key_file Path to SSL private key file
     */
    HttpsTransport::HttpsTransport(const std::string &address, unsigned short port,
                                   const std::string &cert_file, const std::string &private_key_file, const std::string &dh_params_file)
        : io_context_(),
          acceptor_(io_context_, asio::ip::tcp::endpoint(asio::ip::make_address(address), port)),
          ssl_context_(asio::ssl::context::tlsv12_server),// Explicit TLS server mode
          work_guard_(asio::make_work_guard(io_context_)),
          is_running_(false) {

        // Resolve certificate paths relative to executable directory
        std::filesystem::path executable_dir(mcp::core::getExecutableDirectory());
        std::filesystem::path cert_path = executable_dir / cert_file;
        std::filesystem::path key_path = executable_dir / private_key_file;

        std::string cert_file_absolute = cert_path.string();
        std::string private_key_file_absolute = key_path.string();

        // Validate certificate files exist
        if (!std::filesystem::exists(cert_file_absolute)) {
            throw std::runtime_error("SSL certificate file not found: " + cert_file_absolute);
        }
        if (!std::filesystem::exists(private_key_file_absolute)) {
            throw std::runtime_error("SSL private key file not found: " + private_key_file_absolute);
        }

        // Log certificate details
        MCP_DEBUG("SSL certificate file: {}", cert_file_absolute);
        MCP_DEBUG("SSL private key file: {}", private_key_file_absolute);

        // Configure SSL security options
        ssl_context_.set_options(
                asio::ssl::context::default_workarounds |
                asio::ssl::context::no_sslv2 |
                asio::ssl::context::no_sslv3 |
                asio::ssl::context::no_tlsv1 |
                asio::ssl::context::no_tlsv1_1 |
                asio::ssl::context::single_dh_use);
        ssl_context_.set_verify_mode(asio::ssl::verify_none);
        ssl_context_.use_tmp_dh_file(dh_params_file);

        // Load and validate certificates
        load_certificates(cert_file_absolute, private_key_file_absolute);

        MCP_INFO("HTTPS Transport initialized with cert: {}, key: {}", cert_file_absolute, private_key_file_absolute);
    }

    /**
     * @brief Destructor - stops the transport.
     */
    HttpsTransport::~HttpsTransport() {
        stop();
    }

    /**
     * @brief Load and validate SSL certificates and private key.
     * @param cert_file Path to certificate file
     * @param key_file Path to private key file
     */
    void HttpsTransport::load_certificates(const std::string &cert_file, const std::string &key_file) {
        asio::error_code ec;

        // Validate and load certificate chain
        std::ifstream cert_stream(cert_file, std::ios::binary);
        if (!cert_stream.is_open()) {
            MCP_ERROR("Failed to open certificate file (invalid handle): {}", cert_file);
            throw std::runtime_error("Certificate file handle invalid");
        }
        (void) ssl_context_.use_certificate_chain_file(cert_file, ec);
        if (ec) {
            MCP_ERROR("Failed to load certificate: {} - {}", cert_file, ec.message());
            throw std::runtime_error("Certificate load failed");
        }
        cert_stream.close();

        // Validate and load private key
        std::ifstream key_stream(key_file, std::ios::binary);
        if (!key_stream.is_open()) {
            MCP_ERROR("Failed to open private key file (invalid handle): {}", key_file);
            throw std::runtime_error("Private key file handle invalid");
        }
        (void) ssl_context_.use_private_key_file(key_file, asio::ssl::context::pem, ec);
        if (ec) {
            MCP_ERROR("Failed to load private key: {} - {}", key_file, ec.message());
            throw std::runtime_error("Private key load failed");
        }
        key_stream.close();

        // Verify certificate and key match
        if (SSL_CTX_check_private_key(ssl_context_.native_handle()) != 1) {
            MCP_ERROR("Private key does not match certificate");
            throw std::runtime_error("Private key mismatch");
        }

        // Verify certificate is properly loaded
        if (!SSL_CTX_get0_certificate(ssl_context_.native_handle())) {
            MCP_ERROR("SSL context has no valid certificate after loading");
            throw std::runtime_error("No certificate in SSL context");
        }
    }

    /**
     * @brief Start accepting HTTPS connections and processing requests.
     * @param on_message Message processing callback
     * @return True if successful
     */
    bool HttpsTransport::start(MessageCallback on_message) {
        handler_ = std::make_unique<HttpHandler>(std::move(on_message));
        is_running_ = true;

        MCP_INFO("Streamable HTTPS Transport started on {}:{}",
                 acceptor_.local_endpoint().address().to_string(),
                 acceptor_.local_endpoint().port());

        // Launch main accept loop
        asio::co_spawn(io_context_, accept_loop(), asio::detached);

        // Run IO context in dedicated thread
        std::thread([this]() {
            try {
                io_context_.run();
            } catch (const std::exception &e) {
                MCP_ERROR("Error in HTTPS io_context: {}", e.what());
            }
        }).detach();

        return true;
    }

    /**
     * @brief Main loop for accepting and handling HTTPS connections.
     */
    asio::awaitable<void> HttpsTransport::accept_loop() {
        try {
            while (is_running_) {
                // Accept new TCP connection
                asio::ip::tcp::socket raw_socket(io_context_);
                co_await acceptor_.async_accept(raw_socket, asio::use_awaitable);

                // Validate socket state
                if (!raw_socket.is_open()) {
                    MCP_WARN("Accepted socket is already closed before session creation");
                    continue;
                }

                // Get client information
                std::string client_addr;
                uint16_t client_port = 0;
                try {
                    auto endpoint = raw_socket.remote_endpoint();
                    client_addr = endpoint.address().to_string();
                    client_port = endpoint.port();
                    MCP_DEBUG("HTTPS client connected from {}:{}", client_addr, client_port);
                } catch (const std::exception &e) {
                    MCP_WARN("Failed to get remote endpoint: {}", e.what());
                }

                // Transfer socket ownership
                auto session_socket = std::make_unique<asio::ip::tcp::socket>(std::move(raw_socket));
                if (!session_socket->is_open()) {
                    MCP_ERROR("Socket ownership transfer failed (client: {}:{})", client_addr, client_port);
                    continue;
                }

                // Create SSL session with socket and SSL context
                auto session = std::make_shared<SslSession>(
                        std::move(*session_socket),// Transfer socket ownership
                        ssl_context_);
                (void) session_socket.release();// Prevent double destruction

                // Final socket validation
                if (!session->get_stream().lowest_layer().is_open()) {
                    MCP_ERROR("Socket became invalid after session creation (client: {}:{})", client_addr, client_port);
                    continue;
                }

                // Launch session handler in thread pool
                asio::co_spawn(
                        AsioIOServicePool::GetInstance()->GetIOService(),
                        [session, handler = handler_.get()]() -> asio::awaitable<void> {
                            co_await session->start(handler);
                        },
                        asio::detached);
            }
        } catch (const std::exception &e) {
            MCP_ERROR("Accept loop exception: {}", e.what());
        }
    }

    /**
     * @brief Stop the HTTPS transport and clean up resources.
     */
    void HttpsTransport::stop() {
        is_running_ = false;
        acceptor_.close();
        work_guard_.reset();
        io_context_.stop();
    }

}// namespace mcp::transport