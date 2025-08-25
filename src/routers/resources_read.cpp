#include "resources_read.hpp"
#include "Resources/resource.h"
#include "core/logger.h"
#include "protocol/json_rpc.h"
#include <nlohmann/json.hpp>

namespace mcp::routers {

    protocol::Response handle_resources_read(
            const protocol::Request &req,
            std::shared_ptr<business::ToolRegistry> /*registry*/,
            std::shared_ptr<transport::Session> /*session*/,
            const std::string &session_id) {

        MCP_DEBUG("Handling resources/read request for session: {}", session_id);

        protocol::Response resp;
        resp.id = req.id;

        try {
            // check if the request has an uri
            if (!req.params.contains("uri")) {
                resp.result = nlohmann::json::parse(protocol::make_error(
                        protocol::error_code::INVALID_PARAMS,
                        "Missing 'uri' parameter",
                        req.id));
                return resp;
            }

            std::string uri = req.params["uri"];

            mcp::resources::ResourceManager resource_manager;

            auto contents = resource_manager.read_resource(uri);

            nlohmann::json result;
            nlohmann::json content_list = nlohmann::json::array();

            for (const auto &content: contents) {
                nlohmann::json c;
                c["uri"] = content.uri;
                if (!content.mimeType.empty()) {
                    c["mimeType"] = content.mimeType;
                }
                if (!content.text.empty()) {
                    c["text"] = content.text;
                }
                if (!content.blob.empty()) {
                    c["blob"] = content.blob;
                }
                content_list.push_back(c);
            }

            result["contents"] = content_list;
            resp.result = result;
        } catch (const std::exception &e) {
            MCP_ERROR("Error handling resources/read request: {}", e.what());
            resp.result = nlohmann::json::parse(protocol::make_error(
                    protocol::error_code::INTERNAL_ERROR,
                    "Failed to read resource: " + std::string(e.what()),
                    req.id));
        }

        return resp;
    }

}// namespace mcp::routers