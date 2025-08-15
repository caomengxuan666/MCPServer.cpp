// src/business/request_handler.h
#pragma once

#include "business/rpc_router.h"
#include "business/tool_registry.h"
#include "transport/session.h"
#include <functional>
#include <memory>
#include <string>


namespace mcp::business {

    // Response callback function signature
    using ResponseCallback = std::function<void(
            const std::string &,                // response JSON string
            std::shared_ptr<transport::Session>,// session
            const std::string &                 // session ID
            )>;

    class RequestHandler {
    public:
        explicit RequestHandler(
                std::shared_ptr<ToolRegistry> registry,
                ResponseCallback send_response = nullptr);

        void handle_request(
                const std::string &msg,
                std::shared_ptr<transport::Session> session,
                const std::string &session_id);

    private:
        std::shared_ptr<ToolRegistry> registry_;
        ResponseCallback send_response_;
        RpcRouter router_;
    };

}// namespace mcp::business