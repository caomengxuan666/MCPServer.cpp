#include "python_plugin_instance.h"
#include "core/logger.h"
#include "python_runtime_manager.h"
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;
namespace mcp::business {
    PythonPluginInstance::PythonPluginInstance() : initialized_(false) {
    }

    PythonPluginInstance::~PythonPluginInstance() {
        uninitialize();
    }

    bool PythonPluginInstance::initialize(const char *plugin_path) {
        MCP_DEBUG("[PLUGIN] PythonPluginInstance::initialize called with path: {}", plugin_path);

        try {
            std::string plugin_path_str(plugin_path);
            fs::path path(plugin_path_str);

            // Extract directory and module name
            plugin_dir_ = path.parent_path().string();
            module_name_ = path.stem().string();

            MCP_DEBUG("[PLUGIN] plugin_dir: {}, module_name: {}", plugin_dir_, module_name_);

            // Check if Python file exists
            fs::path python_file_path = path.parent_path() / (module_name_ + ".py");
            if (!fs::exists(python_file_path)) {
                MCP_ERROR("[PLUGIN] Python file does not exist: {}", python_file_path.string());
                return false;
            }

            MCP_DEBUG("[PLUGIN] Python file found: {}", python_file_path.string());

            // Get the Python runtime manager instance
            auto &runtime_manager = PythonRuntimeManager::getInstance();

            // Initialize Python runtime if not already done
            if (!runtime_manager.isInitialized()) {
                if (!runtime_manager.initialize(plugin_dir_)) {
                    MCP_ERROR("[PLUGIN] Failed to initialize Python runtime");
                    return false;
                }
            }

            // Load the plugin module
            if (!initialize_plugin_module()) {
                MCP_ERROR("[PLUGIN] Failed to load plugin module");
                return false;
            }

            initialized_ = true;
            MCP_INFO("[PLUGIN] Plugin instance initialized successfully");
            return true;
        } catch (const std::exception &e) {
            MCP_ERROR("[PLUGIN] Error initializing plugin: {}", e.what());
            return false;
        }
    }

    void PythonPluginInstance::uninitialize() {
        std::lock_guard<std::mutex> lock(cache_mutex_);

        for (auto &tool: tools_cache_) {
            free((void *) tool.name);
            free((void *) tool.description);
            free((void *) tool.parameters);
        }
        tools_cache_.clear();

        plugin_module_ = py::module_();

        initialized_ = false;
        MCP_DEBUG("[PLUGIN] Plugin instance uninitialized");
    }

    bool PythonPluginInstance::initialize_plugin_module() {
        try {
            MCP_DEBUG("[PLUGIN] Initializing plugin module: {}", module_name_);

            // Critical fix: Must acquire GIL before importing module (main thread GIL has been released)
            MCP_DEBUG("[PLUGIN] initialize_plugin_module: acquiring GIL for import...");
            py::gil_scoped_acquire acquire;// Explicitly acquire GIL
            MCP_DEBUG("[PLUGIN] initialize_plugin_module: GIL acquired");

            // Get the Python runtime manager instance
            auto &runtime_manager = PythonRuntimeManager::getInstance();

            // Import the plugin module (now safe to call Python API under GIL protection)
            MCP_DEBUG("[PLUGIN] Trying to import Python module: {}", module_name_);
            plugin_module_ = runtime_manager.importModule(module_name_);
            MCP_DEBUG("[PLUGIN] Python module imported successfully: {}", module_name_);

            // Validate module has required functions
            if (!py::hasattr(plugin_module_, "get_tools") ||
                !py::hasattr(plugin_module_, "call_tool")) {
                MCP_ERROR("[PLUGIN] Module missing required functions");
                return false;
            }

            MCP_INFO("[PLUGIN] Plugin module loaded successfully");
            return true;
        } catch (const py::error_already_set &) {
            // Print detailed Python exception (including stack trace) to help locate import failure
            // MCP_ERROR("[PLUGIN] Python traceback: {}", e.trace());
            return false;
        } catch (const std::exception &e) {
            MCP_ERROR("[PLUGIN] C++ error loading module '{}': {}", module_name_, e.what());
            return false;
        }
    }
    ToolInfo *PythonPluginInstance::get_tools(int *count) {
        MCP_DEBUG("[PLUGIN] get_tools called, initialized={}", (initialized_ ? "true" : "false"));

        if (!initialized_) {
            MCP_ERROR("[PLUGIN] Error: Plugin not initialized before calling get_tools");
            *count = 0;
            return nullptr;
        }

        try {
            py::gil_scoped_acquire acquire;

            std::lock_guard<std::mutex> lock(cache_mutex_);

            // Clear the cache
            for (auto &tool: tools_cache_) {
                free((void *) tool.name);
                free((void *) tool.description);
                free((void *) tool.parameters);
            }
            tools_cache_.clear();

            // Call Python function
            py::object get_tools_func = plugin_module_.attr("get_tools");
            py::list tools_list = get_tools_func();

            *count = static_cast<int>(py::len(tools_list));
            tools_cache_.reserve(*count);

            // Convert Python objects to ToolInfo structures
            for (const auto &tool_obj: tools_list) {
                ToolInfo tool{};

                // Extract string fields and make copies
                std::string name = py::str(tool_obj.attr("name")).cast<std::string>();
                std::string description = py::str(tool_obj.attr("description")).cast<std::string>();
                std::string parameters = py::str(tool_obj.attr("parameters")).cast<std::string>();

                // Allocate memory for strings
                tool.name = strdup(name.c_str());
                tool.description = strdup(description.c_str());
                tool.parameters = strdup(parameters.c_str());

                tool.is_streaming = tool_obj.attr("is_streaming").cast<bool>();

                tools_cache_.push_back(tool);
            }

            return tools_cache_.data();
        } catch (const py::error_already_set &e) {
            MCP_ERROR("[PLUGIN] Python error in get_tools: {}", e.what());
            *count = 0;
            return nullptr;
        } catch (const std::exception &e) {
            MCP_ERROR("[PLUGIN] C++ error in get_tools: {}", e.what());
            *count = 0;
            return nullptr;
        } catch (...) {
            MCP_ERROR("[PLUGIN] Unknown error in get_tools");
            *count = 0;
            return nullptr;
        }
    }

    const char *PythonPluginInstance::call_tool(const char *name, const char *args_json, MCPError *error) {
        MCP_DEBUG("[PLUGIN] call_tool called, initialized={}", (initialized_ ? "true" : "false"));

        // 1. First check basic parameter validity (avoid hidden crashes caused by null pointers)
        if (!name || strlen(name) == 0) {
            MCP_ERROR("[PLUGIN] call_tool: ERROR - tool name is null/empty");
            if (error) {
                error->code = -1;
                error->message = strdup("Invalid tool name");
            }
            return nullptr;
        }
        const char *actual_args = args_json ? args_json : "{}";// Fallback to empty JSON
        MCP_DEBUG("[PLUGIN] call_tool: tool name={}, args_json={}", name, actual_args);

        if (!initialized_) {
            MCP_ERROR("[PLUGIN] call_tool: ERROR - plugin not initialized");
            if (error) {
                error->code = -1;
                error->message = strdup("Plugin not initialized");
            }
            return nullptr;
        }

        // 2. Check if Python module is valid (exclude module not loaded)
        if (plugin_module_.is_none()) {
            MCP_ERROR("[PLUGIN] call_tool: ERROR - plugin_module_ is empty");
            if (error) {
                error->code = -1;
                error->message = strdup("Plugin module not loaded");
            }
            return nullptr;
        }
        MCP_DEBUG("[PLUGIN] call_tool: plugin_module_ is valid");

        try {
            // 3. Key: Acquire GIL (most likely blocking point)
            MCP_DEBUG("[PLUGIN] call_tool: trying to acquire GIL...");
            py::gil_scoped_acquire acquire;// Acquire GIL, will block on failure
            MCP_DEBUG("[PLUGIN] call_tool: GIL acquired successfully");

            // 4. Find the call_tool function in Python layer (exclude function not exported)
            MCP_DEBUG("[PLUGIN] call_tool: looking for 'call_tool' in Python module");
            if (!py::hasattr(plugin_module_, "call_tool")) {
                MCP_ERROR("[PLUGIN] call_tool: ERROR - Python module has no 'call_tool' function");
                if (error) {
                    error->code = -1;
                    error->message = strdup("Python call_tool not found");
                }
                return nullptr;
            }
            py::object call_tool_func = plugin_module_.attr("call_tool");
            MCP_DEBUG("[PLUGIN] call_tool: got Python call_tool function");

            // 5. Call Python function (if we reach this step, previous steps have no issues)
            MCP_DEBUG("[PLUGIN] call_tool: calling Python call_tool (name={})", name);
            py::object result = call_tool_func(py::str(name), py::str(actual_args));
            MCP_DEBUG("[PLUGIN] call_tool: Python call_tool returned");

            // 6. Process return result
            std::string result_str = py::str(result).cast<std::string>();
            MCP_DEBUG("[PLUGIN] call_tool: Python result={}", result_str);
            const char *ret = strdup(result_str.c_str());
            return ret;

        } catch (const py::error_already_set &e) {
            // Catch Python exceptions (e.g. JSON parsing errors, tool not found)
            MCP_ERROR("[PLUGIN] call_tool: PYTHON ERROR - {}", e.what());

            // MCP_ERROR("[PLUGIN] call_tool: PYTHON TRACEBACK - {}", e.trace());
            if (error) {
                error->code = -1;
                error->message = strdup(e.what());
            }
            return nullptr;
        } catch (const std::exception &e) {
            // Catch C++ exceptions (e.g. memory allocation failure)
            MCP_ERROR("[PLUGIN] call_tool: C++ ERROR - {}: {}", typeid(e).name(), e.what());
            if (error) {
                error->code = -1;
                error->message = strdup(e.what());
            }
            return nullptr;
        } catch (...) {
            // Catch unknown exceptions
            MCP_ERROR("[PLUGIN] call_tool: UNKNOWN ERROR");
            if (error) {
                error->code = -1;
                error->message = strdup("Unknown error");
            }
            return nullptr;
        }
    }
}// namespace mcp::business