// plugins/official/http_plugin/http_plugin.cpp
#include "core/mcpserver_api.h"
#include "httplib.h"// Include httplib
#include "mcp_plugin.h"
#include "protocol/json_rpc.h"
#include "tool_info_parser.h"
#include <cstdlib>
#include <nlohmann/json.hpp>



static std::vector<ToolInfo> g_tools;

// Send HTTP GET request
static std::string http_get(const std::string &url) {
    try {
        httplib::Client client(url.c_str());

        auto res = client.Get("/");
        if (!res) {
            // Return custom error code and message, consistent with safe_system_plugin
            return nlohmann::json{
                    {"error", {{"code", -32000},// Custom error code
                               {"message", "Request failed: Network error or invalid URL"}}}}
                    .dump();
        }

        return nlohmann::json{
                {"status_code", res->status},
                {"body", res->body},
                {"headers", res->headers}}
                .dump();
    } catch (const std::exception& e) {
        // Return custom error code and message, consistent with safe_system_plugin
        return nlohmann::json{
                {"error", {{"code", -32000},// Custom error code
                           {"message", "HTTP GET request failed: " + std::string(e.what())}}}}
                .dump();
    }
}

// Send HTTP POST request (application/json)
static std::string http_post(const std::string &url, const std::string &body) {
    try {
        httplib::Client client(url.c_str());

        auto res = client.Post("/", body, "application/json");
        if (!res) {
            // Return custom error code and message, consistent with safe_system_plugin
            return nlohmann::json{
                    {"error", {{"code", -32000},// Custom error code
                               {"message", "Request failed: Network error or invalid URL"}}}}
                    .dump();
        }

        return nlohmann::json{
                {"status_code", res->status},
                {"body", res->body},
                {"headers", res->headers}}
                .dump();
    } catch (const std::exception& e) {
        // Return custom error code and message, consistent with safe_system_plugin
        return nlohmann::json{
                {"error", {{"code", -32000},// Custom error code
                           {"message", "HTTP POST request failed: " + std::string(e.what())}}}}
                .dump();
    }
}

// Export functions
extern "C" MCP_API ToolInfo *get_tools(int *count) {
    try {
        if (g_tools.empty()) {
            g_tools = ToolInfoParser::loadFromFile("http_plugin_tools.json");
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

        if (tool_name == "http_get") {
            std::string url = args.value("url", "");
            if (url.empty()) {
                // Use MCPError to return error instead of constructing JSON string
                error->code = mcp::protocol::error_code::INVALID_TOOL_INPUT;
                error->message = "Missing 'url' parameter";
                return nullptr;
            }
            std::string result = http_get(url);
            return strdup(result.c_str());
        } else if (tool_name == "http_post") {
            std::string url = args.value("url", "");
            std::string body = args.value("body", "");
            if (url.empty()) {
                // Use MCPError to return error instead of constructing JSON string
                error->code = mcp::protocol::error_code::INVALID_TOOL_INPUT;
                error->message = "Missing 'url' parameter";
                return nullptr;
            }
            std::string result = http_post(url, body);
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