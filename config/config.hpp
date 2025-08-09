#ifndef MCP_CONFIG_HPP
#define MCP_CONFIG_HPP

#include "core/logger.h"
#include "inicpp.hpp"
#include <filesystem>
#include <iostream>
#include <string>

namespace mcp {
    namespace config {

        // Configuration file path constant
        constexpr const char *CONFIG_FILE = "config.ini";

        /**
         * Server-specific configuration structure
         * Contains all network and service-related settings
         */
        struct ServerConfig {
            std::string ip;           // Server binding IP address
            std::string log_level;    // Logging severity level
            std::string log_path;     // Directory path for log files
            std::string log_pattern;  // Custom log format pattern
            std::string plugin_dir;   // Directory containing plugins
            std::string ssl_cert_file;// SSL certificate file path
            std::string ssl_key_file; // SSL private key file path
            size_t max_file_size;     // Maximum size per log file (bytes)
            size_t max_files;         // Maximum number of log files to retain
            unsigned short port;      // Legacy server listening port
            unsigned short http_port; // HTTP transport port (0 to disable)
            unsigned short https_port;// HTTPS transport port (0 to disable)
            bool enable_stdio;        // Enable stdio transport protocol
            bool enable_http;         // Enable HTTP transport protocol
            bool enable_https;        // Enable HTTPS transport protocol

            /**
             * Loads server configuration from INI file
             * @param ini Reference to IniManager instance
             * @return Populated ServerConfig structure
             */
            static ServerConfig load(inicpp::IniManager &ini) {
                try {
                    // Get server section
                    auto server_section = ini["server"];

                    ServerConfig config;

                    // String values
                    config.ip = server_section["ip"].String().empty() ? "127.0.0.1" : server_section["ip"].String();
                    config.log_level = server_section["log_level"].String().empty() ? "info" : server_section["log_level"].String();
                    config.log_path = server_section["log_path"].String().empty() ? "logs/mcp_server.log" : server_section["log_path"].String();
                    config.log_pattern = server_section["log_pattern"].String();
                    config.plugin_dir = server_section["plugin_dir"].String().empty() ? "plugins" : server_section["plugin_dir"].String();
                    config.ssl_cert_file = server_section["ssl_cert_file"].String().empty() ? "certs/server.crt" : server_section["ssl_cert_file"].String();
                    config.ssl_key_file = server_section["ssl_key_file"].String().empty() ? "certs/server.key" : server_section["ssl_key_file"].String();

                    // Numeric values with explicit conversion
                    config.max_file_size = server_section["max_file_size"].String().empty() ? 10485760 : static_cast<size_t>(server_section["max_file_size"]);
                    config.max_files = server_section["max_files"].String().empty() ? 10 : static_cast<size_t>(server_section["max_files"]);
                    config.port = server_section["port"].String().empty() ? 6666 : static_cast<unsigned short>(server_section["port"]);
                    config.http_port = server_section["http_port"].String().empty() ? 6666 : static_cast<unsigned short>(server_section["http_port"]);
                    config.https_port = server_section["https_port"].String().empty() ? 6667 : static_cast<unsigned short>(server_section["https_port"]);

                    // Boolean values
                    config.enable_stdio = server_section["enable_stdio"].String().empty() ? true : static_cast<bool>(server_section["enable_stdio"]);
                    config.enable_http = server_section["enable_http"].String().empty() ? false : static_cast<bool>(server_section["enable_http"]);
                    config.enable_https = server_section["enable_https"].String().empty() ? false : static_cast<bool>(server_section["enable_https"]);

                    return config;
                } catch (const std::exception &e) {
                    MCP_ERROR("Failed to load server config: {}", e.what());
                    std::cerr << "Failed to load server config: " << e.what() << std::endl;
                    throw;
                }
            }
        };

        /**
         * Global application configuration structure
         * Contains top-level configuration and nested server settings
         */
        struct GlobalConfig {
            std::string title;  // Configuration file title/description
            ServerConfig server;// Nested server configuration

            /**
             * Loads complete configuration from INI file
             * @return Populated GlobalConfig structure
             */
            static GlobalConfig load() {
                try {
                    inicpp::IniManager ini(CONFIG_FILE);
                    std::cout << "Loaded configuration file: " << CONFIG_FILE << std::endl;

                    GlobalConfig config;
                    config.title = ini[""]["title"].String().empty() ? "MCP Server Configuration" : ini[""]["title"].String();
                    config.server = ServerConfig::load(ini);

                    return config;
                } catch (const std::exception &e) {
                    MCP_ERROR("Failed to load global config: {}", e.what());
                    throw;
                }
            }
        };

        /**
         * Initializes default configuration file if it doesn't exist or is empty
         * Creates standard sections and default values for first-time setup
         */
        inline void initialize_default_config() {
            try {
                // Check if file exists and has content using filesystem
                bool file_valid = false;
                if (std::filesystem::exists(CONFIG_FILE)) {
                    if (std::filesystem::file_size(CONFIG_FILE) > 0) {
                        file_valid = true;

                    } else {
                        MCP_WARN("Configuration file is empty - recreating: {}", CONFIG_FILE);
                        std::cout << "Configuration file is empty - recreating: " << CONFIG_FILE << std::endl;
                    }
                }

                if (file_valid) {
                    return;
                }

                // Create new configuration file with defaults
                inicpp::IniManager ini(CONFIG_FILE);
                std::cout << "Creating default configuration file: " << CONFIG_FILE << std::endl;
                // Server section configuration
                ini.set("server", "ip", "0.0.0.0");
                ini.set("server", "port", 6666);
                ini.set("server", "http_port", 6666);
                ini.set("server", "https_port", 6667);
                ini.set("server", "log_level", "trace");
                ini.set("server", "log_path", "logs/mcp_server.log");
                ini.set("server", "max_file_size", 10485760);// 10MB
                ini.set("server", "max_files", 10);
                ini.set("server", "plugin_dir", "plugins");
                ini.set("server", "enable_stdio", 1);
                ini.set("server", "enable_http", 1);
                ini.set("server", "enable_https", 0);
                ini.set("server", "ssl_cert_file", "certs/server.crt");
                ini.set("server", "ssl_key_file", "certs/server.key");

                // Add comments for server section
                ini.setComment("server", "ip", "IP address the server binds to");
                ini.setComment("server", "port", "Legacy network port for incoming connections");
                ini.setComment("server", "http_port", "HTTP transport port (set to 0 to disable HTTP)");
                ini.setComment("server", "https_port", "HTTPS transport port (set to 0 to disable HTTPS)");
                ini.setComment("server", "log_level", "Logging severity (trace, debug, info, warn, error)");
                ini.setComment("server", "log_path", "Filesystem path for log storage");
                ini.setComment("server", "max_file_size", "Maximum size per log file in bytes");
                ini.setComment("server", "max_files", "Maximum number of rotated log files");
                ini.setComment("server", "plugin_dir", "Directory containing plugin modules");
                ini.setComment("server", "enable_stdio", "Enable stdio transport (1=enable, 0=disable)");
                ini.setComment("server", "enable_http", "Enable HTTP transport (1=enable, 0=disable)");
                ini.setComment("server", "enable_https", "Enable HTTPS transport (1=enable, 0=disable)");
                ini.setComment("server", "ssl_cert_file", "SSL certificate file path (required for HTTPS)");
                ini.setComment("server", "ssl_key_file", "SSL private key file path (required for HTTPS)");

                // Root section configuration
                ini.set("title", "MCP Server Configuration");
                ini.setComment("title", "Auto-generated configuration file for MCP Server");

                // Force configuration write
                ini.parse();
                std::cout << "Successfully created default configuration" << std::endl;
            } catch (const std::exception &) {
                std::cerr << "Configuration initialization failed" << std::endl;
                throw;
            }
        }

        /**
         * Prints configuration values to debug log
         * @param config Reference to GlobalConfig instance to display
         */
        inline void print_config(const GlobalConfig &config) {
            MCP_DEBUG("===== MCP Configuration =====");
            MCP_DEBUG("Title: {}", config.title);
            MCP_DEBUG("Server IP: {}", config.server.ip);
            MCP_DEBUG("Server Port: {}", config.server.port);
            MCP_DEBUG("HTTP Port: {}", config.server.http_port);
            MCP_DEBUG("HTTPS Port: {}", config.server.https_port);
            MCP_DEBUG("Log Level: {}", config.server.log_level);
            MCP_DEBUG("Log Path: {}", config.server.log_path);
            MCP_DEBUG("Log Pattern: {}", config.server.log_pattern);
            MCP_DEBUG("Plugin Directory: {}", config.server.plugin_dir);
            MCP_DEBUG("SSL Certificate File: {}", config.server.ssl_cert_file);
            MCP_DEBUG("SSL Key File: {}", config.server.ssl_key_file);
            MCP_DEBUG("Max Log File Size: {}", config.server.max_file_size);
            MCP_DEBUG("Max Log Files: {}", config.server.max_files);
            MCP_DEBUG("Stdio Transport Enabled: {}", config.server.enable_stdio ? "Yes" : "No");
            MCP_DEBUG("HTTP Transport Enabled: {}", config.server.enable_http ? "Yes" : "No");
            MCP_DEBUG("HTTPS Transport Enabled: {}", config.server.enable_https ? "Yes" : "No");
            MCP_DEBUG("=============================");
        }

        /**
         * Lists all configuration sections and key-value pairs
         * Useful for debugging and verification of configuration structure
         */
        inline void list_config_sections() {
            try {
                inicpp::IniManager ini(CONFIG_FILE);
                MCP_DEBUG("Listing all configuration sections from: {}", CONFIG_FILE);

                for (const auto &section: ini.sectionsList()) {
                    MCP_DEBUG("\n[{}]", section);
                    for (const auto &[key, value]: ini.sectionMap(section)) {
                        MCP_DEBUG("  {} = {}", key, value);
                    }
                }
            } catch (const std::exception &e) {
                MCP_ERROR("Failed to list configuration sections: {}", e.what());
                throw;
            }
        }

    }// namespace config
}// namespace mcp

#endif// MCP_CONFIG_HPP