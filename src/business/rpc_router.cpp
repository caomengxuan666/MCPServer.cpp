#include "rpc_router.h"
#include "core/logger.h"
#include "protocol/json_rpc.h"
#include "transport/mcp_cache.h"
#include "transport/session.h"
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <mutex>


#include <string>


namespace mcp::business {

    /**
     * @brief Initialize cache singleton once with reconnection parameters
     *        Max 1000 sessions, 500 entries per session, 24-hour expiration
     */
    static void init_cache_once() {
        static std::once_flag flag;
        std::call_once(flag, []() {
            auto *cache = mcp::cache::McpCache::GetInstance();
            cache->Init(1000, 500, std::chrono::hours(24));

            if (cache->IsInitialized()) {
                MCP_INFO("McpCache initialized successfully for reconnection support");
            } else {
                MCP_ERROR("Failed to initialize McpCache - reconnection functionality will be unavailable");
            }
        });
    }

    RpcRouter::RpcRouter() {
        init_cache_once();// Initialize cache on router creation
    }

    /**
     * @brief Register RPC method handler
     * @param method RPC method name
     * @param handler Handler function for the method
     */
    void RpcRouter::register_handler(const std::string &method, RpcHandler handler) {
        handlers_[method] = std::move(handler);
    }

    /**
     * @brief Find registered handler for a method
     * @param method RPC method name
     * @return Optional handler if found
     */
    std::optional<RpcHandler> RpcRouter::find_handler(const std::string &method) const {
        auto it = handlers_.find(method);
        return (it != handlers_.end()) ? std::optional<RpcHandler>(it->second) : std::nullopt;
    }

    /**
     * @brief Route RPC request to appropriate handler
     * @param req RPC request object
     * @param registry Tool registry instance
     * @param session Transport session
     * @param session_id Unique session identifier
     * @return RPC response
     */
    protocol::Response RpcRouter::route_request(
            const protocol::Request &req,
            std::shared_ptr<ToolRegistry> registry,
            std::shared_ptr<transport::Session> session,
            const std::string &session_id) const {

        std::string method = req.method;

        // Map HTTP paths to RPC methods for debugging convenience
        if (method == "/tools/list") {
            method = "tools/list";
        } else if (method == "/tools/call") {
            method = "tools/call";
        }

        auto handler = find_handler(method);
        if (handler.has_value()) {
            return handler.value()(req, registry, session, session_id);
        } else {
            protocol::Response resp;
            resp.id = req.id.value_or(nullptr);
            resp.result = nlohmann::json::parse(protocol::make_error(
                    protocol::error_code::METHOD_NOT_FOUND,
                    "Method not supported: " + method,
                    req.id.value_or(nullptr)));
            return resp;
        }
    }


}// namespace mcp::business