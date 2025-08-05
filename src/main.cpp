#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0601
#endif

#include "core/logger.h"
#include "core/server.h"

int main() {
    try {
        // 1. init the logger
        mcp::core::initializeAsyncLogger("mcp-server.log", "trace");
        mcp::core::MCPLogger::instance()->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        MCP_INFO("MCPServer.cpp started");

        // 2. construct and build our server
        auto server = mcp::core::MCPserver::Builder{}
                              .with_builtin_tools()
                              .with_plugin("file_plugin.dll")
                              .with_plugin("http_plugin.dll")
                              .with_plugin("safe_system_plugin.dll")
                              .with_plugin("example_stream_plugin.dll")
                              .build();

        MCP_INFO("MCPServer.cpp is ready. Send JSON-RPC messages via /mcp.");

        server->run();

        return 0;
    } catch (const std::exception &e) {
        MCP_CRITICAL("Critical error: {}", e.what());
        return 1;
    }
}