// plugins/official/http_plugin/http_plugin.cpp
#include "core/mcpserver_api.h"
#include "httplib.h"// Include httplib
#include "mcp_plugin.h"
#include "tool_info_parser.h"
#include <cstdlib>
#include <nlohmann/json.hpp>



static std::vector<ToolInfo> g_tools;

// Send HTTP GET request
static std::string http_get(const std::string &url) {
    httplib::Client client(url.c_str());

    auto res = client.Get("/");
    if (!res) {
        return nlohmann::json{
                {"error", "Request failed"},
                {"message", "Network error or invalid URL"}}
                .dump();
    }

    return nlohmann::json{
            {"status_code", res->status},
            {"body", res->body},
            {"headers", res->headers}}
            .dump();
}

// Send HTTP POST request (application/json)
static std::string http_post(const std::string &url, const std::string &body) {
    httplib::Client client(url.c_str());

    auto res = client.Post("/", body, "application/json");
    if (!res) {
        return nlohmann::json{
                {"error", "Request failed"},
                {"message", "Network error or invalid URL"}}
                .dump();
    }

    return nlohmann::json{
            {"status_code", res->status},
            {"body", res->body},
            {"headers", res->headers}}
            .dump();
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

extern "C" MCP_API const char *call_tool(const char *name, const char *args_json) {
    try {
        auto args = nlohmann::json::parse(args_json);
        std::string tool_name = name;

        if (tool_name == "http_get") {
            std::string url = args.value("url", "");
            if (url.empty()) {
                return strdup(R"({"error": "Missing 'url' parameter"})");
            }
            std::string result = http_get(url);
            return strdup(result.c_str());
        } else if (tool_name == "http_post") {
            std::string url = args.value("url", "");
            std::string body = args.value("body", "");
            if (url.empty()) {
                return strdup(R"({"error": "Missing 'url' parameter"})");
            }
            std::string result = http_post(url, body);
            return strdup(result.c_str());
        } else {
            return strdup(R"({"error": "Unknown tool"})");
        }
    } catch (const std::exception &e) {
        return strdup((R"({"error": ")" + std::string(e.what()) + R"("})").c_str());
    }
}

extern "C" MCP_API void free_result(const char *result) {
    if (result) {
        std::free(const_cast<char *>(result));
    }
}