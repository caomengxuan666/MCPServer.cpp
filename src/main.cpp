#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0601
#endif

#include "core/logger.h"
#include "core/server.h"
int main() {
    try {
        // 1. init the logger
        mcp::core::initializeAsyncLogger("mcp-server.log", "info");
        mcp::core::MCPLogger::instance()->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        MCP_INFO("MCPServer.cpp started");

        // 2. construct and build our server
        auto server = mcp::core::MCPserver::Builder{}
                              .with_plugin_path("plugins")
                              .build();

        MCP_INFO("MCPServer.cpp is ready. Send JSON-RPC messages via /mcp.");

        server->run();

        return 0;
    } catch (const std::exception &e) {
        MCP_CRITICAL("Critical error: {}", e.what());
        return 1;
    }
}