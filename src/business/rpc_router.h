#pragma once
#include "protocol/json_rpc.h"
#include "tool_registry.h"
#include "transport/session.h"
#include <functional>
#include <memory>
#include <unordered_map>


namespace mcp::business {
    using RpcHandler = std::function<protocol::Response(
            const protocol::Request &,
            std::shared_ptr<ToolRegistry>,
            std::shared_ptr<transport::Session>,
            const std::string &// SessionID
            )>;

    protocol::Response handle_initialize(
            const protocol::Request &req,
            std::shared_ptr<ToolRegistry> registry,
            std::shared_ptr<transport::Session> session,
            const std::string &session_id);

    protocol::Response handle_tools_list(
            const protocol::Request &req,
            std::shared_ptr<ToolRegistry> registry,
            std::shared_ptr<transport::Session> session,
            const std::string &session_id);

    protocol::Response handle_tools_call(
            const protocol::Request &req,
            std::shared_ptr<ToolRegistry> registry,
            std::shared_ptr<transport::Session> session,
            const std::string &session_id);

    protocol::Response handle_exit(
            const protocol::Request &req,
            std::shared_ptr<ToolRegistry> registry,
            std::shared_ptr<transport::Session> session,
            const std::string &session_id);

    class RpcRouter {
    public:
        RpcRouter();

        void register_handler(const std::string &method, RpcHandler handler);

        std::optional<RpcHandler> find_handler(const std::string &method) const;

        protocol::Response route_request(
                const protocol::Request &req,
                std::shared_ptr<ToolRegistry> registry,
                std::shared_ptr<transport::Session> session,
                const std::string &session_id) const;

    private:
        std::unordered_map<std::string, RpcHandler> handlers_;
    };

}// namespace mcp::business
