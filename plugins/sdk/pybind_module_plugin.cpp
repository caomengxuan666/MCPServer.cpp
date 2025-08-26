/*
 * @Author: caomengxuan
 * @Date: 2025-08-25 12:56:20 
 * @note: We use this to load python plugins,but why not reuse it?
 * Because if every plugin load the same python runtime,it will cause a problem.
 * For example,if we have two plugins,one plugin use python3.9,another plugin use python3.10,
 * in this case,we could not load two different python versions.So use seperate python runtimes for each plugin
 * may be the best practice.
 * 
 * @Last Modified by: caomengxuan
 * @Last Modified time: 2025-08-25 12:59:17
 */
#pragma once

#include "../src/business/python_plugin_instance.h"
#include "../src/core/logger.h"
#include "../src/core/mcpserver_api.h"
#include "mcp_plugin.h"
#include <memory>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>
#include <unordered_map>


namespace py = pybind11;
namespace fs = std::filesystem;

using namespace mcp::business;


static std::unordered_map<std::string, std::unique_ptr<PythonPluginInstance>> g_plugin_instances;


extern "C" {
// Get tools function
//!Notice that we only use the first tool to make sure that every plugin is seperated.
MCP_API ToolInfo *get_tools(int *count) {
    if (g_plugin_instances.empty()) {
        MCP_ERROR("[PLUGIN] No plugin instances available");
        *count = 0;
        return nullptr;
    }

    PythonPluginInstance *instance = g_plugin_instances.begin()->second.get();
    return instance->get_tools(count);
}

// Call tool function
//!Notice that we only use the first tool to make sure that every plugin is seperated.
MCP_API const char *call_tool(const char *name, const char *args_json, MCPError *error) {
    if (g_plugin_instances.empty()) {
        MCP_ERROR("[PLUGIN] No plugin instances available");
        if (error) {
            error->code = -1;
            error->message = strdup("No plugin instances available");
        }
        return nullptr;
    }

    PythonPluginInstance *instance = g_plugin_instances.begin()->second.get();
    return instance->call_tool(name, args_json, error);
}

// Free result function
MCP_API void free_result(const char *result) {
    free((void *) result);
}

// Free error function
MCP_API void free_error(MCPError *error) {
    if (error) {
        if (error->message) {
            free((void *) error->message);
            error->message = nullptr;
        }
        error->code = 0;
    }
}

// Initialize plugin function
MCP_API bool initialize_plugin(const char *plugin_path) {
    MCP_DEBUG("[PLUGIN] initialize_plugin called with path: {}", plugin_path);

    try {
        std::string plugin_path_str(plugin_path);

        auto instance = std::make_unique<PythonPluginInstance>();
        if (!instance->initialize(plugin_path)) {
            MCP_ERROR("[PLUGIN] Failed to initialize plugin instance");
            return false;
        }

        g_plugin_instances[plugin_path_str] = std::move(instance);
        MCP_INFO("[PLUGIN] Plugin instance created and stored successfully");
        return true;
    } catch (const std::exception &e) {
        MCP_ERROR("[PLUGIN] Error creating plugin instance: {}", e.what());
        return false;
    }
}
MCP_API void uninitialize_plugin(const char *plugin_path) {
    MCP_DEBUG("[PLUGIN] uninitialize_plugin called with path: {}", plugin_path);

    auto it = g_plugin_instances.find(std::string(plugin_path));
    if (it != g_plugin_instances.end()) {
        it->second->uninitialize();
        g_plugin_instances.erase(it);
        MCP_INFO("[PLUGIN] Plugin instance removed successfully");
    }
}
}