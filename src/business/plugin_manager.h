#pragma once

// 确保 ASIO 在 Windows 上正确配置 WinSock
#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0601
#endif

#include "mcp_plugin.h"
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
using lib_handle = HMODULE;
#define LOAD_LIB(name) LoadLibraryA(name)
#define GET_FUNC(lib, name) GetProcAddress(lib, name)
#define CLOSE_LIB(lib) FreeLibrary(lib)
#else
#include <dlfcn.h>
using lib_handle = void *;
#define LOAD_LIB(name) dlopen(name, RTLD_LAZY)
#define GET_FUNC(lib, name) dlsym(lib, name)
#define CLOSE_LIB(lib) dlclose(lib)
#endif

namespace mcp::business {

    class PluginManager {
    public:
        struct Plugin {
            lib_handle handle;
            get_tools_func get_tools;
            call_tool_func call_tool;
            free_result_func free_result;
            std::vector<ToolInfo> tool_list;
            get_stream_next_func get_stream_next;
            get_stream_free_func get_stream_free;
        };

        ~PluginManager();

        bool load_plugin(const std::string &path);

        // get tools from a specific plugin
        std::vector<ToolInfo> get_tools_from_plugin(const std::string &plugin_path) const;

        // get all tools from all plugins(for debugging)
        std::vector<ToolInfo> get_all_tools() const;

        // call a tool by name with JSON arguments
        nlohmann::json call_tool(const std::string &name, const nlohmann::json &args);

        std::optional<std::string> find_plugin_name_for_tool(const std::string &tool_name) const;

        StreamGenerator start_streaming_tool(const std::string &name, const nlohmann::json &args);

        std::pair<StreamGeneratorNext, StreamGeneratorFree> get_stream_functions(StreamGenerator generator) const;

        void set_current_plugin(Plugin *plugin) { current_plugin_ = plugin; }


    private:
        std::unordered_map<std::string, std::unique_ptr<Plugin>> plugins_;// name -> Plugin mapping
        std::vector<std::string> load_order_;                             // to keep track of load order
        Plugin *current_plugin_ = nullptr;
        mutable std::mutex generator_mutex_;// 线程安全
        std::unordered_map<StreamGenerator, Plugin *> generator_to_plugin_;
    };

}// namespace mcp::business
