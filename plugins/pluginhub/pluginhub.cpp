/*
 * @Author: caomengxuan666 
 * @Date: 2025-08-13 14:58:46 
 * @Description: It is able to install plugins whether the server runs or not
 * @Last Modified by: caomengxuan666
 * @Last Modified time: 2025-08-13 16:13:19
 */

#include "pluginhub.hpp"
#include "core/logger.h"// For MCP_INFO, MCP_DEBUG
#include "plugins/sdk/mcp_plugin.h"
#include <filesystem>
#include <algorithm>

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
    std::string server_url = config_.plugin_server_baseurl;
    unsigned short server_port = config_.plugin_server_port;

    std::string download_url = server_url + ":" + std::to_string(server_port) + config_.download_route;

    MCP_INFO("Downloading plugin '{}' from server: {}", plugin_id, download_url);

    // TODO: Implement actual download logic
    return true;
}

std::vector<std::string> PluginHub::listRemote() {
    std::string server_url = config_.plugin_server_baseurl;
    unsigned short server_port = config_.plugin_server_port;

    std::string list_url = server_url + ":" + std::to_string(server_port) + config_.latest_fetch_route;

    MCP_INFO("Listing remote plugins from server: {}", list_url);

    // TODO: Implement actual remote plugin listing
    return {"example_plugin", "file_plugin", "http_plugin"};
}

std::vector<std::string> PluginHub::listInstalled() {
    std::string install_dir = config_.plugin_install_dir;
    MCP_INFO("Listing installed plugins from directory: {}", install_dir);
    
    return getPluginsInDirectory(install_dir);
}

bool PluginHub::isPluginEnabled(const std::string &plugin_name) {
    std::string enable_dir = config_.plugin_enable_dir;
    fs::path plugin_path(enable_dir);
    plugin_path /= plugin_name;
    
    return fs::exists(plugin_path);
}

std::vector<std::string> PluginHub::getPluginsInDirectory(const std::string& directory) {
    std::vector<std::string> plugins;
    
    if (!fs::exists(directory)) {
        MCP_WARN("Plugin directory does not exist: {}", directory);
        return plugins;
    }
    
    try {
        for (const auto& entry : fs::directory_iterator(directory)) {
            if (entry.is_directory()) {
                plugins.push_back(entry.path().filename().string());
            }
        }
    } catch (const fs::filesystem_error& e) {
        MCP_ERROR("Error reading plugin directory {}: {}", directory, e.what());
    }
    
    // Sort for consistent output
    std::sort(plugins.begin(), plugins.end());
    return plugins;
}

bool PluginHub::install(const std::string &plugin_id) {
    std::string server_url = config_.plugin_server_baseurl;
    unsigned short server_port = config_.plugin_server_port;

    std::string install_url = server_url + ":" + std::to_string(server_port) + config_.download_route;

    MCP_INFO("Installing plugin '{}' from server: {}", plugin_id, install_url);

    // TODO: Implement actual download and installation logic
    return true;
}

void PluginHub::uninstall(const std::string &plugin_name) {
    std::string install_dir = config_.plugin_install_dir;
    std::string enable_dir = config_.plugin_enable_dir;

    MCP_INFO("Uninstalling plugin '{}' from install dir: {} and enable dir: {}",
             plugin_name, install_dir, enable_dir);

    // TODO: Remove the plugin files and metadata
}

void PluginHub::enable(const std::string &plugin_name) {
    MCP_INFO("Enabling plugin: {}", plugin_name);
    // TODO: Move plugin from install dir to enable dir
}

void PluginHub::disable(const std::string &plugin_name) {
    MCP_INFO("Disabling plugin: {}", plugin_name);
    // TODO: Move plugin from enable dir to install dir (or bin)
}

PluginInfo PluginHub::getPluginInfo(const std::string &plugin_id) {
    std::string install_dir = config_.plugin_install_dir;

    MCP_DEBUG("Getting plugin info for '{}' from directory: {}", plugin_id, install_dir);
    // TODO: Iterate through directory and parse plugin metadata

    return {};
}

ReleaseInfo PluginHub::getReleaseInfo(const std::string &plugin_id) {
    std::string server_url = config_.plugin_server_baseurl;
    unsigned short server_port = config_.plugin_server_port;
    std::string fetch_route = config_.latest_fetch_route;

    std::string release_url = server_url + ":" + std::to_string(server_port) + fetch_route;

    MCP_DEBUG("Fetching release info for '{}' from: {}", plugin_id, release_url);

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