/*
 * @Author: caomengxuan
 * @Date: 2025-08-14
 * @Description: MCP Plugin CLI Tool (plugin_ctl) - Refactored version
 *               Following CommandLineConfig best practices
 */
#pragma once
#include "args.hxx"
#include "config/config.hpp"
#include "core/logger.h"
#include "plugins/pluginhub/pluginhub.hpp"

// STL
#include <filesystem>
#include <functional>
#include <iostream>
#include <mutex>
#include <unordered_map>


namespace fs = std::filesystem;
namespace mcp {
    namespace apps {

        class PluginCtlConfig {
        public:
            PluginCtlConfig() : config_path_(config::get_default_config_path()) {}

            // Parse command line arguments
            bool initialize(int argc, char *argv[]) {
                std::lock_guard<std::mutex> lock(mutex_);
                return parseArguments(argc, argv);
            }

            // Get parsed results
            std::string getCommand() const {
                std::lock_guard<std::mutex> lock(mutex_);
                return command_;
            }

            std::string getPluginId() const {
                std::lock_guard<std::mutex> lock(mutex_);
                return plugin_id_;
            }

            std::string getConfigPath() const {
                std::lock_guard<std::mutex> lock(mutex_);
                return config_path_;
            }

            bool isRemoteList() const {
                std::lock_guard<std::mutex> lock(mutex_);
                return list_remote_;
            }

        private:
            bool parseArguments(int argc, char *argv[]) {
                args::ArgumentParser parser("MCP Plugin Management Tool",
                                            "plugin_ctl <command> [options]\nCommands: create, download, install, enable, disable, uninstall, list, status");
                parser.Prog(argv[0]);

                // Global options
                args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});
                args::ValueFlag<std::string> config_path(parser, "PATH", "Custom config file path", {"config"},
                                                         config::get_default_config_path());

                // Subcommand definitions
                args::Command create_cmd(parser, "create", "Create new plugin template");
                args::Command download_cmd(parser, "download", "Download plugin from server");
                args::Command install_cmd(parser, "install", "Install plugin");
                args::Command enable_cmd(parser, "enable", "Enable plugin");
                args::Command disable_cmd(parser, "disable", "Disable plugin");
                args::Command uninstall_cmd(parser, "uninstall", "Uninstall plugin");
                args::Command list_cmd(parser, "list", "List plugins");
                args::Command status_cmd(parser, "status", "Show hub status");

                // Subcommand arguments
                args::Positional<std::string> create_id(create_cmd, "PLUGIN_ID", "Name of plugin to create");
                args::Positional<std::string> download_id(download_cmd, "PLUGIN_ID", "ID of plugin to download");
                args::Positional<std::string> install_id(install_cmd, "PLUGIN_ID", "ID of plugin to install");
                args::Positional<std::string> enable_id(enable_cmd, "PLUGIN_ID", "ID of plugin to enable");
                args::Positional<std::string> disable_id(disable_cmd, "PLUGIN_ID", "ID of plugin to disable");
                args::Positional<std::string> uninstall_id(uninstall_cmd, "PLUGIN_ID", "ID of plugin to uninstall");
                args::Flag list_remote(list_cmd, "remote", "List remote plugins", {"remote"});

                try {
                    parser.ParseCLI(argc, argv);
                } catch (const args::Help &) {
                    std::cout << parser;
                    return false;
                } catch (const args::ParseError &e) {
                    std::cerr << "Error parsing command line: " << e.what() << std::endl;
                    std::cerr << parser;
                    return false;
                } catch (const args::ValidationError &e) {
                    std::cerr << "Validation error: " << e.what() << std::endl;
                    std::cerr << parser;
                    return false;
                }

                // Store parsed results
                config_path_ = args::get(config_path);

                if (create_cmd) {
                    command_ = "create";
                    plugin_id_ = args::get(create_id);
                } else if (download_cmd) {
                    command_ = "download";
                    plugin_id_ = args::get(download_id);
                } else if (install_cmd) {
                    command_ = "install";
                    plugin_id_ = args::get(install_id);
                } else if (enable_cmd) {
                    command_ = "enable";
                    plugin_id_ = args::get(enable_id);
                } else if (disable_cmd) {
                    command_ = "disable";
                    plugin_id_ = args::get(disable_id);
                } else if (uninstall_cmd) {
                    command_ = "uninstall";
                    plugin_id_ = args::get(uninstall_id);
                } else if (list_cmd) {
                    command_ = "list";
                    list_remote_ = args::get(list_remote);
                } else if (status_cmd) {
                    command_ = "status";
                } else {
                    std::cerr << "Error: No command specified" << std::endl;
                    std::cerr << parser;
                    return false;
                }

                // Validate required parameters
                if (needsPluginId(command_) && plugin_id_.empty()) {
                    std::cerr << "Error: Plugin ID is required for " << command_ << std::endl;
                    std::cerr << parser;
                    return false;
                }

                return true;
            }

            // Check if command needs plugin ID parameter
            bool needsPluginId(const std::string &cmd) const {
                static const std::unordered_set<std::string> cmds_need_id = {
                        "create", "download", "install", "enable", "disable", "uninstall"};
                return cmds_need_id.count(cmd) > 0;
            }

            mutable std::mutex mutex_;
            std::string command_;
            std::string plugin_id_;
            std::string config_path_;
            bool list_remote_ = false;
        };

        // Command handler function declarations
        void handle_create(const std::string &plugin_id);
        void handle_download(const std::string &plugin_id);
        void handle_install(const std::string &plugin_id);
        void handle_enable(const std::string &plugin_id);
        void handle_disable(const std::string &plugin_id);
        void handle_uninstall(const std::string &plugin_id);
        void handle_list(bool remote);
        void handle_status();

        // Global configuration
        config::PluginHubConfig g_hub_config;

        /**
         * @brief Load configuration and initialize logger
         */
        void load_config(const std::string &config_path) {
            config::set_config_file_path(config_path);
            auto config = config::GlobalConfig::load();
            g_hub_config = config.plugin_hub;

            std::cout << "plugin_ctl started with config: " << config_path << std::endl;
            std::cout << "  Plugin install dir: " << g_hub_config.plugin_install_dir << std::endl;
            std::cout << "  Plugin enable dir:  " << g_hub_config.plugin_enable_dir << std::endl;
            std::cout << "  Tools install dir:   " << g_hub_config.tools_install_dir << std::endl;
            std::cout << "  Tools enable dir:    " << g_hub_config.tools_enable_dir << std::endl;
            std::cout << "  Server base URL:     " << g_hub_config.plugin_server_baseurl << std::endl;
            std::cout << "  Server port:         " << g_hub_config.plugin_server_port << std::endl;
            std::cout << "  Download route:      " << g_hub_config.download_route << std::endl;
            std::cout << "  Latest fetch route:  " << g_hub_config.latest_fetch_route << std::endl;
        }


        // =============================================================================
        // Command Implementations
        // =============================================================================

        void handle_create(const std::string &plugin_id) {
            // Create plugin directory
            auto current_dir = fs::current_path();
            fs::path plugins_dir = current_dir / plugin_id;

            if (fs::exists(plugins_dir)) {
                std::cerr << "Error: Plugin directory already exists: " << plugins_dir << std::endl;
                return;
            }

            try {
                // Create plugin directory
                auto current_dir = fs::current_path();
                fs::path plugins_dir = current_dir / plugin_id;
                
                if (fs::exists(plugins_dir)) {
                    std::cerr << "Error: Plugin directory already exists: " << plugins_dir << std::endl;
                    return;
                }
                
                fs::create_directory(plugins_dir);

                // Create <plugin_name>.cpp file
                fs::path plugin_file = plugins_dir / (plugin_id + ".cpp");
                if (!fs::exists(plugin_file)) {
                    std::ofstream ofs(plugin_file);
                    ofs << "// Plugin: " << plugin_id << "\n";
                    ofs << "// This is a template for your plugin implementation.\n";
                    ofs << "#include \"core/mcpserver_api.h\"\n";
                    ofs << "#include \"mcp_plugin.h\"\n";
                    ofs << "#include \"tool_info_parser.h\"\n";
                    ofs << "#include <cstdlib>\n";
                    ofs << "#include <nlohmann/json.hpp>\n";
                    ofs << "#include <string>\n\n";
                    ofs << "static std::vector<ToolInfo> g_tools;\n\n";
                    ofs << "// Generator structure for streaming tools\n";
                    ofs << "struct " << plugin_id << "Generator {\n";
                    ofs << "    // Add your generator fields here\n";
                    ofs << "    bool running = true;\n";
                    ofs << "    std::string error;\n";
                    ofs << "};\n\n";
                    ofs << "// Generator next function for streaming tools\n";
                    ofs << "static int " << plugin_id << "_next(void* generator, const char** result_json, MCPError *error) {\n";
                    ofs << "    // Add your streaming logic here\n";
                    ofs << "    // Return 0 to continue streaming, 1 to stop\n";
                    ofs << "    if (!generator) {\n";
                    ofs << "        *result_json = R\"({\"error\": \"Invalid generator pointer\"})\";\n";
                    ofs << "        if (error) {\n";
                    ofs << "            error->code = 1;\n";
                    ofs << "            error->message = strdup(\"Invalid generator pointer\");\n";
                    ofs << "        }\n";
                    ofs << "        return 1;\n";
                    ofs << "    }\n\n";
                    ofs << "    auto* gen = static_cast<" << plugin_id << "Generator*>(generator);\n";
                    ofs << "    \n";
                    ofs << "    // Check if there's an error\n";
                    ofs << "    if (!gen->error.empty()) {\n";
                    ofs << "        *result_json = gen->error.c_str();\n";
                    ofs << "        if (error) {\n";
                    ofs << "            error->code = 2;\n";
                    ofs << "            error->message = strdup(gen->error.c_str());\n";
                    ofs << "        }\n";
                    ofs << "        return 1;\n";
                    ofs << "    }\n\n";
                    ofs << "    // Check if streaming should stop\n";
                    ofs << "    if (!gen->running) {\n";
                    ofs << "        *result_json = nullptr;\n";
                    ofs << "        return 1;\n";
                    ofs << "    }\n\n";
                    ofs << "    // TODO: Implement your streaming logic here\n";
                    ofs << "    // Example streaming response:\n";
                    ofs << "    static thread_local std::string buffer;\n";
                    ofs << "    buffer = nlohmann::json({{\"jsonrpc\", \"2.0\"},\n";
                    ofs << "                             {\"method\", \"text\"},\n";
                    ofs << "                             {\"params\", {{\"text\", \"Example streamed content\"}}}})\n";
                    ofs << "                     .dump();\n\n";
                    ofs << "    *result_json = buffer.c_str();\n";
                    ofs << "    if (error) {\n";
                    ofs << "        error->code = 0; // No error\n";
                    ofs << "        error->message = nullptr;\n";
                    ofs << "    }\n";
                    ofs << "    return 0; // Continue streaming\n";
                    ofs << "}\n\n";
                    ofs << "// Generator free function for streaming tools\n";
                    ofs << "static void " << plugin_id << "_free(void* generator) {\n";
                    ofs << "    auto* gen = static_cast<" << plugin_id << "Generator*>(generator);\n";
                    ofs << "    if (gen) {\n";
                    ofs << "        gen->running = false;\n";
                    ofs << "        delete gen;\n";
                    ofs << "    }\n";
                    ofs << "}\n\n";
                    ofs << "extern \"C\" MCP_API ToolInfo *get_tools(int *count) {\n";
                    ofs << "    try {\n";
                    ofs << "        // Load tool information if not already loaded\n";
                    ofs << "        if (g_tools.empty()) {\n";
                    ofs << "            g_tools = ToolInfoParser::loadFromFile(\"" << plugin_id << "_tools.json\");\n";
                    ofs << "        }\n\n";
                    ofs << "        *count = static_cast<int>(g_tools.size());\n";
                    ofs << "        return g_tools.data();\n";
                    ofs << "    } catch (const std::exception &e) {\n";
                    ofs << "        *count = 0;\n";
                    ofs << "        return nullptr;\n";
                    ofs << "    }\n";
                    ofs << "}\n\n";
                    ofs << "extern \"C\" MCP_API const char *call_tool(const char *name, const char *args_json, MCPError *error) {\n";
                    ofs << "    try {\n";
                    ofs << "        auto args = nlohmann::json::parse(args_json);\n";
                    ofs << "        std::string tool_name = name;\n\n";
                    ofs << "        // TODO: Implement your tool logic here\n";
                    ofs << "        if (tool_name == \"" << plugin_id << "\") {\n";
                    ofs << "            // Example implementation - replace with your actual logic\n";
                    ofs << "            std::string result = nlohmann::json{{\"result\", \"Hello from " << plugin_id << "\"}}.dump();\n";
                    ofs << "            return strdup(result.c_str());\n";
                    ofs << "        }\n\n";
                    ofs << "        // For streaming tools, return the generator\n";
                    ofs << "        // Example for a streaming tool:\n";
                    ofs << "        // if (tool_name == \"stream_" << plugin_id << "\") {\n";
                    ofs << "        //     auto* gen = new " << plugin_id << "Generator();\n";
                    ofs << "        //     // Initialize your generator here\n";
                    ofs << "        //     return reinterpret_cast<const char*>(gen);\n";
                    ofs << "        // }\n\n";
                    ofs << "        if (error) {\n";
                    ofs << "            error->code = 3;\n";
                    ofs << "            std::string error_msg = \"Unknown tool: \" + tool_name;\n";
                    ofs << "            error->message = strdup(error_msg.c_str());\n";
                    ofs << "        }\n";
                    ofs << "        return strdup((nlohmann::json{{\"error\", \"Unknown tool: \" + tool_name}}.dump()).c_str());\n";
                    ofs << "    } catch (const std::exception &e) {\n";
                    ofs << "        if (error) {\n";
                    ofs << "            error->code = 4;\n";
                    ofs << "            error->message = strdup(e.what());\n";
                    ofs << "        }\n";
                    ofs << "        return strdup((nlohmann::json{{\"error\", e.what()}}.dump()).c_str());\n";
                    ofs << "    }\n";
                    ofs << "extern \"C\" MCP_API void free_result(const char *result) {\n";
                    ofs << "    if (result) {\n";
                    ofs << "        std::free(const_cast<char *>(result));\n";
                    ofs << "    }\n";
                    ofs << "}\n\n";
                    ofs << "// For streaming tools, implement these functions\n";
                    ofs << "extern \"C\" MCP_API StreamGeneratorNext get_stream_next() {\n";
                    ofs << "    return reinterpret_cast<StreamGeneratorNext>(" << plugin_id << "_next);\n";
                    ofs << "}\n\n";
                    ofs << "extern \"C\" MCP_API StreamGeneratorFree get_stream_free() {\n";
                    ofs << "    return reinterpret_cast<StreamGeneratorFree>(" << plugin_id << "_free);\n";
                    ofs << "}\n";
                }

                // Create CMakeLists.txt file in plugin directory
                fs::path cmakelists_file = plugins_dir / "CMakeLists.txt";
                if (!fs::exists(cmakelists_file)) {
                    std::ofstream ofs(cmakelists_file);
                    ofs << "configure_plugin(" << plugin_id << " " << plugin_id << ".cpp)\n";
                }

                // Create tools.json file
                fs::path tools_json_file = plugins_dir / "tools.json";
                if (!fs::exists(tools_json_file)) {
                    std::ofstream ofs(tools_json_file);
                    ofs << "{\n";
                    ofs << "  \"tools\": [\n";
                    ofs << "    {\n";
                    ofs << "      \"name\": \"" << plugin_id << "\",\n";
                    ofs << "      \"description\": \"Description of " << plugin_id << "\",\n";
                    ofs << "      \"parameters\": {\n";
                    ofs << "        \"type\": \"object\",\n";
                    ofs << "        \"properties\": {\n";
                    ofs << "          \"param1\": {\n";
                    ofs << "            \"type\": \"string\",\n";
                    ofs << "            \"description\": \"An example parameter\"\n";
                    ofs << "          }\n";
                    ofs << "        },\n";
                    ofs << "        \"required\": []\n";
                    ofs << "      }\n";
                    ofs << "    },\n";
                    ofs << "    {\n";
                    ofs << "      \"name\": \"stream_" << plugin_id << "\",\n";
                    ofs << "      \"description\": \"Stream data from " << plugin_id << "\",\n";
                    ofs << "      \"parameters\": {\n";
                    ofs << "        \"type\": \"object\",\n";
                    ofs << "        \"properties\": {\n";
                    ofs << "          \"param1\": {\n";
                    ofs << "            \"type\": \"string\",\n";
                    ofs << "            \"description\": \"An example parameter\"\n";
                    ofs << "          }\n";
                    ofs << "        },\n";
                    ofs << "        \"required\": []\n";
                    ofs << "      },\n";
                    ofs << "      \"is_streaming\": true\n";
                    ofs << "    }\n";
                    ofs << "  ]\n";
                    ofs << "}\n";
                }

                std::cout << "Plugin template created at: " << plugin_file << std::endl;
                std::cout << "Tools JSON template created at: " << tools_json_file << std::endl;
                std::cout << "You can now implement your plugin logic in " << plugin_file << std::endl;
                std::cout << "Modify the tools.json file to define your tool's interface" << std::endl;

            } catch (const fs::filesystem_error &e) {
                std::cerr << "Error creating plugin: " << e.what() << std::endl;
            }
        }
        void handle_download(const std::string &plugin_id) {
            auto &hub = plugins::PluginHub::getInstance();

            std::cout << "Downloading plugin: " << plugin_id << std::endl;
            if (hub.download(plugin_id)) {
                std::cout << "⬇️  Plugin '" << plugin_id << "' downloaded to '"
                          << g_hub_config.plugin_install_dir << "'" << std::endl;
            } else {
                std::cerr << "❌ Failed to download plugin '" << plugin_id << "'" << std::endl;
            }
        }

        void handle_install(const std::string &plugin_id) {
            auto &hub = plugins::PluginHub::getInstance();
            fs::path zip_path = fs::path(g_hub_config.plugin_install_dir) / (plugin_id + ".zip");

            if (!fs::exists(zip_path)) {
                std::cout << "Archive not found, downloading..." << std::endl;
                if (!hub.download(plugin_id)) {
                    std::cerr << "❌ Failed to download plugin '" << plugin_id << "'" << std::endl;
                    return;
                }
            }

            if (hub.install(plugin_id)) {
                std::cout << "✅ Plugin '" << plugin_id << "' installed successfully" << std::endl;
            } else {
                std::cerr << "❌ Failed to install plugin '" << plugin_id << "'" << std::endl;
            }
        }

        void handle_enable(const std::string &plugin_id) {
            auto &hub = plugins::PluginHub::getInstance();
            fs::path src = fs::path(g_hub_config.plugin_install_dir) / plugin_id;

            if (!fs::exists(src)) {
                std::cerr << "❌ Plugin not installed: " << plugin_id << std::endl;
                return;
            }

            hub.enable(plugin_id);
            std::cout << "🟢 Plugin '" << plugin_id << "' enabled." << std::endl;
        }

        void handle_disable(const std::string &plugin_id) {
            auto &hub = plugins::PluginHub::getInstance();
            hub.disable(plugin_id);
            std::cout << "🔴 Plugin '" << plugin_id << "' disabled." << std::endl;
        }

        void handle_uninstall(const std::string &plugin_id) {
            auto &hub = plugins::PluginHub::getInstance();
            hub.uninstall(plugin_id);
            std::cout << "🗑️ Plugin '" << plugin_id << "' uninstalled." << std::endl;
        }

        void handle_list(bool remote) {
            auto &hub = plugins::PluginHub::getInstance();

            if (remote) {
                auto plugins = hub.listRemote();
                std::cout << "🌍 Remote plugins:" << std::endl;
                for (const auto &p: plugins) {
                    std::cout << "  - " << p << std::endl;
                }
            } else {
                auto plugins = hub.listInstalled();
                std::cout << "📦 Installed plugins:" << std::endl;
                for (const auto &p: plugins) {
                    bool enabled = hub.isPluginEnabled(p);
                    std::cout << "  - " << p << (enabled ? " (enabled)" : " (disabled)") << std::endl;
                }
            }
        }

        void handle_status() {
            std::cout << "🔧 PluginHub Status\n";
            std::cout << "==================\n";
            std::cout << "Config file:              " << config::get_config_file_path() << "\n";
            std::cout << "Install dir:              " << g_hub_config.plugin_install_dir << "\n";
            std::cout << "Enable dir (plugins):     " << g_hub_config.plugin_enable_dir << "\n";
            std::cout << "Tools install dir:        " << g_hub_config.tools_install_dir << "\n";
            std::cout << "Tools enable dir (configs): " << g_hub_config.tools_enable_dir << "\n";
            std::cout << "Server base URL:          " << g_hub_config.plugin_server_baseurl << "\n";
            std::cout << "Server port:              " << g_hub_config.plugin_server_port << "\n";
            std::cout << "Download route:           " << g_hub_config.download_route << "\n";
            std::cout << "Latest fetch route:       " << g_hub_config.latest_fetch_route << "\n";
        }


    }// namespace apps
}// namespace mcp

using namespace mcp::apps;
int main(int argc, char *argv[]) {
    try {
        // 1. Parse command line arguments
        PluginCtlConfig cli_config;
        if (!cli_config.initialize(argc, argv)) {
            return 1;// Parsing failed or user requested help
        }

        // 2. Load configuration file
        load_config(cli_config.getConfigPath());

        // 3. Initialize plugin manager
        mcp::plugins::PluginHub::create(g_hub_config);

        // 4. Dispatch command processing
        const std::string command = cli_config.getCommand();
        const std::unordered_map<std::string, std::function<void()>> command_handlers = {
                {"create", [&]() { handle_create(cli_config.getPluginId()); }},
                {"download", [&]() { handle_download(cli_config.getPluginId()); }},
                {"install", [&]() { handle_install(cli_config.getPluginId()); }},
                {"enable", [&]() { handle_enable(cli_config.getPluginId()); }},
                {"disable", [&]() { handle_disable(cli_config.getPluginId()); }},
                {"uninstall", [&]() { handle_uninstall(cli_config.getPluginId()); }},
                {"list", [&]() { handle_list(cli_config.isRemoteList()); }},
                {"status", []() { handle_status(); }}};

        auto it = command_handlers.find(command);
        if (it != command_handlers.end()) {
            it->second();
        } else {
            std::cerr << "Unknown command: " << command << std::endl;
            return 1;
        }

        return 0;

    } catch (const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}