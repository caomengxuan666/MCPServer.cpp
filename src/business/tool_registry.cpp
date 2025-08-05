#include "tool_registry.h"
#include "core/logger.h"

namespace mcp::business {

    void ToolRegistry::register_builtin(const mcp::protocol::Tool &tool, ToolExecutor exec) {
        if (tools_.count(tool.name)) {
            MCP_WARN("Built-in tool '{}' already exists, overwriting", tool.name);
        }
        tools_[tool.name] = {tool, std::move(exec)};
        MCP_TRACE("Registered builtin tool: {}", tool.name);
    }

    void ToolRegistry::register_plugin_tool(const ToolInfo &info, ToolExecutor exec) {
        try {
            // make sure the tool name is valid
            if (info.name == nullptr || info.name[0] == '\0') {
                MCP_ERROR("Invalid tool name (empty or null)");
                return;
            }
            std::string tool_name = info.name;

            // parse the tool info
            mcp::protocol::Tool tool;
            tool.name = tool_name;
            tool.description = info.description ? info.description : "";
            tool.is_streaming = info.is_streaming;
            if (info.parameters) {
                try {
                    tool.parameters = nlohmann::json::parse(info.parameters);
                } catch (const nlohmann::json::parse_error &e) {
                    MCP_ERROR("Failed to parse parameters for tool '{}': {}", tool_name, e.what());
                    MCP_ERROR("Invalid parameters: {}", info.parameters);
                    return;
                }
            }

            // insert the tool into the registry
            if (tools_.count(tool_name)) {
                MCP_WARN("Plugin tool '{}' already exists, overwriting", tool_name);
            }
            tools_[tool_name] = {tool, std::move(exec)};
            MCP_TRACE("Registered plugin tool: {}", tool_name);
            MCP_TRACE("Current registry size after adding '{}': {}", tool_name, tools_.size());

        } catch (const std::exception &e) {
            MCP_ERROR("Unexpected error registering tool: {}", e.what());
        }
    }

    std::optional<nlohmann::json> ToolRegistry::execute(const std::string &name, const nlohmann::json &args) {
        MCP_INFO("Querying tool: '{}' (registry size: {})", name, tools_.size());

        auto it = tools_.find(name);
        if (it == tools_.end()) {
            MCP_ERROR("Tool not found: '{}'", name);
            return std::nullopt;
        }

        try {
            return it->second.executor(args);
        } catch (const std::exception &e) {
            MCP_ERROR("Error executing tool '{}': {}", name, e.what());
            return std::nullopt;
        }
    }

    std::vector<std::string> ToolRegistry::get_all_tool_names() const {
        std::vector<std::string> names;
        for (const auto &[name, _]: tools_) {
            names.push_back(name);
        }
        return names;
    }
    std::shared_ptr<mcp::protocol::Tool> ToolRegistry::get_tool_info(const std::string &name) const {
        auto it = tools_.find(name);
        if (it != tools_.end()) {
            return std::make_shared<mcp::protocol::Tool>(it->second.metadata);
        }
        return nullptr;
    }
    std::vector<mcp::protocol::Tool> ToolRegistry::get_all_tools() const {
        std::vector<mcp::protocol::Tool> all_tools;
        for (const auto &[name, registered_tool]: tools_) {
            all_tools.push_back(registered_tool.metadata);
        }
        return all_tools;
    }

    void ToolRegistry::set_plugin_manager(std::shared_ptr<PluginManager> pm) {
        plugin_manager_ = std::move(pm);
    }

    std::shared_ptr<PluginManager> ToolRegistry::get_plugin_manager() const {
        return plugin_manager_;
    }
}// namespace mcp::business
