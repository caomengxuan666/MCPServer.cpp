#pragma once
#include "protocol/json_rpc.h"
#include "tool_registry.h"
#include "transport/session.h"
#include <memory>
#include <version.h>

namespace mcp::routers {
    /**
     * @brief Handle initialization request
     * @param req RPC request
     * @return Response with server capabilities and version info
     */
    inline protocol::Response handle_initialize(
            const protocol::Request &req,
            std::shared_ptr<business::ToolRegistry> /*registry*/,
            std::shared_ptr<transport::Session> /*session*/,
            const std::string & /*session_id*/) {
        protocol::Response resp;
        resp.id = req.id.value_or(nullptr);

        // get client's request protocol version
        std::string client_protocol_version = req.params.value("protocolVersion", "2025-01-07");
        std::string server_version = PROJECT_VERSION;
        std::string server_name = PROJECT_NAME;

        resp.result = nlohmann::json{
                {"protocolVersion", client_protocol_version},
                {"capabilities", nlohmann::json({{"logging", nlohmann::json::object()},
                                                 {"prompts", {{"listChanged", true}}},
                                                 {"resources", {{"listChanged", true}, {"subscribe", true}}},
                                                 {"tools", {{"listChanged", true}}}})},
                {"serverInfo", {{"name", server_name}, {"version", server_version}}}};

        return resp;
    }
}// namespace mcp::routers
