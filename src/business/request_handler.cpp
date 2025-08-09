// src/business/request_handler.cpp
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
        // register route handler
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

            return protocol::Response{};// return null response for those notofications
        });
        router_.register_handler("ping", [](
                                                 const protocol::Request &req,
                                                 std::shared_ptr<business::ToolRegistry> /*registry*/,
                                                 std::shared_ptr<transport::Session> /*session*/,
                                                 const std::string &session_id) {
            MCP_DEBUG("Received ping request (session: {})", session_id);

            protocol::Response resp;
            resp.id = req.id;
            resp.result = nlohmann::json::object();

            return resp;
        });
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
                    "Invalid JSON-RPC request");

            // for stdio write to stdout
            // if no session,it is stdio
            if (!session) {
                std::cout << err << std::endl;
                return;
            }

            send_response_(err, session, session_id);
            return;
        }

        //handle routers
        auto response = router_.route_request(*req, registry_, session, session_id);

        // Only send unStreaming responses
        if (response.id != nullptr) {
            std::string resp_str = protocol::make_response(response);

            // for stdio write to stdout
            //if no session,it is stdio
            if (!session) {
                std::cout << resp_str << std::endl;
                return;
            }

            send_response_(resp_str, session, session_id);
        }
    }

}// namespace mcp::business