// src/transport/stdio_transport.h
#pragma once
#include <functional>
#include <string>

namespace mcp::transport {

    class StdioTransport {
    public:
        using MessageCallback = std::function<void(const std::string &)>;

        bool open(MessageCallback on_message);
        void close();
        bool write(const std::string &message);

    private:
        bool running_ = false;
    };

}// namespace mcp::transport