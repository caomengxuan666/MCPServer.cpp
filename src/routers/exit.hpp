#include "core/logger.h"
#include "protocol/json_rpc.h"
#include "request_handler.h"

namespace mcp::routers {

    /**
     * @brief Handle exit request
     * @param req RPC request
     * @return Terminates application
     */
    inline protocol::Response handle_exit(
            const protocol::Request & /*req*/,
            std::shared_ptr<business::ToolRegistry> /*registry*/,
            std::shared_ptr<transport::Session> /*session*/,
            const std::string & /*session_id*/) {
        MCP_INFO("Exit command received");
        std::exit(0);
    }
}// namespace mcp::routers