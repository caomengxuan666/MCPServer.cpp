// src/business/plugin_manager.cpp
#include "plugin_manager.h"
#include "core/logger.h"
#include <filesystem>

namespace mcp::business {

    PluginManager::~PluginManager() {
        for (auto &[name, plugin]: plugins_) {
            if (plugin->handle) {
                CLOSE_LIB(plugin->handle);
            }
        }
        plugins_.clear();
        load_order_.clear();
    }

    bool PluginManager::load_plugin(const std::string &path) {
        std::filesystem::path plugin_path = std::filesystem::absolute(path);
        std::string plugin_file_path = plugin_path.string();
        MCP_TRACE("Loading plugin from: {}", plugin_file_path);

        if (!std::filesystem::exists(plugin_file_path)) {
            MCP_ERROR("Plugin file not found: {}", plugin_file_path);
            return false;
        }

        lib_handle handle = LOAD_LIB(plugin_file_path.c_str());
        if (!handle) {
            MCP_ERROR("Failed to load library: {}", plugin_file_path);
#ifdef _WIN32
            MCP_ERROR("Last error: {}", GetLastError());
#else
            MCP_ERROR("dlerror: {}", dlerror());
#endif
            return false;
        }

        // Get the plugin functions
        auto get_tools = (get_tools_func) GET_FUNC(handle, "get_tools");
        auto call_tool = (call_tool_func) GET_FUNC(handle, "call_tool");
        auto free_result = (free_result_func) GET_FUNC(handle, "free_result");

        // Load stream functions if available
        auto get_stream_next_loader = (get_stream_next_func) GET_FUNC(handle, "get_stream_next");
        auto get_stream_free_loader = (get_stream_free_func) GET_FUNC(handle, "get_stream_free");
        std::string plugin_name = plugin_path.filename().string();


        if (!get_tools || !call_tool || !free_result) {
            MCP_ERROR("Plugin missing required functions: {}", plugin_file_path);
            CLOSE_LIB(handle);
            return false;
        }

        // Loading tools from the plugin
        int tool_count = 0;
        ToolInfo *tool_infos = get_tools(&tool_count);
        if (!tool_infos || tool_count == 0) {
            MCP_WARN("Plugin has no tools: {}", plugin_file_path);
        }

        // Store the plugins
        auto plugin = std::make_unique<Plugin>();
        plugin->handle = handle;
        plugin->get_tools = get_tools;
        plugin->call_tool = call_tool;
        plugin->free_result = free_result;
        plugin->get_stream_next = get_stream_next_loader;
        plugin->get_stream_free = get_stream_free_loader;
        for (int i = 0; i < tool_count; ++i) {
            plugin->tool_list.push_back(tool_infos[i]);
            MCP_DEBUG("Loaded tool: '{}' from plugin", tool_infos[i].name);
        }


        plugins_[plugin_name] = std::move(plugin);
        load_order_.push_back(plugin_name);
        MCP_DEBUG("Plugin loaded successfully: {} (total plugins: {})", plugin_name, plugins_.size());

        return true;
    }


    void PluginManager::load_plugins_from_directory(const std::string &directory) {
        // Handle empty directory path - use current directory
        std::filesystem::path dir_path;
        if (directory.empty()) {
            dir_path = std::filesystem::current_path();
            MCP_INFO("Empty directory path provided, using current directory: {}", dir_path.string());
        } else {
            dir_path = std::filesystem::path(directory);
        }

        if (!std::filesystem::exists(dir_path)) {
            MCP_WARN("Plugin directory does not exist: {}", dir_path.string());
            return;
        }

        if (!std::filesystem::is_directory(dir_path)) {
            MCP_WARN("Path is not a directory: {}", dir_path.string());
            return;
        }

        MCP_INFO("Scanning plugin directory: {}", dir_path.string());

        // Foreach file in directory
        for (const auto &entry: std::filesystem::directory_iterator(dir_path)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();

                // Cross platform
#ifdef _WIN32
                if (entry.path().extension() == ".dll") {
#elif __APPLE__
                if (entry.path().extension() == ".dylib") {
#else
                if (entry.path().extension() == ".so") {
#endif
                    MCP_TRACE("Found plugin file: {}", filename);
                    load_plugin(entry.path().string());
                }
            }
        }
    }


    std::vector<ToolInfo> PluginManager::get_tools_from_plugin(const std::string &plugin_path) const {
        std::string plugin_name = std::filesystem::path(plugin_path).filename().string();
        MCP_INFO("get_tools_from_plugin: looking for '{}'", plugin_name);

        auto it = plugins_.find(plugin_name);
        if (it == plugins_.end()) {
            MCP_WARN("Plugin '{}' not found", plugin_name);
            return {};
        }

        MCP_INFO("Found plugin '{}' with {} tools", plugin_name, it->second->tool_list.size());
        return it->second->tool_list;
    }

    std::vector<ToolInfo> PluginManager::get_all_tools() const {
        std::vector<ToolInfo> all_tools;
        for (const auto &[name, plugin]: plugins_) {
            all_tools.insert(all_tools.end(), plugin->tool_list.begin(), plugin->tool_list.end());
        }
        return all_tools;
    }

    nlohmann::json PluginManager::call_tool(const std::string &name, const nlohmann::json &args) {
        MCP_INFO("Calling tool: '{}'", name);
        for (const auto &[plugin_name, plugin]: plugins_) {
            for (const auto &tool: plugin->tool_list) {
                if (tool.name && std::string(tool.name) == name) {
                    try {
                        std::string args_json = args.dump();
                        set_current_plugin(plugin.get());

                        // Create MCPError object to receive plugin errors
                        MCPError error = {0, nullptr, nullptr, nullptr};
                        const char *result_json = plugin->call_tool(name.c_str(), args_json.c_str(), &error);
                        set_current_plugin(nullptr);

                        // Check if there is error information
                        if (error.code != 0) {
                            MCP_CRITICAL("Error calling tool: {}", error.message);
                            return nlohmann::json{
                                    {"error", {{"code", error.code},// Use the error code returned by the plugin
                                               {"message", error.message ? error.message : "Unknown error"}}}};
                        }

                        if (!result_json) {
                            return nlohmann::json{
                                    {"error", {{"code", -32603},// INTERNAL_ERROR
                                               {"message", "Tool returned null result"}}}};
                        }

                        // Parse the JSON returned by the plugin
                        auto result = nlohmann::json::parse(result_json);
                        plugin->free_result(result_json);

                        // If the JSON returned by the plugin contains an error field, return the error directly
                        if (result.contains("error")) {
                            // Use the error code and message returned by the plugin
                            return nlohmann::json{
                                    {"error", {{"code", result["error"].value("code", -32603)}, {"message", result["error"].value("message", "Unknown error")}}}};
                        }

                        // Return result on success
                        return result;
                    } catch (const std::exception &e) {
                        MCP_ERROR("Error calling tool '{}': {}", name, e.what());
                        return nlohmann::json{
                                {"error", {{"code", -32603},// INTERNAL_ERROR
                                           {"message", e.what()}}}};
                    }
                }
            }
        }
        return nlohmann::json{
                {"error", {{"code", -32601},// METHOD_NOT_FOUND
                           {"message", "Tool not found: " + name}}}};
    }

    std::optional<std::string> PluginManager::find_plugin_name_for_tool(const std::string &tool_name) const {
        for (const auto &[plugin_name, plugin_ptr]: plugins_) {
            for (const auto &tool_info: plugin_ptr->tool_list) {
                if (tool_info.name && std::string(tool_info.name) == tool_name) {
                    return plugin_name;// Return when found
                }
            }
        }
        return std::nullopt;// Not found
    }

    PluginManager::StreamFunctions PluginManager::get_stream_functions(StreamGenerator generator) const {
        std::lock_guard<std::mutex> lock(generator_mutex_);
        auto it = generator_to_plugin_.find(generator);
        if (it != generator_to_plugin_.end()) {
            Plugin *plugin = it->second;
            StreamGeneratorNext next_func = plugin->get_stream_next ? plugin->get_stream_next() : nullptr;
            StreamGeneratorFree free_func = plugin->get_stream_free ? plugin->get_stream_free() : nullptr;
            return {next_func, free_func, {0, nullptr, nullptr, nullptr}};
        }
        // Return error if plugin not found
        MCPError error = {
                -1,
                "Plugin not found for generator",
                nullptr,
                "PluginManager::get_stream_functions"};
        return {nullptr, nullptr, error};
    }

    StreamGenerator PluginManager::start_streaming_tool(const std::string &name, const nlohmann::json &args, MCPError *out_error) {
        // Initialize output error
        if (out_error) {
            out_error->code = 0;
            out_error->message = nullptr;
        }

        for (const auto &[plugin_name, plugin]: plugins_) {
            for (const auto &tool: plugin->tool_list) {
                if (tool.name && std::string(tool.name) == name && tool.is_streaming) {
                    try {
                        std::string args_json = args.dump();
                        MCPError error = {0, nullptr, nullptr, nullptr};
                        const char *raw_result = plugin->call_tool(name.c_str(), args_json.c_str(), &error);

                        // Check plugin returned error
                        if (error.code != 0) {
                            if (out_error) {
                                *out_error = error;
                            }
                            return nullptr;
                        }

                        if (!raw_result) {
                            if (out_error) {
                                out_error->code = -32603;// INTERNAL_ERROR
                                out_error->message = "Plugin returned null for streaming tool";
                            }
                            return nullptr;
                        }

                        StreamGenerator generator = reinterpret_cast<StreamGenerator>(const_cast<char *>(raw_result));
                        {
                            std::lock_guard<std::mutex> lock(generator_mutex_);
                            generator_to_plugin_[generator] = plugin.get();
                        }
                        return generator;
                    } catch (const std::exception &e) {
                        if (out_error) {
                            out_error->code = -32603;// INTERNAL_ERROR
                            out_error->message = e.what();
                        }
                        return nullptr;
                    }
                }
            }
        }

        if (out_error) {
            out_error->code = -32601;// METHOD_NOT_FOUND
            out_error->message = "Streaming tool not found or not marked as streaming";
        }
        return nullptr;
    }
}// namespace mcp::business