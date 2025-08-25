/*
 * @Author: caomengxuan
 * @Date: 2025-08-14
 * @Description: MCP Plugin CLI Tool (plugin_ctl) - Refactored version
 *               Following CommandLineConfig best practices
 */
#include "args.hxx"
#include "config/config.hpp"
#include "core/logger.h"
#include "plugins/pluginhub/pluginhub.hpp"

// STL
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <sstream>
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

            bool isPythonPlugin() const {
                std::lock_guard<std::mutex> lock(mutex_);
                return python_plugin_;
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
                args::Command build_cmd(parser, "build", "Build Python plugin to DLL");

                // Subcommand arguments
                args::Positional<std::string> create_id(create_cmd, "PLUGIN_ID", "Name of plugin to create");
                args::Positional<std::string> download_id(download_cmd, "PLUGIN_ID", "ID of plugin to download");
                args::Positional<std::string> install_id(install_cmd, "PLUGIN_ID", "ID of plugin to install");
                args::Positional<std::string> enable_id(enable_cmd, "PLUGIN_ID", "ID of plugin to enable");
                args::Positional<std::string> disable_id(disable_cmd, "PLUGIN_ID", "ID of plugin to disable");
                args::Positional<std::string> uninstall_id(uninstall_cmd, "PLUGIN_ID", "ID of plugin to uninstall");
                args::Positional<std::string> build_id(build_cmd, "PLUGIN_ID", "ID of Python plugin to build");
                args::Flag list_remote(list_cmd, "remote", "List remote plugins", {"remote"});
                args::Flag python_flag(create_cmd, "python", "Create Python plugin template instead of C++", {'p', "python"});

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
                    python_plugin_ = args::get(python_flag);
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
                } else if (build_cmd) {
                    command_ = "build";
                    plugin_id_ = args::get(build_id);
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
                        "create", "download", "install", "enable", "disable", "uninstall", "build"};
                return cmds_need_id.count(cmd) > 0;
            }

            mutable std::mutex mutex_;
            std::string command_;
            std::string plugin_id_;
            std::string config_path_;
            bool list_remote_ = false;
            bool python_plugin_ = false;
        };

        // Command handler function declarations
        void handle_create(const std::string &plugin_id, bool is_python);
        void handle_download(const std::string &plugin_id);
        void handle_install(const std::string &plugin_id);
        void handle_enable(const std::string &plugin_id);
        void handle_disable(const std::string &plugin_id);
        void handle_uninstall(const std::string &plugin_id);
        void handle_list(bool remote);
        void handle_status();
        void handle_build(const std::string &plugin_id);

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

        void handle_create(const std::string &plugin_id, bool is_python) {
            if (is_python) {
                // Create Python plugin template
                auto current_dir = fs::current_path();
                fs::path plugins_dir = current_dir / plugin_id;

                if (fs::exists(plugins_dir)) {
                    std::cerr << "Error: Plugin directory already exists: " << plugins_dir << std::endl;
                    return;
                }

                try {
                    // Create plugin directory
                    fs::create_directory(plugins_dir);

                    // Create plugin.py file
                    fs::path plugin_file = plugins_dir / (plugin_id + ".py");
                    if (!fs::exists(plugin_file)) {
                        std::ofstream ofs(plugin_file);
                        ofs << "# Plugin: " << plugin_id << "\n";
                        ofs << "# This is a template for your Python plugin implementation using the new MCP SDK.\n\n";
                        ofs << "from mcp_sdk import tool, string_param, get_tools, call_tool\n\n";
                        ofs << "# Example of a standard tool\n";
                        ofs << "@tool(\n";
                        ofs << "    name=\"" << plugin_id << "\",\n";
                        ofs << "    description=\"Description of " << plugin_id << "\",\n";
                        ofs << "    param1=string_param(description=\"An example parameter\")\n";
                        ofs << ")\n";
                        ofs << "def " << plugin_id << "_tool(param1: str = \"default_value\"):\n";
                        ofs << "    \"\"\"Example tool implementation\"\"\"\n";
                        ofs << "    return f\"Hello from " << plugin_id << "! Parameter value: {param1}\"\n\n";
                        ofs << "# Example of a streaming tool\n";
                        ofs << "@tool(\n";
                        ofs << "    name=\"stream_" << plugin_id << "\",\n";
                        ofs << "    description=\"Stream data from " << plugin_id << "\",\n";
                        ofs << "    tool_type=\"streaming\",\n";
                        ofs << "    count=string_param(description=\"Number of items to stream\", required=False, default=\"5\")\n";
                        ofs << ")\n";
                        ofs << "def stream_" << plugin_id << "_tool(count: int = 5):\n";
                        ofs << "    \"\"\"Example streaming tool implementation\"\"\"\n";
                        ofs << "    for i in range(int(count)):\n";
                        ofs << "        yield {\"text\": f\"Streamed data item {i}\"}\n";
                    }

                    // Create CMakeLists.txt file in plugin directory
                    fs::path cmakelists_file = plugins_dir / "CMakeLists.txt";
                    if (!fs::exists(cmakelists_file)) {
                        std::ofstream ofs(cmakelists_file);
                        ofs << "# CMakeLists.txt for Python Plugin\n";
                        ofs << "cmake_minimum_required(VERSION 3.23)\n";
                        ofs << "project(" << plugin_id << ")\n\n";
                        ofs << "# Set the path to MCPServer++ root directory\n";
                        ofs << "set(MCP_SERVER_ROOT \"${CMAKE_CURRENT_SOURCE_DIR}/../..\" CACHE STRING \"Path to MCPServer++ root directory\")\n\n";
                        ofs << "# Find required packages\n";
                        ofs << "find_package(Python COMPONENTS Interpreter Development REQUIRED)\n\n";
                        ofs << "# Add the plugin library\n";
                        ofs << "add_library(${PROJECT_NAME} SHARED\n";
                        ofs << "    ${MCP_SERVER_ROOT}/plugins/sdk/pybind_module_plugin.cpp\n";
                        ofs << ")\n\n";
                        ofs << "# Include directories\n";
                        ofs << "target_include_directories(${PROJECT_NAME} PRIVATE\n";
                        ofs << "    ${CMAKE_CURRENT_SOURCE_DIR}\n";
                        ofs << "    ${MCP_SERVER_ROOT}/plugins/sdk\n";
                        ofs << "    ${MCP_SERVER_ROOT}/include\n";
                        ofs << "    ${MCP_SERVER_ROOT}/third_party/nlohmann\n";
                        ofs << "    ${MCP_SERVER_ROOT}/third_party/pybind11/include\n";
                        ofs << ")\n\n";
                        ofs << "# Add preprocessor definition for DLL export\n";
                        ofs << "target_compile_definitions(${PROJECT_NAME} PRIVATE MCP_PLUGIN_EXPORTS)\n\n";
                        ofs << "# Link libraries\n";
                        ofs << "target_link_libraries(${PROJECT_NAME} PRIVATE \n";
                        ofs << "    pybind11::embed\n";
                        ofs << ")\n\n";
                        ofs << "# Ensure the Python plugin file and SDK are available\n";
                        ofs << "configure_file(${CMAKE_CURRENT_SOURCE_DIR}/" << plugin_id << ".py \n";
                        ofs << "               ${CMAKE_CURRENT_BINARY_DIR}/" << plugin_id << ".py \n";
                        ofs << "               COPYONLY)\n";
                        ofs << "configure_file(${MCP_SERVER_ROOT}/plugins/sdk/mcp_sdk.py \n";
                        ofs << "               ${CMAKE_CURRENT_BINARY_DIR}/mcp_sdk.py \n";
                        ofs << "               COPYONLY)\n";
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
                        ofs << "          \"count\": {\n";
                        ofs << "            \"type\": \"string\",\n";
                        ofs << "            \"description\": \"Number of items to stream\"\n";
                        ofs << "          }\n";
                        ofs << "        },\n";
                        ofs << "        \"required\": []\n";
                        ofs << "      },\n";
                        ofs << "      \"is_streaming\": true\n";
                        ofs << "    }\n";
                        ofs << "  ]\n";
                        ofs << "}\n";
                    }

                    std::cout << "Python plugin template created at: " << plugins_dir << std::endl;
                    std::cout << "Python plugin file: " << plugin_file << std::endl;
                    std::cout << "CMakeLists.txt file: " << cmakelists_file << std::endl;
                    std::cout << "Tools JSON template: " << tools_json_file << std::endl;
                    std::cout << std::endl;
                    std::cout << "To build the plugin DLL:" << std::endl;
                    std::cout << "  1. cd " << plugins_dir.string() << std::endl;
                    std::cout << "  2. mkdir build && cd build" << std::endl;
                    std::cout << "  3. cmake .." << std::endl;
                    std::cout << "  4. cmake --build ." << std::endl;
                    std::cout << std::endl;
                    std::cout << "The resulting DLL will be in the build directory." << std::endl;
                } catch (const fs::filesystem_error &e) {
                    std::cerr << "Error creating Python plugin: " << e.what() << std::endl;
                }
            } else {
                // Create C++ plugin template (existing code)
                auto current_dir = fs::current_path();
                fs::path plugins_dir = current_dir / plugin_id;

                if (fs::exists(plugins_dir)) {
                    std::cerr << "Error: Plugin directory already exists: " << plugins_dir << std::endl;
                    return;
                }

                try {
                    // Create plugin directory
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
                        ofs << "}\n\n";
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
        }

        void handle_build(const std::string &plugin_id) {
            // Build Python plugin to DLL
            auto current_dir = fs::current_path();
            fs::path plugins_dir = current_dir / plugin_id;

            if (!fs::exists(plugins_dir)) {
                std::cerr << "Error: Plugin directory does not exist: " << plugins_dir << std::endl;
                return;
            }

            // Check if it's a Python plugin by looking for .py file
            fs::path plugin_py_file = plugins_dir / (plugin_id + ".py");
            if (!fs::exists(plugin_py_file)) {
                std::cerr << "Error: Python plugin file not found: " << plugin_py_file << std::endl;
                return;
            }

            try {
                // Change to plugin directory
                fs::current_path(plugins_dir);

                // Create build directory
                fs::path build_dir = plugins_dir / "build";
                if (!fs::exists(build_dir)) {
                    fs::create_directory(build_dir);
                }

                // Change to build directory
                fs::current_path(build_dir);

                // Run cmake
                std::cout << "Configuring Python plugin build..." << std::endl;
                int cmake_result = std::system("cmake ..");
                if (cmake_result != 0) {
                    std::cerr << "Error: CMake configuration failed" << std::endl;
                    return;
                }

                // Build the plugin
                std::cout << "Building Python plugin..." << std::endl;
                int build_result = std::system("cmake --build . --config Release");
                if (build_result != 0) {
                    std::cerr << "Error: Plugin build failed" << std::endl;
                    return;
                }

                std::cout << "Python plugin built successfully!" << std::endl;
                std::cout << "DLL file location: " << (build_dir / (plugin_id + ".dll")).string() << std::endl;
            } catch (const fs::filesystem_error &e) {
                std::cerr << "Error building Python plugin: " << e.what() << std::endl;
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
                {"create", [&]() { handle_create(cli_config.getPluginId(), cli_config.isPythonPlugin()); }},
                {"download", [&]() { handle_download(cli_config.getPluginId()); }},
                {"install", [&]() { handle_install(cli_config.getPluginId()); }},
                {"enable", [&]() { handle_enable(cli_config.getPluginId()); }},
                {"disable", [&]() { handle_disable(cli_config.getPluginId()); }},
                {"uninstall", [&]() { handle_uninstall(cli_config.getPluginId()); }},
                {"list", [&]() { handle_list(cli_config.isRemoteList()); }},
                {"status", []() { handle_status(); }},
                {"build", [&]() { handle_build(cli_config.getPluginId()); }}};

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