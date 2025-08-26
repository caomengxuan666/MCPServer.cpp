#include "Auth/AuthManager.hpp"
#include "business/python_runtime_manager.h"
#include "config/config.hpp"// Configuration management using INI file
#include "config/config_observer.hpp"
#include "core/logger.h"
#include "core/server.h"
#include "metrics/metrics_manager.h"
#include "metrics/performance_metrics.h"
#include "metrics/rate_limiter.h"
#include "utils/auth_utils.h"
#include <asio/io_context.hpp>
#include <asio/signal_set.hpp>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <thread>

/**
 * Entry point of the MCP server application.
 * This function initializes the configuration, sets up logging, builds the server instance,
 * and starts the main event loop.
 *
 * @return 0 on successful execution, 1 if an exception occurs.
 */
int main() {
    try {
        // Step 1: Ensure the default configuration file exists.
        // If the config file is missing or empty, create one with safe defaults.
        mcp::config::initialize_config_system(mcp::config::ConfigMode::DYNAMIC);

        // Step 2: Load the full configuration from the INI file.
        auto config = mcp::config::get_current_config();

        // Step 3: Initialize the asynchronous logger using settings from the config.
        // Parameters include log file path, log level, maximum file size, and number of rotation files.
        mcp::core::initializeAsyncLogger(
                config.server.log_path,
                config.server.log_level,
                config.server.max_file_size,
                config.server.max_files);
        MCP_INFO("Starting MCP Server with configuration: {}", mcp::config::get_config_file_path());

        // Print configuration
        MCP_INFO("Server Configuration:");
        MCP_INFO("  IP: {}", config.server.ip);
        MCP_INFO("  Port: {}", config.server.port);
        MCP_INFO("  HTTP Port: {}", config.server.http_port);
        MCP_INFO("  HTTPS Port: {}", config.server.https_port);
        MCP_INFO("  Plugin Directory: {}", config.server.plugin_dir);
        MCP_INFO("  Log Level: {}", config.server.log_level);
        MCP_INFO("  Log Path: {}", config.server.log_path);
        auto address = config.server.ip;

        // Step 4: Set up PythonRuntimeManager with initial config and observer
        auto &python_runtime_manager = mcp::business::PythonRuntimeManager::getInstance();
        auto python_env_config = mcp::business::PythonRuntimeManager::createEnvironmentConfig(
                config.python_env.default_env,
                config.python_env.uv_venv_path);
        python_runtime_manager.setEnvironmentConfig(std::move(python_env_config));

        MCP_INFO("Python Environment Initialized:");
        MCP_INFO("  Default: {}", config.python_env.default_env);
        MCP_INFO("  UV Venv: {}", config.python_env.uv_venv_path);

        auto python_observer = std::make_unique<mcp::business::PythonConfigObserver>(python_runtime_manager);

        // Register observer — now it will be notified on every config reload
        mcp::config::g_config_loader->addObserver(python_observer.get());


        // Step 5: Initialize the metrics manager.
        // You can set a metrics callback here
        // it would be called every time a request is completed
        mcp::metrics::MetricsManager::getInstance()->set_performance_callback([](
                                                                                      const mcp::metrics::TrackedHttpRequest &request,
                                                                                      const mcp::metrics::PerformanceMetrics &metrics,
                                                                                      const std::string &session_id) {
            MCP_DEBUG("Performance - Session: {}, Method: {}, Target: {}, Duration: {:.2f}ms, RPS: {:.2f}",
                      session_id,
                      request.method,
                      request.target,
                      metrics.duration_ms(),
                      metrics.requests_per_second());
        });

        // Initialize the rate limiter
        auto rate_limiter = mcp::metrics::RateLimiter::getInstance();
        mcp::metrics::RateLimitConfig rate_limit_config;
        rate_limit_config.max_requests_per_second = config.server.max_requests_per_second;
        rate_limit_config.max_concurrent_requests = config.server.max_concurrent_requests;
        rate_limit_config.max_request_size = config.server.max_request_size;
        rate_limit_config.max_response_size = config.server.max_response_size;
        rate_limiter->set_config(rate_limit_config);

        rate_limiter->set_rate_limit_callback([](
                                                      const std::string &session_id,
                                                      mcp::metrics::RateLimitDecision decision) {
            switch (decision) {
                case mcp::metrics::RateLimitDecision::ALLOW:
                    MCP_DEBUG("Request allowed - Session: {}", session_id);
                    break;
                case mcp::metrics::RateLimitDecision::RATE_LIMITED:
                    MCP_WARN("Request rate limited - Session: {}", session_id);
                    break;
                case mcp::metrics::RateLimitDecision::TOO_LARGE:
                    MCP_WARN("Request too large - Session: {}", session_id);
                    break;
            }
        });

        // Create auth manager if auth is enabled
        std::shared_ptr<AuthManagerBase> auth_manager = nullptr;
        if (config.server.enable_auth) {
            MCP_DEBUG("Authentication is enabled with type: {}", config.server.auth_type);

            // Load auth keys from file
            auto auth_keys = mcp::utils::load_auth_keys_from_file(config.server.auth_env_file);

            if (auth_keys.empty()) {
                MCP_WARN("Authentication is enabled but no keys were loaded from {}", config.server.auth_env_file);
            } else {
                if (config.server.auth_type == "X-API-Key") {
                    auth_manager = std::make_shared<AuthManagerXApi>(auth_keys);
                } else if (config.server.auth_type == "Bearer") {
                    auth_manager = std::make_shared<AuthManagerBearer>(auth_keys);
                } else {
                    MCP_WARN("Unknown authentication type: {}, using X-API-Key as default", config.server.auth_type);
                    auth_manager = std::make_shared<AuthManagerXApi>(auth_keys);
                }
            }
        } else {
            MCP_DEBUG("Authentication is disabled");
        }

        // Step 6: Build the MCP server instance using the configuration.
        // Configure transport layers and plugin directory based on settings.
        auto server = mcp::core::MCPserver::Builder{}
                              .with_plugin_path(config.server.plugin_dir)      // Load plugins from specified directory
                              .with_address(address)                           // Set server address
                              .with_port(config.server.http_port)              // Set HTTP port
                              .enableHttpTransport(config.server.enable_http)  // Enable HTTP transport if configured
                              .enableStdioTransport(config.server.enable_stdio)// Enable stdio transport if configured
                              .enableHttpsTransport(config.server.enable_https)// Enable HTTPS transport if configured
                              .with_https_port(config.server.https_port)       // Set HTTPS port
                              .with_ssl_certificates(config.server.ssl_cert_file,
                                                     config.server.ssl_key_file, config.server.ssl_dh_params_file)// Set SSL certificate files
                              .with_auth_manager(auth_manager)                                                    // Set authentication manager
                              .build();                                                                           // Construct the server instance

        // Setup signal handler for graceful shutdown
        asio::io_context io_context;
        asio::signal_set signals(io_context, SIGINT, SIGTERM);

        signals.async_wait([&](const asio::error_code &error, int signal_number) {
            if (!error) {
                MCP_INFO("Received signal {}, initiating graceful shutdown...", signal_number);
                std::quick_exit(0);
            }
        });

        // Run the signal handler in a separate thread
        std::thread signal_thread([&io_context]() {
            io_context.run();
        });

        // Notify that the server is ready to accept connections.
        MCP_INFO("MCPServer.cpp is ready.");
        MCP_INFO("Send JSON-RPC messages via /mcp.");

        // Step 7.: Start the server's main loop.
        // This call is expected to block until the server is stopped.
        server->run();

        // Stop the signal handler and wait for the thread to finish
        io_context.stop();
        if (signal_thread.joinable()) {
            signal_thread.join();
        }

        MCP_INFO("Server shutdown complete.");
        return 0;// Normal exit

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        MCP_ERROR("Server error: {}", e.what());
        return -1;
    }
}
