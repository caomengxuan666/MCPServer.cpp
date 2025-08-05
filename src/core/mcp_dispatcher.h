// src/core/mcp_dispatcher.h
#pragma once

#include "business/tool_registry.h"
#include "transport/session.h"
#include <memory>

namespace mcp::core {

    class McpDispatcher {
    public:
        explicit McpDispatcher(std::shared_ptr<business::ToolRegistry> registry);

        void handle_request(const std::string &json_message,
                            std::shared_ptr<mcp::transport::Session> session,
                            const std::string &session_id);

    private:
        std::shared_ptr<business::ToolRegistry> registry_;
        std::shared_ptr<business::PluginManager> plugin_manager_;
        void send_sse_error_event(std::shared_ptr<mcp::transport::Session> session, const std::string &message);
        void send_json_response(std::shared_ptr<mcp::transport::Session> session, const std::string &json_body, int status_code);
    };

}// namespace mcp::core