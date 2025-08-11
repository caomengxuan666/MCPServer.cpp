#include "config/config.hpp"// Configuration management using INI file
#include "core/logger.h"
#include "core/server.h"

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
        mcp::config::initialize_default_config();

        // Step 2: Load the full configuration from the INI file.
        auto config = mcp::config::GlobalConfig::load();

        // Step 4: Initialize the asynchronous logger using settings from the config.
        // Parameters include log file path, log level, maximum file size, and number of rotation files.
        mcp::core::initializeAsyncLogger(
                config.server.log_path,
                config.server.log_level,
                config.server.max_file_size,
                config.server.max_files);

        auto address = config.server.ip;


        // Set the log message format pattern. Use default if not specified in config.
        const std::string logPattern = config.server.log_pattern.empty()
                                               ? "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v"
                                               : config.server.log_pattern;

        mcp::core::MCPLogger::instance()->set_pattern(logPattern);

        // Log that the application has started successfully.
        MCP_INFO("MCPServer.cpp started with configuration from '{}'", mcp::config::get_config_file_path());

        // Step 3: Output all configuration values to the debug log for inspection.
        mcp::config::print_config(config);


        // Step 5: Build the MCP server instance using the configuration.
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
                              .build();                                                                           // Construct the server instance

        // Notify that the server is ready to accept connections.
        MCP_INFO("MCPServer.cpp is ready.");
        MCP_INFO("Send JSON-RPC messages via /mcp.");

        // Step 6: Start the server's main loop.
        // This call is expected to block until the server is stopped.
        server->run();

        return 0;// Normal exit

    } catch (const std::exception &e) {
        // Log any critical error that caused the server to fail at startup.
        MCP_CRITICAL("Critical error during startup: {}", e.what());
        return 1;// Indicate failure
    }
}