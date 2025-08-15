#pragma once

#include "business/tool_registry.h"
#include "protocol/json_rpc.h"
#include "transport/session.h"
#include <memory>
#include <string>

namespace mcp::routers {

    protocol::Response handle_resources_subscribe(
            const protocol::Request &req,
            std::shared_ptr<business::ToolRegistry> registry,
            std::shared_ptr<transport::Session> session,
            const std::string &session_id);

    protocol::Response handle_resources_unsubscribe(
            const protocol::Request &req,
            std::shared_ptr<business::ToolRegistry> registry,
            std::shared_ptr<transport::Session> session,
            const std::string &session_id);

}// namespace mcp::routers