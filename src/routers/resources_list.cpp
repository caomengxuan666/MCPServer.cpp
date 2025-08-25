#include "resources_list.hpp"
#include "Resources/resource.h"
#include "core/logger.h"
#include "protocol/json_rpc.h"
#include <nlohmann/json.hpp>

namespace mcp::routers {

    protocol::Response handle_resources_list(
            const protocol::Request &req,
            std::shared_ptr<business::ToolRegistry> /*registry*/,
            std::shared_ptr<transport::Session> /*session*/,
            const std::string &session_id) {

        MCP_DEBUG("Handling resources/list request for session: {}", session_id);

        // make a resp object
        protocol::Response resp;
        resp.id = req.id;

        try {
            // get the resource manager
            mcp::resources::ResourceManager resource_manager;

            // get the resources and templates
            auto resources = resource_manager.get_resources();
            auto templates = resource_manager.get_resource_templates();

            nlohmann::json result;

            // add the resources list
            nlohmann::json resource_list = nlohmann::json::array();
            for (const auto &resource: resources) {
                nlohmann::json res;
                res["uri"] = resource.uri;
                res["name"] = resource.name;
                if (!resource.description.empty()) {
                    res["description"] = resource.description;
                }
                if (!resource.mimeType.empty()) {
                    res["mimeType"] = resource.mimeType;
                }
                resource_list.push_back(res);
            }
            result["resources"] = resource_list;

            // add templates list
            nlohmann::json template_list = nlohmann::json::array();
            for (const auto &resourceTemplate: templates) {
                nlohmann::json tmpl;
                tmpl["uriTemplate"] = resourceTemplate.uriTemplate;
                tmpl["name"] = resourceTemplate.name;
                if (!resourceTemplate.description.empty()) {
                    tmpl["description"] = resourceTemplate.description;
                }
                if (!resourceTemplate.mimeType.empty()) {
                    tmpl["mimeType"] = resourceTemplate.mimeType;
                }
                template_list.push_back(tmpl);
            }
            result["resourceTemplates"] = template_list;

            resp.result = result;
        } catch (const std::exception &e) {
            MCP_ERROR("Error handling resources/list request: {}", e.what());
            resp.result = nlohmann::json::parse(protocol::make_error(
                    protocol::error_code::INTERNAL_ERROR,
                    "Failed to list resources: " + std::string(e.what()),
                    req.id));
        }

        return resp;
    }

}// namespace mcp::routers