// src/business/request_handler.cpp
#include "request_handler.h"
#include "core/logger.h"
#include "protocol/json_rpc.h"
#include "rpc_router.h"


namespace mcp::business {

    RequestHandler::RequestHandler(std::shared_ptr<ToolRegistry> registry, ResponseCallback send_response)
        : registry_(std::move(registry)), send_response_(std::move(send_response)) {
        // register route handler
        router_.register_handler("initialize", handle_initialize);
        router_.register_handler("tools/list", handle_tools_list);
        router_.register_handler("tools/call", handle_tools_call);
        router_.register_handler("exit", handle_exit);
    }

    void RequestHandler::handle_request(
            const std::string &msg,
            std::shared_ptr<transport::Session> session,
            const std::string &session_id) {
        MCP_DEBUG("Raw message: {}", msg);
        auto req = mcp::protocol::parse_request(msg);
        if (!req) {
            // invalid request
            std::string err = protocol::make_error(
                    protocol::error_code::INVALID_REQUEST,
                    "Invalid JSON-RPC request", nullptr);
            send_response_(err, session, session_id);
            return;
        }

        //handle routers
        auto response = router_.route_request(*req, registry_, session, session_id);

        // Only send unStreaming responses
        if (response.id != nullptr) {
            std::string resp_str = protocol::make_response(response);
            send_response_(resp_str, session, session_id);
        }
    }

}// namespace mcp::business