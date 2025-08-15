#pragma once

#include "business/tool_registry.h"
#include "protocol/json_rpc.h"
#include "transport/session.h"
#include <memory>
#include <string>

namespace mcp::routers {

    protocol::Response handle_resources_list(
            const protocol::Request &req,
            std::shared_ptr<business::ToolRegistry> registry,
            std::shared_ptr<transport::Session> session,
            const std::string &session_id);

}// namespace mcp::routers