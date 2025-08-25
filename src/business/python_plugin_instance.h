#ifndef PYTHON_PLUGIN_INSTANCE_H
#define PYTHON_PLUGIN_INSTANCE_H

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>
#include <vector>
#include <mutex>
#include "mcp_plugin.h"

namespace py = pybind11;

class PythonPluginInstance {
public:
    PythonPluginInstance();
    ~PythonPluginInstance();
    
    bool initialize(const char* plugin_path);
    void uninitialize();
    
    ToolInfo* get_tools(int* count);
    const char* call_tool(const char* name, const char* args_json, MCPError* error);
    
private:
    bool initialize_plugin_module();
    
    py::module_ plugin_module_;
    std::string plugin_dir_;
    std::string module_name_;
    std::vector<ToolInfo> tools_cache_;
    bool initialized_;
    std::mutex cache_mutex_;  
};

#endif // PYTHON_PLUGIN_INSTANCE_H