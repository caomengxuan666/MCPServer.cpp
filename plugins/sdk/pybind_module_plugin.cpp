#include <pybind11/pybind11.h>
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <filesystem>
#include "mcp_plugin.h"
#include "../src/core/mcpserver_api.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace py = pybind11;
namespace fs = std::filesystem;

// Global variables to hold the Python interpreter and module
static std::unique_ptr<py::scoped_interpreter> g_interpreter;
static py::module_ g_plugin_module;
static std::vector<ToolInfo> g_tools_cache;
static bool g_initialized = false;

// Initialize Python and load the plugin module
static bool initialize_python_plugin(const std::string& plugin_dir) {
    try {
        std::cerr << "[PLUGIN] Initializing Python interpreter in directory: " << plugin_dir << std::endl;
        
        // Initialize the Python interpreter if not already done
        if (!g_interpreter) {
            g_interpreter = std::make_unique<py::scoped_interpreter>();
            std::cerr << "[PLUGIN] Python interpreter initialized successfully" << std::endl;
        }
        
        // Add the plugin directory to Python path
        py::module_ sys = py::module_::import("sys");
        sys.attr("path").attr("append")(plugin_dir);
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize Python: " << e.what() << std::endl;
        return false;
    }
}

// Load the plugin module
static bool load_plugin_module(const std::string& module_name) {
    try {
        std::cerr << "[PLUGIN] Loading module: " << module_name << std::endl;
        g_plugin_module = py::module_::import(module_name.c_str());
        std::cerr << "[PLUGIN] Module loaded successfully" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load plugin module '" << module_name << "': " << e.what() << std::endl;
        return false;
    }
}

extern "C" {
    // Get tools function
    MCP_API ToolInfo* get_tools(int* count) {
        if (!g_initialized) {
            std::cerr << "[PLUGIN] Error: Plugin not initialized before calling get_tools" << std::endl;
            *count = 0;
            return nullptr;
        }
        
        try {
            // Clear the cache
            for (auto& tool : g_tools_cache) {
                // Clean up previous allocations
                delete[] const_cast<char*>(tool.name);
                delete[] const_cast<char*>(tool.description);
                delete[] const_cast<char*>(tool.parameters);
            }
            g_tools_cache.clear();
            
            // Call Python function
            py::object get_tools_func = g_plugin_module.attr("get_tools");
            py::list tools_list = get_tools_func();
            
            *count = static_cast<int>(py::len(tools_list));
            g_tools_cache.reserve(*count);
            
            // Convert Python objects to ToolInfo structures
            for (const auto& tool_obj : tools_list) {
                ToolInfo tool{};
                
                // Extract string fields and make copies
                std::string name = py::str(tool_obj.attr("name")).cast<std::string>();
                std::string description = py::str(tool_obj.attr("description")).cast<std::string>();
                std::string parameters = py::str(tool_obj.attr("parameters")).cast<std::string>();
                
                // Allocate memory for strings (will be freed by the server or when cache is cleared)
                tool.name = new char[name.length() + 1];
                tool.description = new char[description.length() + 1];
                tool.parameters = new char[parameters.length() + 1];
                
                std::strcpy(const_cast<char*>(tool.name), name.c_str());
                std::strcpy(const_cast<char*>(tool.description), description.c_str());
                std::strcpy(const_cast<char*>(tool.parameters), parameters.c_str());
                
                tool.is_streaming = tool_obj.attr("is_streaming").cast<bool>();
                
                g_tools_cache.push_back(tool);
            }
            
            return g_tools_cache.data();
        } catch (const std::exception& e) {
            std::cerr << "Error in get_tools: " << e.what() << std::endl;
            *count = 0;
            return nullptr;
        }
    }
    
    // Call tool function
    MCP_API const char* call_tool(const char* name, const char* args_json, MCPError* error) {
        if (!g_initialized) {
            std::cerr << "[PLUGIN] Error: Plugin not initialized before calling call_tool" << std::endl;
            if (error) {
                error->code = -1;
                std::string error_msg = "Plugin not initialized";
                error->message = new char[error_msg.length() + 1];
                std::strcpy(const_cast<char*>(error->message), error_msg.c_str());
            }
            return nullptr;
        }
        
        try {
            std::cerr << "[PLUGIN] Calling tool: " << name << std::endl;
            py::object call_tool_func = g_plugin_module.attr("call_tool");
            py::object result = call_tool_func(py::str(name), py::str(args_json));
            
            // Use Python's built-in string conversion to ensure proper encoding
            py::str result_str_obj = py::str(result);
            std::string result_str = result_str_obj.cast<std::string>();
            
            // Allocate memory for the result (will be freed by free_result)
            char* result_cstr = new char[result_str.length() + 1];
            std::strcpy(result_cstr, result_str.c_str());
            
            return result_cstr;
        } catch (const std::exception& e) {
            if (error) {
                error->code = -1;
                // Make a copy of the error message
                std::string error_msg = e.what();
                error->message = new char[error_msg.length() + 1];
                std::strcpy(const_cast<char*>(error->message), error_msg.c_str());
            }
            return nullptr;
        }
    }
    
    // Free result function
    MCP_API void free_result(const char* result) {
        delete[] result;
    }
    
    // Free error function
    MCP_API void free_error(MCPError* error) {
        if (error && error->message) {
            delete[] error->message;
        }
    }
    
    // Initialize plugin function
    MCP_API bool initialize_plugin(const char* plugin_path) {
        try {
            std::string plugin_path_str(plugin_path);
            fs::path path(plugin_path_str);
            
            // Extract directory and module name
            std::string plugin_dir = path.parent_path().string();
            std::string module_name = path.stem().string();
            
            std::cerr << "[PLUGIN] initialize_plugin called with path: " << plugin_path_str << std::endl;
            std::cerr << "[PLUGIN] plugin_dir: " << plugin_dir << ", module_name: " << module_name << std::endl;
            
            // Initialize Python
            if (!initialize_python_plugin(plugin_dir)) {
                std::cerr << "[PLUGIN] Failed to initialize Python plugin" << std::endl;
                return false;
            }
            
            // Load the plugin module
            if (!load_plugin_module(module_name)) {
                std::cerr << "[PLUGIN] Failed to load plugin module" << std::endl;
                return false;
            }
            
            g_initialized = true;
            std::cerr << "[PLUGIN] Plugin initialized successfully" << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error initializing plugin: " << e.what() << std::endl;
            return false;
        }
    }
}

// PYBIND11_MODULE definition with proper export - allows the DLL to also be imported as a Python module
#define PYBIND11_EXPORTS // Ensure proper symbol exports
PYBIND11_MODULE(mcp_python_plugin, m) {
    m.doc() = "MCPServer++ Python Plugin Support Module";
    
    // This is just for testing that the module can be imported
    m.def("test_initialization", []() {
        return "Python plugin support module loaded";
    });
}