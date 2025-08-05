/*
 * @Author: caomengxuan666 
 * @Date: 2025-08-05 17:00:56 
 * @Description: HTTP and SSE is not more supported, use Streamable HTTP instead.
 * @Last Modified by: caomengxuan666
 * @Last Modified time: 2025-08-05 17:01:52
 */
// src/transport/sse_transport.h
#pragma once

#include "business/plugin_manager.h"
#include "business/tool_registry.h"
#include "session.h"
#include <mutex>
#include <string>
#include <unordered_map>


namespace mcp::transport {

    class [[maybe_unused]] SSETransport {
    public:
        using MessageCallback = std::function<void(const std::string &, std::shared_ptr<Session>, const std::string &)>;

        explicit SSETransport(std::shared_ptr<business::ToolRegistry> registry);
        bool start(uint16_t port = 6666, const std::string &address = "0.0.0.0");
        void stop();
        void handle_request(const std::string &json_message, std::shared_ptr<Session> session, const std::string &session_id);

    private:
        std::shared_ptr<business::ToolRegistry> registry_;
        std::shared_ptr<business::PluginManager> plugin_manager_;

        std::unordered_map<std::string, std::shared_ptr<Session>> active_sse_sessions_;
        void send_error_event(std::shared_ptr<Session> session, const std::string &message);
        std::mutex sessions_mutex_;

        bool running_ = false;
    };

}// namespace mcp::transport