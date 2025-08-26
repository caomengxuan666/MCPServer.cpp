#ifndef MCP_CONFIG_HPP
#define MCP_CONFIG_HPP

#include "config_observer.hpp"
#include "core/executable_path.h"
#include "core/logger.h"
#include "inicpp.hpp"
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>


namespace mcp {
    namespace config {

        constexpr const char *CONFIG_FILE = "config.ini";

        inline std::string g_config_file_path;

        // Use executable directory to construct config file path
        inline std::string get_config_file_path() {
            static std::string config_file_path = []() {
                std::filesystem::path exe_dir(mcp::core::getExecutableDirectory());
                return (exe_dir / CONFIG_FILE).string();
            }();
            return config_file_path;
        }

        inline void set_config_file_path(const std::string &path) {
            g_config_file_path = path;
        }

        inline std::string get_default_config_path() {
            if (!g_config_file_path.empty()) {
                return g_config_file_path;
            }
            return get_config_file_path();
        }

        enum class ConfigMode {
            NONE,  // Use default settings without file
            STATIC,// Load from static file once
            DYNAMIC// Load and monitor for changes
        };

        /**
 * Server-specific configuration structure
 */
        struct ServerConfig {
            std::string ip;
            std::string log_level;
            std::string log_path;
            std::string log_pattern;
            std::string plugin_dir;
            std::string ssl_cert_file;
            std::string ssl_key_file;
            std::string ssl_dh_params_file;
            std::string auth_type;
            std::string auth_env_file;
            size_t max_file_size;
            size_t max_files;
            unsigned short port;
            unsigned short http_port;
            unsigned short https_port;
            bool enable_stdio;
            bool enable_http;
            bool enable_https;
            bool enable_auth;
            size_t max_requests_per_second;
            size_t max_concurrent_requests;
            size_t max_request_size;
            size_t max_response_size;

            static ServerConfig load(inicpp::IniManager &ini) {
                try {
                    auto server_section = ini["server"];
                    ServerConfig config;

                    config.ip = server_section["ip"].String().empty() ? "127.0.0.1" : server_section["ip"].String();
                    config.log_level = server_section["log_level"].String().empty() ? "info" : server_section["log_level"].String();
                    config.log_path = server_section["log_path"].String().empty() ? "logs/mcp_server.log" : server_section["log_path"].String();
                    config.log_pattern = server_section["log_pattern"].String();
                    config.plugin_dir = server_section["plugin_dir"].String().empty() ? "plugins" : server_section["plugin_dir"].String();
                    config.ssl_cert_file = server_section["ssl_cert_file"].String().empty() ? "certs/server.crt" : server_section["ssl_cert_file"].String();
                    config.ssl_key_file = server_section["ssl_key_file"].String().empty() ? "certs/server.key" : server_section["ssl_key_file"].String();
                    config.ssl_dh_params_file = server_section["ssl_dh_params_file"].String().empty() ? "certs/dh2048.pem" : server_section["ssl_dh_params_file"].String();
                    config.auth_type = server_section["auth_type"].String().empty() ? "X-API-Key" : server_section["auth_type"].String();
                    config.auth_env_file = server_section["auth_env_file"].String().empty() ? ".env.auth" : server_section["auth_env_file"].String();

                    config.max_file_size = server_section["max_file_size"].String().empty() ? 10485760 : static_cast<size_t>(server_section["max_file_size"]);
                    config.max_files = server_section["max_files"].String().empty() ? 10 : static_cast<size_t>(server_section["max_files"]);
                    config.port = server_section["port"].String().empty() ? 6666 : static_cast<unsigned short>(server_section["port"]);
                    config.http_port = server_section["http_port"].String().empty() ? 6666 : static_cast<unsigned short>(server_section["http_port"]);
                    config.https_port = server_section["https_port"].String().empty() ? 6667 : static_cast<unsigned short>(server_section["https_port"]);

                    config.max_requests_per_second = server_section["max_requests_per_second"].String().empty() ? 100 : static_cast<size_t>(server_section["max_requests_per_second"]);
                    config.max_concurrent_requests = server_section["max_concurrent_requests"].String().empty() ? 1000 : static_cast<size_t>(server_section["max_concurrent_requests"]);
                    config.max_request_size = server_section["max_request_size"].String().empty() ? 1024 * 1024 : static_cast<size_t>(server_section["max_request_size"]);
                    config.max_response_size = server_section["max_response_size"].String().empty() ? 10 * 1024 * 1024 : static_cast<size_t>(server_section["max_response_size"]);

                    config.enable_stdio = server_section["enable_stdio"].String().empty() ? true : static_cast<bool>(server_section["enable_stdio"]);
                    config.enable_http = server_section["enable_http"].String().empty() ? false : static_cast<bool>(server_section["enable_http"]);
                    config.enable_https = server_section["enable_https"].String().empty() ? false : static_cast<bool>(server_section["enable_https"]);
                    config.enable_auth = server_section["enable_auth"].String().empty() ? false : static_cast<bool>(server_section["enable_auth"]);

                    return config;
                } catch (const std::exception &e) {
                    MCP_ERROR("Failed to load server config: {}", e.what());
                    throw;
                }
            }
        };

        /**
 * PluginHub configuration
 */
        struct PluginHubConfig {
            std::string plugin_server_baseurl;
            unsigned short plugin_server_port;
            std::string latest_fetch_route;
            std::string download_route;
            std::string plugin_install_dir;
            std::string plugin_enable_dir;
            std::string tools_install_dir;
            std::string tools_enable_dir;

            static PluginHubConfig load(inicpp::IniManager &ini) {
                try {
                    PluginHubConfig config;
                    auto section = ini["plugin_hub"];
                    config.plugin_server_baseurl = section["plugin_server_baseurl"].String().empty() ? "http://47.120.50.122" : section["plugin_server_baseurl"].String();
                    config.plugin_server_port = section["plugin_server_port"].String().empty() ? 6680 : static_cast<unsigned short>(section["plugin_server_port"]);
                    config.latest_fetch_route = section["latest_fetch_route"].String().empty() ? "/self/latest/info" : section["latest_fetch_route"].String();
                    config.download_route = section["download_route"].String().empty() ? "/self/latest/download" : section["download_route"].String();
                    config.plugin_install_dir = section["plugin_install_dir"].String().empty() ? "plugins_install" : section["plugin_install_dir"].String();
                    config.plugin_enable_dir = section["plugin_enable_dir"].String().empty() ? "plugins" : section["plugin_enable_dir"].String();
                    config.tools_install_dir = section["tools_install_dir"].String().empty() ? "plugins_install" : section["tools_install_dir"].String();
                    config.tools_enable_dir = section["tools_enable_dir"].String().empty() ? "configs" : section["tools_enable_dir"].String();
                    return config;
                } catch (const std::exception &e) {
                    MCP_ERROR("Failed to load plugin hub config: {}", e.what());
                    throw;
                }
            }
        };

        /**
 * Python environment configuration
 */
        struct PythonEnvConfig {
            std::string default_env;
            std::string conda_prefix;
            std::string uv_venv_path;

            static PythonEnvConfig load(inicpp::IniManager &ini) {
                try {
                    PythonEnvConfig config;
                    auto section = ini["python_environment"];
                    config.default_env = section["default"].String().empty() ? "system" : section["default"].String();
                    config.conda_prefix = section["conda_prefix"].String().empty() ? "/opt/conda" : section["conda_prefix"].String();
                    config.uv_venv_path = section["uv_venv_path"].String().empty() ? "./venv" : section["uv_venv_path"].String();
                    return config;
                } catch (const std::exception &e) {
                    MCP_ERROR("Failed to load Python env config: {}", e.what());
                    throw;
                }
            }
        };

        /**
 * Global configuration
 */
        struct GlobalConfig {
            std::string title;
            ServerConfig server;
            PluginHubConfig plugin_hub;
            PythonEnvConfig python_env;

            static GlobalConfig load() {
                try {
                    inicpp::IniManager ini(get_config_file_path());
                    MCP_INFO("Loading configuration from: {}", get_config_file_path());

                    GlobalConfig config;
                    config.title = ini[""]["title"].String().empty() ? "MCP Server Configuration" : ini[""]["title"].String();
                    config.server = ServerConfig::load(ini);
                    config.plugin_hub = PluginHubConfig::load(ini);
                    config.python_env = PythonEnvConfig::load(ini);
                    return config;
                } catch (const std::exception &e) {
                    MCP_ERROR("Failed to load global config: {}", e.what());
                    throw;
                }
            }
        };

        // Forward declaration
        class ConfigLoader;

        // Global state (managed by loader)
        inline std::unique_ptr<ConfigLoader> g_config_loader;
        inline std::unique_ptr<GlobalConfig> g_current_config;
        inline std::mutex g_config_mutex;
        inline std::atomic<bool> g_config_initialized{false};

        /**
 * Abstract base class using Template Method and Observer patterns
 */
        class ConfigLoader {
        protected:
            std::vector<ConfigObserver *> observers_;
            std::atomic<bool> monitoring_active{false};
            std::thread monitor_thread;

            virtual std::unique_ptr<GlobalConfig> createDefaultConfig() = 0;

            virtual std::unique_ptr<GlobalConfig> loadFromStaticFile() {
                return std::make_unique<GlobalConfig>(GlobalConfig::load());
            }

            virtual void startMonitoring(GlobalConfig *config) {
                if (monitoring_active.load()) return;
                monitoring_active = true;

                monitor_thread = std::thread([this, config]() {
                    std::filesystem::file_time_type last_write = {};
                    while (monitoring_active.load()) {
                        try {
                            auto path = std::filesystem::path(get_config_file_path());
                            if (std::filesystem::exists(path)) {
                                auto curr_time = std::filesystem::last_write_time(path);
                                if (curr_time != last_write) {
                                    MCP_INFO("Config file changed, reloading...");
                                    auto newConfig = loadFromStaticFile();
                                    if (newConfig) {
                                        std::lock_guard<std::mutex> lock(g_config_mutex);
                                        *config = *newConfig;
                                        notifyObservers(*config);
                                        last_write = curr_time;
                                    }
                                }
                            }
                            std::this_thread::sleep_for(std::chrono::seconds(2));
                        } catch (...) {
                            if (monitoring_active.load()) {
                                MCP_ERROR("Error monitoring config file");
                            }
                            break;
                        }
                    }
                });
            }

            void notifyObservers(const GlobalConfig &config) {
                for (auto *obs: observers_) {
                    try {
                        obs->onConfigReloaded(config);
                    } catch (const std::exception &e) {
                        MCP_ERROR("Observer notification failed: {}", e.what());
                    }
                }
            }

        public:
            virtual ~ConfigLoader() {
                monitoring_active = false;
                if (monitor_thread.joinable()) {
                    monitor_thread.join();
                }
            }

            void addObserver(ConfigObserver *obs) {
                if (obs) {
                    std::lock_guard<std::mutex> lock(g_config_mutex);
                    if (std::find(observers_.begin(), observers_.end(), obs) == observers_.end()) {
                        observers_.push_back(obs);
                    }
                }
            }

            void removeObserver(ConfigObserver *obs) {
                std::lock_guard<std::mutex> lock(g_config_mutex);
                observers_.erase(
                        std::remove(observers_.begin(), observers_.end(), obs),
                        observers_.end());
            }

            std::unique_ptr<GlobalConfig> load(ConfigMode mode) {
                std::unique_ptr<GlobalConfig> config;

                switch (mode) {
                    case ConfigMode::NONE:
                        config = createDefaultConfig();
                        break;
                    case ConfigMode::STATIC:
                        if (std::filesystem::exists(get_config_file_path())) {
                            config = loadFromStaticFile();
                        } else {
                            MCP_WARN("Config file not found, using default settings");
                            config = createDefaultConfig();
                        }
                        break;
                    case ConfigMode::DYNAMIC:
                        config = loadFromStaticFile();
                        startMonitoring(config.get());
                        break;
                    default:
                        config = createDefaultConfig();
                }

                notifyObservers(*config);
                return config;
            }
        };

        /**
 * Default implementation of ConfigLoader
 */
        class DefaultConfigLoader : public ConfigLoader {
        protected:
            std::unique_ptr<GlobalConfig> createDefaultConfig() override {
                auto config = std::make_unique<GlobalConfig>();
                config->title = "Default MCP Server Config";
                config->server.ip = "127.0.0.1";
                config->server.port = 6666;
                config->server.http_port = 6666;
                config->server.https_port = 0;
                config->server.log_level = "info";
                config->server.plugin_dir = "plugins";
                config->server.enable_stdio = true;
                config->server.enable_http = true;
                config->server.enable_https = false;
                config->server.enable_auth = false;
                config->plugin_hub.plugin_server_baseurl = "http://47.120.50.122";
                config->plugin_hub.plugin_server_port = 6680;
                config->python_env.default_env = "system";
                config->python_env.conda_prefix = "/opt/conda";
                config->python_env.uv_venv_path = "./venv";
                return config;
            }
        };

        /**
 * Compatibility layer: keep old interfaces
 */

        inline void initialize_default_config() {
            try {
                std::string config_file = get_config_file_path();
                if (std::filesystem::exists(config_file) && std::filesystem::file_size(config_file) > 0) {
                    return;
                }

                inicpp::IniManager ini(config_file);
                MCP_INFO("Creating default config file: {}", config_file);

                // [server]
                ini.set("server", "ip", "0.0.0.0");
                ini.set("server", "port", 6666);
                ini.set("server", "http_port", 6666);
                ini.set("server", "https_port", 6667);
                ini.set("server", "log_level", "trace");
                ini.set("server", "log_path", "logs/mcp_server.log");
                ini.set("server", "max_file_size", 10485760);
                ini.set("server", "max_files", 10);
                ini.set("server", "plugin_dir", "plugins");
                ini.set("server", "enable_stdio", 1);
                ini.set("server", "enable_http", 1);
                ini.set("server", "enable_https", 0);
                ini.set("server", "enable_auth", 0);
                ini.set("server", "auth_type", "X-API-Key");
                ini.set("server", "auth_env_file", ".env.auth");
                ini.set("server", "ssl_cert_file", "certs/server.crt");
                ini.set("server", "ssl_key_file", "certs/server.key");
                ini.set("server", "ssl_dh_params_file", "certs/dh2048.pem");
                ini.set("server", "max_requests_per_second", 100);
                ini.set("server", "max_concurrent_requests", 1000);
                ini.set("server", "max_request_size", 1024 * 1024);
                ini.set("server", "max_response_size", 10 * 1024 * 1024);

                // [plugin_hub]
                ini.set("plugin_hub", "plugin_server_baseurl", "http://47.120.50.122");
                ini.set("plugin_hub", "plugin_server_port", 6680);
                ini.set("plugin_hub", "latest_fetch_route", "/self/latest/info");
                ini.set("plugin_hub", "download_route", "/self/latest/download");
                ini.set("plugin_hub", "plugin_install_dir", "plugins_install");
                ini.set("plugin_hub", "plugin_enable_dir", "plugins");
                ini.set("plugin_hub", "tools_install_dir", "plugins_install");
                ini.set("plugin_hub", "tools_enable_dir", "configs");

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
                ini.setComment("server", "enable_auth", "Enable authentication (1=enable, 0=disable)");
                ini.setComment("server", "auth_type", "Authentication type (X-API-Key, Bearer)");
                ini.setComment("server", "auth_env_file", "Authentication environment file path");
                ini.setComment("server", "ssl_cert_file", "SSL certificate file path (required for HTTPS)");
                ini.setComment("server", "ssl_key_file", "SSL private key file path (required for HTTPS)");
                ini.setComment("server", "ssl_dh_params_file", "SSL Diffie-Hellman parameters file path (required for HTTPS)");
                // Rate limiter configuration comments
                ini.setComment("server", "max_requests_per_second", "Rate limiter: maximum requests allowed per second");
                ini.setComment("server", "max_concurrent_requests", "Rate limiter: maximum concurrent requests");
                ini.setComment("server", "max_request_size", "Rate limiter: maximum request size in bytes");
                ini.setComment("server", "max_response_size", "Rate limiter: maximum response size in bytes");

                // Add comments for plugin_hub section
                ini.setComment("plugin_hub", "plugin_server_baseurl", "Base URL for plugin server");
                ini.setComment("plugin_hub", "plugin_server_port", "Port for plugin server");
                ini.setComment("plugin_hub", "latest_fetch_route", "Route for fetching latest plugin info");
                ini.setComment("plugin_hub", "download_route", "Route for downloading plugin");
                ini.setComment("plugin_hub", "plugin_install_dir", "Directory for installing plugins");
                ini.setComment("plugin_hub", "plugin_enable_dir", "Directory for enabling plugins");

                // Add coments for PythonEnvConfig
                ini.setComment("PythonEnvConfig", "default_env", "Default environment interpreter to use for Python plugins");
                ini.setComment("PythonEnvConfig", "conda_prefix", "Path to conda prefix");
                ini.setComment("PythonEnvConfig", "uv_venv_path", "Path to uv_venv");


                // Root section configuration

                ini.set("title", "MCP Server Configuration");
                ini.setComment("title", "Auto-generated configuration file");
                ini.parse();

                MCP_INFO("Default config created successfully");
            } catch (const std::exception &e) {
                MCP_ERROR("Failed to initialize default config: {}", e.what());
                throw;
            }
        }

        inline void print_config(const GlobalConfig &config) {
            MCP_DEBUG("===== MCP Configuration =====");
            MCP_DEBUG("Title: {}", config.title);
            MCP_DEBUG("Server IP: {}", config.server.ip);
            MCP_DEBUG("Port: {}", config.server.port);
            MCP_DEBUG("HTTP Port: {}", config.server.http_port);
            MCP_DEBUG("HTTPS Port: {}", config.server.https_port);
            MCP_DEBUG("Log Level: {}", config.server.log_level);
            MCP_DEBUG("Plugin Dir: {}", config.server.plugin_dir);
            MCP_DEBUG("Auth Enabled: {}", config.server.enable_auth ? "Yes" : "No");
            MCP_DEBUG("Max Requests/sec: {}", config.server.max_requests_per_second);
            MCP_DEBUG("Plugin Server: {}:{}", config.plugin_hub.plugin_server_baseurl, config.plugin_hub.plugin_server_port);
            MCP_DEBUG("Python Env: {}", config.python_env.default_env);
            MCP_DEBUG("=============================");
        }

        inline void list_config_sections() {
            try {
                std::string path = get_config_file_path();
                inicpp::IniManager ini(path);
                MCP_DEBUG("=== Config File: {} ===", path);
                for (const auto &sec: ini.sectionsList()) {
                    MCP_DEBUG("[{}]", sec);
                    for (const auto &[k, v]: ini.sectionMap(sec)) {
                        MCP_DEBUG("  {} = {}", k, v);
                    }
                }
            } catch (const std::exception &e) {
                MCP_ERROR("Failed to list config sections: {}", e.what());
                throw;
            }
        }

        /**
 * Initialize the new config system
 */
        inline void initialize_config_system(ConfigMode mode = ConfigMode::STATIC) {
            if (g_config_initialized.load()) return;

            g_config_loader = std::make_unique<DefaultConfigLoader>();
            {
                std::lock_guard<std::mutex> lock(g_config_mutex);
                g_current_config = g_config_loader->load(mode);
            }
            g_config_initialized = true;
        }

        /**
 * Get current config (thread-safe)
 */
        inline GlobalConfig get_current_config() {
            std::lock_guard<std::mutex> lock(g_config_mutex);
            return *g_current_config;
        }

        /**
 * Update config (used by observers or reload)
 */
        inline void update_current_config(const GlobalConfig &newConfig) {
            std::lock_guard<std::mutex> lock(g_config_mutex);
            *g_current_config = newConfig;
        }

    }// namespace config
}// namespace mcp

#endif// MCP_CONFIG_HPP