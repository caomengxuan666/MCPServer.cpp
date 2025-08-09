// src/core/server.h
#pragma once
#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0601
#endif

#include "business/plugin_manager.h"
#include "business/request_handler.h"
#include "business/tool_registry.h"
#include "mcp_dispatcher.h"
#include "transport/http_transport.h"
#include <asio/io_context.hpp>
#include <memory>
#include <vector>


namespace mcp::core {

    class MCPserver {
    public:
        class Builder;

        void start();

        void run();
        McpDispatcher &get_dispatcher() { return *dispatcher_; }
        asio::io_context &get_io_context();

    private:
        MCPserver() = default;
        friend class Builder;

        bool start_http_transport(uint16_t port, const std::string &address);
        bool start_stdio_transport();
        std::shared_ptr<business::ToolRegistry> registry_;
        std::shared_ptr<business::PluginManager> plugin_manager_;
        std::unique_ptr<McpDispatcher> dispatcher_;
        std::unique_ptr<mcp::transport::HttpTransport> http_transport_;
        mcp::transport::StdioTransport stdio_transport_;
        std::unique_ptr<business::RequestHandler> request_handler_;

        // Used to record configuration
        bool should_register_echo_tool_ = false;

        std::vector<std::string> plugin_paths_;
        std::vector<std::string> plugin_directories_;


        asio::io_context io_context_;
    };

    class MCPserver::Builder {
    public:
        Builder();
        Builder &with_builtin_tools();
        Builder &with_echo_tool();
        Builder &with_plugin(const std::string &path);
        Builder &with_plugin_path(const std::string &path = "");
        Builder &with_address(const std::string &address = "0.0.0.0") {
            address_ = address;
            return *this;
        };
        Builder &with_port(unsigned short port = 6666) {
            port_ = port;
            return *this;
        }
        Builder &enableHttpTransport(bool enable = true) {
            enable_http_transport_ = enable;
            return *this;
        }
        Builder &enableStdioTransport(bool enable = true) {
            enable_stdio_transport_ = enable;
            return *this;
        }
        std::unique_ptr<MCPserver> build();

    private:
        std::unique_ptr<MCPserver> server_ = nullptr;
        bool enable_http_transport_ = false;
        bool enable_stdio_transport_ = false;

        std::string address_;
        unsigned short port_ = 0;
    };

}// namespace mcp::core