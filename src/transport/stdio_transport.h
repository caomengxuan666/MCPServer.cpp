// src/transport/stdio_transport.h
#pragma once
#include <functional>
#include <memory>
#include <string>

// Forward declarations
namespace mcp::business {
    class ToolRegistry;
}

namespace mcp::transport {

    class StdioTransport {
    public:
        explicit StdioTransport(std::shared_ptr<mcp::business::ToolRegistry> registry = nullptr);
        using MessageCallback = std::function<void(const std::string &)>;
        bool open(MessageCallback on_message);
        void close();
        bool write(const std::string &message);

    private:
        bool running_ = false;
        std::shared_ptr<mcp::business::ToolRegistry> registry_;
    };

}// namespace mcp::transport