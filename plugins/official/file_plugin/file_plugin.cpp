// plugins/official/file_plugin/file_plugin.cpp
#include "core/mcpserver_api.h"
#include "mcp_plugin.h"
#include "protocol/json_rpc.h"
#include "tool_info_parser.h"
#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>


static std::vector<ToolInfo> g_tools;

static std::string read_file(const std::string &path) {
    try {
        std::ifstream f(path);
        if (!f.is_open()) {
            // Return custom error code and message, consistent with safe_system_plugin
            return nlohmann::json{
                    {"error", {{"code", -32000},// Custom error code
                               {"message", "File not found or cannot open"}}}}
                    .dump();
        }
        std::stringstream buffer;
        buffer << f.rdbuf();
        return nlohmann::json{{"content", buffer.str()}}.dump();
    } catch (const std::exception &e) {
        // Return custom error code and message, consistent with safe_system_plugin
        return nlohmann::json{
                {"error", {{"code", -32000},// Custom error code
                           {"message", "Failed to read file: " + std::string(e.what())}}}}
                .dump();
    }
}

static std::string write_file(const std::string &path, const std::string &content) {
    try {
        std::ofstream f(path);
        if (!f.is_open()) {
            // Return custom error code and message, consistent with safe_system_plugin
            return nlohmann::json{
                    {"error", {{"code", -32000},// Custom error code
                               {"message", "Cannot open file for writing"}}}}
                    .dump();
        }
        f << content;
        f.close();
        return R"({"result": "success"})";
    } catch (const std::exception &e) {
        // Return custom error code and message, consistent with safe_system_plugin
        return nlohmann::json{
                {"error", {{"code", -32000},// Custom error code
                           {"message", "Failed to write file: " + std::string(e.what())}}}}
                .dump();
    }
}

static std::string list_files(const std::string &path) {
    // This is a simplified implementation that just returns the path
    // A real implementation would actually list files in the directory
    return nlohmann::json{{"path", path}}.dump();
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

extern "C" MCP_API const char *call_tool(const char *name, const char *args_json, MCPError *error) {
    try {
        auto args = nlohmann::json::parse(args_json);
        std::string tool_name = name;

        if (tool_name == "read_file") {
            std::string file_path = args.value("path", "");
            if (file_path.empty()) {
                // Use MCPError to return error instead of constructing JSON string
                error->code = mcp::protocol::error_code::INVALID_TOOL_INPUT;
                error->message = "Missing 'path' parameter";
                return nullptr;
            }
            std::string result = read_file(file_path);
            return strdup(result.c_str());
        } else if (tool_name == "write_file") {
            std::string file_path = args.value("path", "");
            std::string content = args.value("content", "");
            if (file_path.empty()) {
                // Use MCPError to return error instead of constructing JSON string
                error->code = mcp::protocol::error_code::INVALID_TOOL_INPUT;
                error->message = "Missing 'path' parameter";
                return nullptr;
            }
            std::string result = write_file(file_path, content);
            return strdup(result.c_str());
        } else if (tool_name == "list_files") {
            std::string dir_path = args.value("path", ".");
            std::string result = list_files(dir_path);
            return strdup(result.c_str());
        } else {
            // Use MCPError to return error instead of constructing JSON string
            error->code = mcp::protocol::error_code::TOOL_NOT_FOUND;
            error->message = "Unknown tool";
            return nullptr;
        }
    } catch (const std::exception &e) {
        // Use MCPError to return error instead of constructing JSON string
        error->code = mcp::protocol::error_code::INTERNAL_ERROR;
        error->message = e.what();
        return nullptr;
    }
}

extern "C" MCP_API void free_result(const char *result) {
    if (result) {
        std::free(const_cast<char *>(result));
    }
}