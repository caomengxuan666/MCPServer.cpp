/*
 * @Author: caomengxuan666 
 * @Date: 2025-08-13 14:58:46 
 * @Description: It is able to install plugins whether the server runs or not
 * @Last Modified by: caomengxuan666
 * @Last Modified time: 2025-08-13 16:13:19
 */

#include "pluginhub.hpp"
#include "core/logger.h"// For MCP_INFO, MCP_DEBUG
#include "httplib.h"
#include "miniz.h"
#include "plugins/sdk/mcp_plugin.h"
#include <algorithm>
#include <filesystem>
#include <fstream>


using namespace mcp::plugins;

namespace fs = std::filesystem;

// Static members definition
mcp::config::PluginHubConfig PluginHub::config_;
std::unique_ptr<PluginHub> PluginHub::instance_;

void PluginHub::create(mcp::config::PluginHubConfig &config) {
    config_ = config;
    instance_ = std::make_unique<PluginHub>();
    instance_->tellPlatform();
    MCP_INFO("PluginHub initialized with plugin directory: {}", config_.plugin_install_dir);
}

PluginHub &PluginHub::getInstance() {
    if (!instance_) {
        MCP_CRITICAL("PluginHub::create() must be called before getInstance().");
        throw std::runtime_error("PluginHub not initialized.");
    }
    return *instance_;
}

bool PluginHub::download(const std::string &plugin_id) {
    // Initialize logger if not already initialized
    static bool logger_initialized = false;
    if (!logger_initialized) {
        try {
            mcp::core::initializeAsyncLogger("logs/plugin_hub.log", "info", 1048576 * 5, 3);
            mcp::core::MCPLogger::enable_file_sink();
            logger_initialized = true;
        } catch (const std::exception &e) {
            // If logger is already initialized, that's fine - just continue
            logger_initialized = true;
        }
    }

    MCP_INFO("Starting download of plugin: {}", plugin_id);
    std::cout << "📥 Downloading plugin '" << plugin_id << "'..." << std::endl;

    std::string server_url = config_.plugin_server_baseurl;
    unsigned short server_port = config_.plugin_server_port;

    // Remove protocol prefix if present
    std::string host = server_url;
    if (host.substr(0, 7) == "http://") {
        host = host.substr(7);
    } else if (host.substr(0, 8) == "https://") {
        host = host.substr(8);
    }

    // Use the configured download route and append platform-specific path
    std::string download_route = config_.download_route;
    switch (platform_) {
        case Platform::Windows:
            download_route += "/windows";
            break;
        case Platform::Linux:
            download_route += "/linux";
            break;
        default:
            download_route += "/unknown";
            break;
    }

    httplib::Client cli(host.c_str(), server_port);
    // Set timeouts for connection and reading
    cli.set_connection_timeout(10, 0);// 10 seconds
    cli.set_read_timeout(30, 0);      // 30 seconds
    cli.set_write_timeout(10, 0);     // 10 seconds

    // Variables for progress tracking
    size_t current_size = 0;
    size_t total_size = 0;
    bool progress_started = false;

    // Progress bar helper function
    auto show_progress = [](size_t current, size_t total) {
        if (total == 0) return;

        int progress_percent = static_cast<int>((static_cast<double>(current) / total) * 100);
        int bar_width = 50;
        int filled_width = static_cast<int>((static_cast<double>(current) / total) * bar_width);

        std::cout << "\r[";
        for (int i = 0; i < bar_width; ++i) {
            if (i < filled_width) {
                std::cout << "=";
            } else if (i == filled_width) {
                std::cout << ">";
            } else {
                std::cout << " ";
            }
        }
        std::cout << "] " << progress_percent << "% (" << current << "/" << total << " bytes)";
        std::cout.flush();
    };

    std::string response_body;
    // Use the correct signature for httplib::Client::Get
    auto res = cli.Get(download_route.c_str(),
                       // Content receiver callback
                       [&](const char *data, size_t data_length) -> bool {
                           response_body.append(data, data_length);
                           current_size += data_length;
                           if (progress_started && total_size > 0) {
                               show_progress(current_size, total_size);
                           }
                           return true;// Continue receiving
                       },
                       // Progress callback
                       [&](size_t current, size_t total) -> bool {
                           total_size = total;
                           progress_started = true;
                           std::cout << "\n📦 File size: " << total << " bytes" << std::endl;
                           return true;// Continue receiving
                       });

    if (!res) {
        MCP_ERROR("Failed to connect to plugin server {}:{}", host, server_port);
        MCP_ERROR("Error: {}", httplib::to_string(res.error()));
        std::cerr << "❌ Failed to download plugin '" << plugin_id << "'" << std::endl;
        std::cerr << "Error: " << httplib::to_string(res.error()) << std::endl;
        return false;
    }

    if (res->status != 200) {
        MCP_ERROR("Failed to download plugin '{}', status code: {}", plugin_id, res->status);
        std::cerr << "❌ Failed to download plugin '" << plugin_id << "', status code: " << res->status << std::endl;
        return false;
    }

    // Create install directory if it doesn't exist
    fs::path install_dir(config_.plugin_install_dir);
    if (!fs::exists(install_dir)) {
        fs::create_directories(install_dir);
    }

    // Extract filename from Content-Disposition header if available
    std::string filename = plugin_id + ".zip";// default filename
    auto it = res->headers.find("Content-Disposition");
    if (it != res->headers.end()) {
        std::string content_disposition = it->second;
        // Look for filename="..." pattern
        size_t pos = content_disposition.find("filename=");
        if (pos != std::string::npos) {
            filename = content_disposition.substr(pos + 9);// 9 is length of "filename="
            // Remove quotes if present
            if (!filename.empty() && filename.front() == '"' && filename.back() == '"') {
                filename = filename.substr(1, filename.length() - 2);
            }
        }
    }

    // Save the downloaded zip file
    fs::path zip_path = install_dir / filename;
    std::ofstream file(zip_path, std::ios::binary);
    if (!file.is_open()) {
        MCP_ERROR("Failed to create file: {}", zip_path.string());
        std::cerr << "❌ Failed to create file: " << zip_path.string() << std::endl;
        return false;
    }

    file.write(response_body.c_str(), response_body.size());
    file.close();

    MCP_INFO("Plugin '{}' downloaded successfully to: {}", plugin_id, zip_path.string());
    std::cout << "✅ Plugin '" << plugin_id << "' downloaded successfully to: " << zip_path.string() << std::endl;
    return true;
}

std::vector<std::string> PluginHub::listRemote() {
    std::string server_url = config_.plugin_server_baseurl;
    unsigned short server_port = config_.plugin_server_port;

    std::string list_url = server_url + ":" + std::to_string(server_port) + config_.latest_fetch_route;

    std::cout << "Listing remote plugins from server: " << list_url << std::endl;

    // TODO: Implement actual remote plugin listing
    return {"example_plugin", "file_plugin", "http_plugin"};
}

std::vector<std::string> PluginHub::listInstalled() {
    std::string install_dir = config_.plugin_install_dir;
    std::cout << "Listing installed plugins from directory: " << install_dir << std::endl;

    return getPluginsInDirectory(install_dir);
}

bool PluginHub::isPluginEnabled(const std::string &plugin_name) {
    std::string enable_dir = config_.plugin_enable_dir;
    fs::path plugin_path(enable_dir);
    plugin_path /= plugin_name;

    return fs::exists(plugin_path);
}

std::vector<std::string> PluginHub::getPluginsInDirectory(const std::string &directory) {
    std::vector<std::string> plugins;

    if (!fs::exists(directory)) {
        std::cerr << "Plugin directory does not exist: " << directory << std::endl;
        return plugins;
    }

    try {
        for (const auto &entry: fs::directory_iterator(directory)) {
            if (entry.is_directory()) {
                plugins.push_back(entry.path().filename().string());
            }
        }
    } catch (const fs::filesystem_error &e) {
        std::cerr << "Error reading plugin directory " << directory << ": " << e.what() << std::endl;
    }

    // Sort for consistent output
    std::sort(plugins.begin(), plugins.end());
    return plugins;
}

bool PluginHub::install(const std::string &plugin_id) {
    // Initialize logger if not already initialized
    static bool logger_initialized = false;
    if (!logger_initialized) {
        try {
            mcp::core::initializeAsyncLogger("logs/plugin_hub.log", "info", 1048576 * 5, 3);
            mcp::core::MCPLogger::enable_file_sink();
            logger_initialized = true;
        } catch (const std::exception &e) {
            // If logger is already initialized, that's fine - just continue
            logger_initialized = true;
        }
    }

    MCP_INFO("Starting installation of plugin: {}", plugin_id);
    std::cout << "⚙️  Starting installation of plugin: " << plugin_id << std::endl;

    // Check if zip file already exists with the standard name
    fs::path zip_path = fs::path(config_.plugin_install_dir) / (plugin_id + ".zip");
    MCP_INFO("Looking for zip file at: {}", zip_path.string());

    // Also check for other possible zip files in the directory that might be the plugin
    if (!fs::exists(zip_path)) {
        MCP_INFO("Standard zip file not found, scanning directory for any zip files");
        try {
            for (const auto &entry: fs::directory_iterator(config_.plugin_install_dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".zip") {
                    // Use the first zip file found - this is a temporary solution
                    // In a more robust implementation, we would match the file to the plugin
                    zip_path = entry.path();
                    MCP_INFO("Using existing zip file: {}", zip_path.string());
                    std::cout << "📁 Using existing zip file: " << zip_path.string() << std::endl;
                    break;
                }
            }
        } catch (const fs::filesystem_error &e) {
            MCP_WARN("Could not iterate plugin directory: {}", e.what());
            std::cerr << "⚠️  Warning: Could not iterate plugin directory: " << e.what() << std::endl;
        }
    }

    if (!fs::exists(zip_path)) {
        // Download if not exists
        MCP_INFO("Plugin zip not found, downloading...");
        std::cout << "📥 Plugin zip not found, downloading..." << std::endl;
        if (!download(plugin_id)) {
            MCP_ERROR("Failed to download plugin: {}", plugin_id);
            std::cerr << "❌ Failed to download plugin: " << plugin_id << std::endl;
            return false;
        }

        // After downloading, we need to find the actual downloaded file
        // since download() now extracts the real filename from HTTP headers
        MCP_INFO("Scanning for downloaded zip file");
        std::cout << "🔎 Scanning for downloaded zip file" << std::endl;
        try {
            for (const auto &entry: fs::directory_iterator(config_.plugin_install_dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".zip") {
                    zip_path = entry.path();
                    MCP_INFO("Found downloaded zip file: {}", zip_path.string());
                    std::cout << "📁 Found downloaded zip file: " << zip_path.string() << std::endl;
                    break;
                }
            }
        } catch (const fs::filesystem_error &e) {
            MCP_WARN("Could not iterate plugin directory: {}", e.what());
            std::cerr << "⚠️  Warning: Could not iterate plugin directory: " << e.what() << std::endl;
        }
    }

    if (!fs::exists(zip_path)) {
        MCP_ERROR("Plugin zip file not found after download attempt");
        std::cerr << "❌ Plugin zip file not found after download attempt" << std::endl;
        return false;
    }

    // Create install directory if it doesn't exist
    fs::path install_dir(config_.plugin_install_dir);
    if (!fs::exists(install_dir)) {
        fs::create_directories(install_dir);
    }

    // Extract the zip file to a temporary directory using miniz
    fs::path temp_dir = install_dir / "temp_extract";
    MCP_INFO("Using temporary extraction directory: {}", temp_dir.string());
    std::cout << "📂 Using temporary extraction directory: " << temp_dir.string() << std::endl;

    // Remove existing temp directory if it exists
    if (fs::exists(temp_dir)) {
        MCP_INFO("Removing existing temporary directory");
        std::cout << "🗑️  Removing existing temporary directory" << std::endl;
        fs::remove_all(temp_dir);
    }

    // Create temp directory
    MCP_INFO("Creating temporary directory");
    std::cout << "📁 Creating temporary directory" << std::endl;
    fs::create_directory(temp_dir);

    // Extract zip file using miniz
    MCP_INFO("Extracting zip file: {}", zip_path.string());
    std::cout << "📦 Extracting zip file..." << std::endl;
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    mz_bool status = mz_zip_reader_init_file(&zip_archive, zip_path.string().c_str(), 0);
    if (!status) {
        MCP_ERROR("Failed to initialize zip reader for file: {}", zip_path.string());
        std::cerr << "❌ Failed to initialize zip reader for file: " << zip_path.string() << std::endl;
        return false;
    }

    int file_count = (int) mz_zip_reader_get_num_files(&zip_archive);
    MCP_INFO("Extracting {} files from plugin zip", file_count);
    std::cout << "📂 Extracting " << file_count << " files from plugin zip" << std::endl;

    for (int i = 0; i < file_count; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
            MCP_ERROR("Failed to get file stat for file index: {}", i);
            std::cerr << "❌ Failed to get file stat for file index: " << i << std::endl;
            mz_zip_reader_end(&zip_archive);
            return false;
        }

        fs::path extract_path = temp_dir / file_stat.m_filename;
        MCP_INFO("Extracting file: {} to {}", file_stat.m_filename, extract_path.string());
        std::cout << "  ➤ Extracting: " << file_stat.m_filename << std::endl;

        // Create directory if needed
        if (file_stat.m_is_directory) {
            fs::create_directories(extract_path);
            continue;
        }

        // Create parent directories if needed
        fs::create_directories(extract_path.parent_path());

        // Extract file
        mz_bool extract_result = mz_zip_reader_extract_to_file(&zip_archive, i, extract_path.string().c_str(), 0);
        if (!extract_result) {
            MCP_ERROR("Failed to extract file: {}", file_stat.m_filename);
            std::cerr << "❌ Failed to extract file: " << file_stat.m_filename << std::endl;
            mz_zip_reader_end(&zip_archive);
            return false;
        }
    }

    // Close the archive
    mz_zip_reader_end(&zip_archive);
    MCP_INFO("Zip extraction completed successfully");
    std::cout << "✅ Zip extraction completed successfully" << std::endl;

    // Now move plugins to plugin_enable_dir and configs to tools_enable_dir
    fs::path plugins_dest_dir = fs::path(config_.plugin_enable_dir);
    fs::path tools_dest_dir = fs::path(config_.tools_enable_dir);

    MCP_INFO("Plugins destination directory: {}", plugins_dest_dir.string());
    MCP_INFO("Tools destination directory: {}", tools_dest_dir.string());
    std::cout << "📂 Plugins destination directory: " << plugins_dest_dir.string() << std::endl;
    std::cout << "📂 Tools destination directory: " << tools_dest_dir.string() << std::endl;

    // Create destination directories if they don't exist
    if (!fs::exists(plugins_dest_dir)) {
        MCP_INFO("Creating plugins destination directory");
        std::cout << "📁 Creating plugins destination directory" << std::endl;
        fs::create_directories(plugins_dest_dir);
    }

    if (!fs::exists(tools_dest_dir)) {
        MCP_INFO("Creating tools destination directory");
        std::cout << "📁 Creating tools destination directory" << std::endl;
        fs::create_directories(tools_dest_dir);
    }

    // Move plugins directory contents if it exists in the extracted files
    // Fix: Look for the correct path structure (bin/plugins instead of just plugins)
    fs::path plugins_src_dir = temp_dir / "bin" / "plugins";
    MCP_INFO("Plugins source directory: {}", plugins_src_dir.string());
    std::cout << "📂 Plugins source directory: " << plugins_src_dir.string() << std::endl;
    if (fs::exists(plugins_src_dir)) {
        MCP_INFO("Plugins directory found in zip, moving contents");
        std::cout << "🚚 Plugins directory found in zip, moving contents" << std::endl;
        try {
            int plugin_count = 0;
            for (const auto &entry: fs::directory_iterator(plugins_src_dir)) {
                fs::path dest_path = plugins_dest_dir / entry.path().filename();
                MCP_INFO("Moving plugin: {} -> {}", entry.path().string(), dest_path.string());
                std::cout << "  ➤ Moving plugin: " << entry.path().filename().string() << std::endl;
                if (fs::exists(dest_path)) {
                    fs::remove_all(dest_path);
                }
                fs::copy(entry.path(), dest_path, fs::copy_options::recursive);
                MCP_INFO("Moved plugin file/directory: {} -> {}", entry.path().string(), dest_path.string());
                plugin_count++;
            }
            std::cout << "✅ Moved " << plugin_count << " plugin(s)" << std::endl;
        } catch (const fs::filesystem_error &e) {
            MCP_ERROR("Failed to move plugins: {}", e.what());
            std::cerr << "❌ Failed to move plugins: " << e.what() << std::endl;
            // Clean up temporary directory
            fs::remove_all(temp_dir);
            return false;
        }
    } else {
        MCP_INFO("No plugins directory found in the extracted zip file");
        std::cout << "⚠️  No plugins directory found in the extracted zip file" << std::endl;
    }

    // Move configs directory contents if it exists in the extracted files
    // Fix: Look for the correct path structure (bin/configs instead of just configs)
    fs::path configs_src_dir = temp_dir / "bin" / "configs";
    MCP_INFO("Configs source directory: {}", configs_src_dir.string());
    std::cout << "📂 Configs source directory: " << configs_src_dir.string() << std::endl;
    if (fs::exists(configs_src_dir)) {
        MCP_INFO("Configs directory found in zip, moving contents");
        std::cout << "🚚 Configs directory found in zip, moving contents" << std::endl;
        try {
            int config_count = 0;
            for (const auto &entry: fs::directory_iterator(configs_src_dir)) {
                fs::path dest_path = tools_dest_dir / entry.path().filename();
                MCP_INFO("Moving config: {} -> {}", entry.path().string(), dest_path.string());
                std::cout << "  ➤ Moving config: " << entry.path().filename().string() << std::endl;
                if (fs::exists(dest_path)) {
                    fs::remove_all(dest_path);
                }
                fs::copy(entry.path(), dest_path, fs::copy_options::recursive);
                MCP_INFO("Moved config file/directory: {} -> {}", entry.path().string(), dest_path.string());
                config_count++;
            }
            std::cout << "✅ Moved " << config_count << " config file(s)" << std::endl;
        } catch (const fs::filesystem_error &e) {
            MCP_ERROR("Failed to move configs: {}", e.what());
            std::cerr << "❌ Failed to move configs: " << e.what() << std::endl;
            // Clean up temporary directory
            fs::remove_all(temp_dir);
            return false;
        }
    } else {
        MCP_INFO("No configs directory found in the extracted zip file");
        std::cout << "⚠️  No configs directory found in the extracted zip file" << std::endl;
    }

    // Clean up temporary directory
    MCP_INFO("Cleaning up temporary directory");
    // Clean up temporary directory
    MCP_INFO("Cleaning up temporary directory");
    fs::remove_all(temp_dir);

    MCP_INFO("Plugin '{}' installed successfully", plugin_id);
    std::cout << "🎉 Plugin '" << plugin_id << "' installed successfully" << std::endl;
    return true;
    fs::remove_all(temp_dir);

    MCP_INFO("Plugin '{}' installed successfully", plugin_id);
    std::cout << "🎉 Plugin '" << plugin_id << "' installed successfully" << std::endl;
    return true;
    fs::remove_all(temp_dir);

    MCP_INFO("Plugin '{}' installed successfully", plugin_id);
    std::cout << "🎉 Plugin '" << plugin_id << "' installed successfully" << std::endl;
    return true;
}

void PluginHub::uninstall(const std::string &plugin_name) {
    std::string install_dir = config_.plugin_install_dir;
    std::string enable_dir = config_.plugin_enable_dir;

    std::cout << "Uninstalling plugin '" << plugin_name << "' from install dir: " << install_dir
              << " and enable dir: " << enable_dir << std::endl;

    // TODO: Remove the plugin files and metadata
}

void PluginHub::enable(const std::string &plugin_name) {
    std::cout << "Enabling plugin: " << plugin_name << std::endl;
    // TODO: Move plugin from install dir to enable dir
}

void PluginHub::disable(const std::string &plugin_name) {
    std::cout << "Disabling plugin: " << plugin_name << std::endl;
    // TODO: Move plugin from enable dir to install dir (or bin)
}

PluginInfo PluginHub::getPluginInfo(const std::string &plugin_id) {
    std::string install_dir = config_.plugin_install_dir;

    std::cout << "Getting plugin info for '" << plugin_id << "' from directory: " << install_dir << std::endl;
    // TODO: Iterate through directory and parse plugin metadata

    return {};
}
PluginHub::PluginHub() {
}

ReleaseInfo PluginHub::getReleaseInfo(const std::string &plugin_id) {
    std::string server_url = config_.plugin_server_baseurl;
    unsigned short server_port = config_.plugin_server_port;
    std::string fetch_route = config_.latest_fetch_route;

    std::string release_url = server_url + ":" + std::to_string(server_port) + fetch_route;

    std::cout << "Fetching release info for '" << plugin_id << "' from: " << release_url << std::endl;

    // TODO: Fetch and parse release info from remote server
    return {};
}

void PluginHub::tellPlatform() {
#ifdef _WIN32
    platform_ = Platform::Windows;
#elif __linux__
    platform_ = Platform::Linux;
#else
    platform_ = Platform::Unknown;
#endif
}