// src/transport/stdio_transport.cpp
#include "stdio_transport.h"
#include "business/tool_registry.h"
#include "core/logger.h"
#include "protocol/json_rpc.h"
#include <iostream>
#include <sstream>
#include <thread>

namespace mcp::transport {

    StdioTransport::StdioTransport(std::shared_ptr<mcp::business::ToolRegistry> registry)
        : registry_(std::move(registry)) {
    }

    bool StdioTransport::open(MessageCallback on_message) {
        MCP_INFO("STDIO Transport started, waiting for input...");
        std::thread([this, cb = std::move(on_message)]() {
            std::string line;
            while (running_ && std::getline(std::cin, line)) {
                if (!line.empty()) {
                    MCP_DEBUG("Received raw message: {}", line);
                    cb(line);
                }
            }
        }).detach();
        running_ = true;
        return true;
    }

    bool StdioTransport::write(const std::string &message) {
        // Key: use std::flush or std::endl to force flushing
        std::cout << message << std::endl;// endl will automatically flush
        // std::cout << message << std::flush; // Or explicit flush
        return true;
    }

    void StdioTransport::close() {
        running_ = false;
    }

}// namespace mcp::transport