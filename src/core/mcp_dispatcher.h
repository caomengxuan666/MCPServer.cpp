// src/core/mcp_dispatcher.h
#pragma once

#include "business/tool_registry.h"
#include "transport/session.h"
#include <memory>

namespace mcp::core {

    class McpDispatcher {
    public:
        explicit McpDispatcher(std::shared_ptr<business::ToolRegistry> registry);
        void send_sse_error_event(std::shared_ptr<mcp::transport::Session> session, const std::string &message);
        void send_json_response(std::shared_ptr<mcp::transport::Session> session, const std::string &json_body, int status_code);

    private:
        std::shared_ptr<business::ToolRegistry> registry_;
        std::shared_ptr<business::PluginManager> plugin_manager_;
    };

}// namespace mcp::core