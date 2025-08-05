// src/business/tool_registry.h
#pragma once

#include "mcp_plugin.h"
#include "protocol/tool.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>


namespace mcp::business {

    // Tool execution function signature
    using ToolExecutor = std::function<nlohmann::json(const nlohmann::json &)>;

    // Tool metadata + executor
    struct RegisteredTool {
        mcp::protocol::Tool metadata;
        ToolExecutor executor;
    };

    class PluginManager;

    class ToolRegistry {
    public:
        // Register built-in tools
        void register_builtin(const mcp::protocol::Tool &tool, ToolExecutor exec);

        void set_plugin_manager(std::shared_ptr<PluginManager> pm);
        std::shared_ptr<PluginManager> get_plugin_manager() const;

        // Plugin tools loaded from PluginManager
        void register_plugin_tool(const ToolInfo &info, ToolExecutor exec);
        std::vector<std::string> get_all_tool_names() const;
        // Get all tools as protocol::Tool objects for debugging
        std::vector<mcp::protocol::Tool> get_all_tools() const;
        // Execute tool
        std::shared_ptr<mcp::protocol::Tool> get_tool_info(const std::string &name) const;
        std::optional<nlohmann::json> execute(const std::string &name, const nlohmann::json &args);

    private:
        std::unordered_map<std::string, RegisteredTool> tools_;
        std::shared_ptr<PluginManager> plugin_manager_;
    };

}// namespace mcp::business