// src/business/request_handler.h
#pragma once

#include "tool_registry.h"
#include "transport/stdio_transport.h"
#include <memory>


namespace mcp::business {

    class RequestHandler {
    public:
        explicit RequestHandler(std::shared_ptr<ToolRegistry> registry);

        void handle_request(const std::string &msg);

    private:
        std::shared_ptr<ToolRegistry> registry_;
        mcp::transport::StdioTransport transport_;
    };

}// namespace mcp::business