// src/core/mcp_dispatcher.h
#pragma once

#include "transport/session.h"
#include <memory>

namespace mcp::core {

    // now the class only handle json response
    class McpDispatcher {
    public:
        explicit McpDispatcher();
        static void send_sse_error_event(std::shared_ptr<mcp::transport::Session> session, const std::string &message);
        static void send_json_response(std::shared_ptr<mcp::transport::Session> session, const std::string &json_body, int status_code);

    };

}// namespace mcp::core