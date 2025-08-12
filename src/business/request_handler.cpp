#include "request_handler.h"
#include "core/logger.h"
#include "protocol/json_rpc.h"
#include "routers/exit.hpp"
#include "routers/initialize.hpp"
#include "routers/tool_list.hpp"
#include "routers/tools_call.hpp"
#include "rpc_router.h"
#include <iostream>


namespace mcp::business {

    using namespace routers;
    RequestHandler::RequestHandler(std::shared_ptr<ToolRegistry> registry, ResponseCallback send_response)
        : registry_(std::move(registry)), send_response_(std::move(send_response)) {
        // Register route handlers
        router_.register_handler("initialize", handle_initialize);
        router_.register_handler("tools/list", handle_tools_list);
        router_.register_handler("tools/call", handle_tools_call);
        router_.register_handler("exit", handle_exit);
        router_.register_handler("notifications/initialized", [](
                                                                      const protocol::Request &,
                                                                      std::shared_ptr<business::ToolRegistry> /*registry*/,
                                                                      std::shared_ptr<transport::Session>,
                                                                      const std::string &session_id) {
            MCP_DEBUG("Received notifications/initialized for session: {}", session_id);

            return protocol::Response{};// Return empty response for notifications
        });
        router_.register_handler("ping", [](
                                                 const protocol::Request &req,
                                                 std::shared_ptr<business::ToolRegistry> /*registry*/,
                                                 std::shared_ptr<transport::Session> /*session*/,
                                                 const std::string &session_id) {
            MCP_DEBUG("Received ping request (session: {})", session_id);

            protocol::Response resp;
            resp.id = req.id.value_or(nlohmann::json(nullptr));
            resp.result = nlohmann::json::object();// Empty object for ping response

            return resp;
        });
    }

    void RequestHandler::handle_request(
            const std::string &msg,
            std::shared_ptr<transport::Session> session,
            const std::string &session_id) {
        MCP_DEBUG("Raw message: {}", msg);
        // Parse JSON-RPC request
        auto [parsed_req, parse_error] = mcp::protocol::parse_request(msg);

        // Check if request parsing failed
        if (!parsed_req.has_value()) {
            // Generate appropriate error message
            std::string err;
            if (parse_error.has_value()) {
                err = protocol::make_error(parse_error.value());
            } else {
                err = protocol::make_error(
                        protocol::error_code::INVALID_REQUEST,
                        "Invalid JSON-RPC request format");
            }

            // Handle stdio transport (no session)
            if (!session) {
                std::cout << err << std::endl;
                return;
            }

            // Send error response through callback
            send_response_(err, session, session_id);
            return;
        }

        // Get valid request object from parsed result
        const protocol::Request &request = parsed_req.value();

        // Route request to appropriate handler
        auto response = router_.route_request(request, registry_, session, session_id);

        // Only send responses for non-notification requests (those with ID)
        if (!response.id.is_null()) {
            std::string resp_str = protocol::make_response(response);

            // Handle stdio transport (no session)
            if (!session) {
                std::cout << resp_str << std::endl;
                return;
            }

            // Send normal response through callback
            send_response_(resp_str, session, session_id);
        }
    }

}// namespace mcp::business