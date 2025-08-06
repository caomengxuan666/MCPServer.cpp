// plugins/official/file_plugin/file_plugin.cpp
#include "core/mcpserver_api.h"
#include "mcp_plugin.h"
#include "tool_info_parser.h"
#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

static std::vector<ToolInfo> g_tools;

static std::string read_file(const std::string &path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        return R"({"error": "File not found or cannot open"})";
    }
    std::stringstream buffer;
    buffer << f.rdbuf();
    return nlohmann::json{{"content", buffer.str()}}.dump();
}

extern "C" MCP_API ToolInfo *get_tools(int *count) {
    try {
        if (g_tools.empty()) {
            g_tools = ToolInfoParser::loadFromFile("file_plugin_tools.json");
        }

        *count = static_cast<int>(g_tools.size());
        return g_tools.data();
    } catch (const std::exception &e) {
        *count = 0;
        return nullptr;
    }
}

extern "C" MCP_API const char *call_tool(const char *name, const char *args_json) {
    try {
        auto args = nlohmann::json::parse(args_json);
        std::string tool_name = name;

        if (tool_name == "read_file") {
            std::string path = args.value("path", "");
            if (path.empty()) {
                return strdup(R"({"error": "Missing 'path' parameter"})");
            }
            std::string result = read_file(path);
            return strdup(result.c_str());
        }
        return strdup(R"({"error": "Unknown tool"})");
    } catch (const std::exception &e) {
        return strdup((R"({"error": ")" + std::string(e.what()) + R"("})").c_str());
    }
}

extern "C" MCP_API void free_result(const char *result) {
    if (result) {
        std::free(const_cast<char *>(result));
    }
}