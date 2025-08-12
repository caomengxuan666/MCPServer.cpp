#pragma once
#include "protocol/json_rpc.h"
#include "tool_registry.h"
#include "transport/session.h"
#include <memory>

namespace mcp::routers {
    /**
     * @brief Handle tool list request
     * @param req RPC request
     * @param registry Tool registry containing available tools
     * @return Response with list of tools and their metadata
     */
    inline protocol::Response handle_tools_list(
            const protocol::Request &req,
            std::shared_ptr<business::ToolRegistry> registry,
            std::shared_ptr<transport::Session> /*session*/,
            const std::string & /*session_id*/) {

        protocol::Response resp;
        resp.id = req.id.value_or(nullptr);

        auto tools = registry->get_all_tools();
        nlohmann::json tools_json = nlohmann::json::array();

        for (const auto &tool: tools) {
            nlohmann::json tool_json = {
                    {"name", tool.name},
                    {"description", tool.description}};

            if (!tool.parameters.is_null() && !tool.parameters.empty()) {
                tool_json["inputSchema"] = tool.parameters;
            }
            if (tool.is_streaming) {
                tool_json["isStreaming"] = true;
            }
            tools_json.push_back(tool_json);
        }

        resp.result = nlohmann::json{{"tools", tools_json}};
        return resp;
    }
}// namespace mcp::routers