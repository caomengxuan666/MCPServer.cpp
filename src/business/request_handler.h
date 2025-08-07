// src/business/request_handler.h
#pragma once

#include "rpc_router.h"
#include "tool_registry.h"
#include "transport/stdio_transport.h"
#include <memory>

namespace mcp::transport {
    class Session;
}
namespace mcp::business {

    class RequestHandler {
    public:
        using ResponseCallback = std::function<void(
                const std::string &response,
                std::shared_ptr<transport::Session> session,
                const std::string &session_id)>;

        explicit RequestHandler(std::shared_ptr<ToolRegistry> registry, ResponseCallback send_response);

        void handle_request(const std::string &msg, std::shared_ptr<transport::Session> session, const std::string &session_id);

    private:
        std::shared_ptr<ToolRegistry> registry_;
        mcp::transport::StdioTransport transport_;
        RpcRouter router_;
        ResponseCallback send_response_;
    };

}// namespace mcp::business