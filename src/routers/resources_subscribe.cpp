#include "resources_subscribe.hpp"
#include "core/logger.h"
#include "protocol/json_rpc.h"
#include <nlohmann/json.hpp>

namespace mcp::routers {

    protocol::Response handle_resources_subscribe(
            const protocol::Request &req,
            std::shared_ptr<business::ToolRegistry> /*registry*/,
            std::shared_ptr<transport::Session> /*session*/,
            const std::string &session_id) {
        
        MCP_DEBUG("Handling resources/subscribe request for session: {}", session_id);
        
        protocol::Response resp;
        resp.id = req.id;
        
        try {
            // check the uri
            if (!req.params.contains("uri")) {
                resp.result = nlohmann::json::parse(protocol::make_error(
                    protocol::error_code::INVALID_PARAMS,
                    "Missing 'uri' parameter",
                    req.id));
                return resp;
            }
            
            std::string uri = req.params["uri"];
            
            // todo set a subscription here instead of returning an empty success response
            nlohmann::json result;
            resp.result = result;
        } catch (const std::exception& e) {
            MCP_ERROR("Error handling resources/subscribe request: {}", e.what());
            resp.result = nlohmann::json::parse(protocol::make_error(
                protocol::error_code::INTERNAL_ERROR,
                "Failed to subscribe to resource: " + std::string(e.what()),
                req.id));
        }
        
        return resp;
    }

    protocol::Response handle_resources_unsubscribe(
            const protocol::Request &req,
            std::shared_ptr<business::ToolRegistry> /*registry*/,
            std::shared_ptr<transport::Session> /*session*/,
            const std::string &session_id) {
        
        MCP_DEBUG("Handling resources/unsubscribe request for session: {}", session_id);
        
        protocol::Response resp;
        resp.id = req.id;
        
        try {
            // check if the request has an id
            if (!req.params.contains("uri")) {
                resp.result = nlohmann::json::parse(protocol::make_error(
                    protocol::error_code::INVALID_PARAMS,
                    "Missing 'uri' parameter",
                    req.id));
                return resp;
            }
            
            std::string uri = req.params["uri"];
            
            // todo unsubscribe here instead of returning an empty success response
            
            nlohmann::json result;
            resp.result = result;
        } catch (const std::exception& e) {
            MCP_ERROR("Error handling resources/unsubscribe request: {}", e.what());
            resp.result = nlohmann::json::parse(protocol::make_error(
                protocol::error_code::INTERNAL_ERROR,
                "Failed to unsubscribe from resource: " + std::string(e.what()),
                req.id));
        }
        
        return resp;
    }

}// namespace mcp::routers