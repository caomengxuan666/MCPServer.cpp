#pragma once

#include "Prompts/prompt.h"
#include "protocol/json_rpc.h"
#include "tool_registry.h"
#include "transport/session.h"
#include <memory>

namespace mcp::routers {

    /**
     * @brief Handle prompts/get request
     * @param req RPC request with prompt name and arguments
     * @return Response with prompt content
     */
    inline protocol::Response handle_prompts_get(
            const protocol::Request &req,
            std::shared_ptr<business::ToolRegistry> /*registry*/,
            std::shared_ptr<transport::Session> /*session*/,
            const std::string & /*session_id*/) {

        protocol::Response resp;
        resp.id = req.id.value_or(nullptr);

        // Extract name from request params
        std::string name = req.params.value("name", "");
        if (name.empty()) {
            resp.error = protocol::Error{
                    protocol::error_code::INVALID_PARAMS,
                    "Missing 'name' parameter"};
            return resp;
        }

        // Extract arguments from request params
        nlohmann::json arguments = req.params.value("arguments", nlohmann::json::object());

        // Create example prompt content - in a real implementation, this would come from the server's prompt management system
        mcp::prompts::PromptContent content;
        content.description = "analyze the code to improve";

        // Create example messages
        mcp::prompts::PromptMessage message1;
        message1.role = "user";
        nlohmann::json content1;
        content1["type"] = "text";
        content1["text"] = "analaze the given code:\n\n```python\ndef calculate_sum(numbers):\n    total = 0\n    for num in numbers:\n        total = total + num\n    return total\n\nresult = calculate_sum([1, 2, 3, 4, 5])\nprint(result)\n```";
        message1.content = content1;

        content.messages.push_back(message1);

        // Build response
        resp.result = mcp::prompts::to_json(content);
        return resp;
    }

}// namespace mcp::routers