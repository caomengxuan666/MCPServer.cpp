// src/business/request_handler.cpp
#include "request_handler.h"
#include "core/logger.h"
#include "protocol/json_rpc.h"


namespace mcp::business {

    RequestHandler::RequestHandler(std::shared_ptr<ToolRegistry> registry)
        : registry_(std::move(registry)) {
        transport_.open([this](const std::string &msg) {
            handle_request(msg);
        });
    }

    void RequestHandler::handle_request(const std::string &msg) {
        MCP_DEBUG("Raw message: {}", msg);
        auto req = mcp::protocol::parse_request(msg);
        if (!req) return;

        if (req->method == "initialize") {
            mcp::protocol::Response resp;
            resp.id = req->id.value_or(nullptr);
            resp.result = nlohmann::json{{"capabilities", nlohmann::json::object()}};
            transport_.write(mcp::protocol::make_response(resp));
        } else if (req->method == "listTools") {
            auto tools = registry_->get_all_tools();
            mcp::protocol::Response resp;
            resp.id = req->id.value_or(nullptr);
            nlohmann::json tools_json = nlohmann::json::array();
            for (const auto &tool: tools) {
                tools_json.push_back({{"name", tool.name},
                                      {"description", tool.description},
                                      {"parameters", tool.parameters}});
            }
            resp.result = tools_json;
            transport_.write(mcp::protocol::make_response(resp));
        } else if (req->method == "callTool") {
            auto params = req->params;
            std::string tool_name = params.value("name", "");
            auto args = params.value("arguments", nlohmann::json{});

            auto result = registry_->execute(tool_name, args);
            if (result) {
                mcp::protocol::Response resp;
                resp.id = req->id.value_or(nullptr);
                resp.result = *result;
                transport_.write(mcp::protocol::make_response(resp));
            } else {
                auto err = mcp::protocol::make_error(
                        mcp::protocol::error_code::METHOD_NOT_FOUND,
                        "Tool not found: " + tool_name,
                        req->id.value_or(nullptr));
                transport_.write(err);
            }
        } else if (req->method == "exit") {
            MCP_INFO("Exit command received");
            std::exit(0);
        } else {
            auto err = mcp::protocol::make_error(
                    mcp::protocol::error_code::METHOD_NOT_FOUND,
                    "Method not supported: " + req->method,
                    req->id.value_or(nullptr));
            transport_.write(err);
        }
    }

}// namespace mcp::business