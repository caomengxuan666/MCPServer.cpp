/*
 * @Author: caomengxuan666 
 * @Date: 2025-08-13 14:58:46 
 * @Description: It is able to install plugins whether the server runs or not
 * @Last Modified by: caomengxuan666
 * @Last Modified time: 2025-08-13 16:14:02
 */
#pragma once

#include "config/config.hpp"
#include <memory>
#include <string>
#include <vector>

struct ToolInfo;

namespace mcp::plugins {
    enum class Platform {
        Unknown,
        Windows,
        Linux
    };

    // Release asset information (by platform)
    struct ReleaseAsset {
        std::string name;        // Asset filename
        std::string download_url;// Download URL
        std::string local_path;  // Local save path
        Platform platform;       // Target platform
    };

    // Complete release information
    struct ReleaseInfo {
        std::string tag_name;            // Version tag (e.g. v1.0.0)
        std::string name;                // Release name
        std::string published_at;        // Publish time
        std::vector<ReleaseAsset> assets;// Assets by platform
    };

    struct PluginInfo {
        // (Original fields unchanged)
        std::string id;
        std::string name;
        std::string version;
        std::string description;
        std::string url;
        std::string file_path;
        std::vector<ToolInfo> tools;
        std::string release_date;
        bool enabled = true;
        std::vector<std::string> tool_names;
        std::vector<std::string> tool_descriptions;
        std::vector<std::string> tool_parameters;
    };

    /**
     * @brief PluginHub manages the installation, uninstallation, enabling, and disabling of plugins.
     * This class follows the singleton pattern and is initialized via `create()`.
     */
    class PluginHub {
    public:
        /**
         * @brief Initializes the PluginHub with the given configuration.
         * This must be called before accessing the instance.
         * @param config Configuration for the plugin hub.
         */
        static void create(mcp::config::PluginHubConfig &config);

        /**
         * @brief Gets the singleton instance of PluginHub.
         * @return Reference to the singleton instance.
         */
        static PluginHub &getInstance();

        // install it from the remote url
        bool install(const std::string &plugin_name);

        // Uninstall a plugin by name
        void uninstall(const std::string &plugin_name);

        // Enable a plugin (move it to the enabled directory)
        void enable(const std::string &plugin_name);

        // Disable a plugin (move it out of the enabled directory)
        void disable(const std::string &plugin_name);

        // Download a plugin from remote server
        bool download(const std::string &plugin_name);
        
        // List remote plugins
        std::vector<std::string> listRemote();
        
        // List installed plugins
        std::vector<std::string> listInstalled();
        
        // Check if a plugin is enabled
        bool isPluginEnabled(const std::string &plugin_name);

        // TODO The server doesn't support it temporarily
        PluginInfo getPluginInfo(const std::string &plugin_id = "");

        // It is supported, but parameters are not supported yet
        // TODO The server doesn't support get release info by id temporarily.
        ReleaseInfo getReleaseInfo(const std::string &plugin_id = "");
        PluginHub();
        ~PluginHub() = default;

    private:
        PluginHub(const PluginHub &) = delete;
        PluginHub &operator=(const PluginHub &) = delete;

        void tellPlatform();
        
        // Helper methods for plugin management
        std::vector<std::string> getPluginsInDirectory(const std::string& directory);

        static mcp::config::PluginHubConfig config_;
        static std::unique_ptr<PluginHub> instance_;
        Platform platform_;
    };
}// namespace mcp::plugins