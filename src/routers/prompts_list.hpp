#pragma once

#include "protocol/json_rpc.h"
#include "Prompts/prompt.h"
#include "tool_registry.h"
#include "transport/session.h"
#include <memory>

namespace mcp::routers {

    /**
     * @brief Handle prompts/list request
     * @param req RPC request
     * @return Response with list of available prompts
     */
    inline protocol::Response handle_prompts_list(
            const protocol::Request& req,
            std::shared_ptr<business::ToolRegistry> /*registry*/,
            std::shared_ptr<transport::Session> /*session*/,
            const std::string& /*session_id*/) {
        
        protocol::Response resp;
        resp.id = req.id.value_or(nullptr);

        // Create example prompts - in a real implementation, these would come from the server's prompt management system
        std::vector<mcp::prompts::Prompt> prompts;

        // Example analyze-code prompt
        mcp::prompts::Prompt analyzeCodePrompt;
        analyzeCodePrompt.name = "analyze-code";
        analyzeCodePrompt.description = "Analyze a code snippet";
        
        mcp::prompts::PromptArgument languageArg;
        languageArg.name = "language";
        languageArg.description = "programming language";
        languageArg.required = true;
        analyzeCodePrompt.arguments.push_back(languageArg);
        
        prompts.push_back(analyzeCodePrompt);

        // Example git-commit prompt
        mcp::prompts::Prompt gitCommitPrompt;
        gitCommitPrompt.name = "git-commit";
        gitCommitPrompt.description = "generate Git commit message";
        
        mcp::prompts::PromptArgument changesArg;
        changesArg.name = "changes";
        changesArg.description = "Git diff or changes description";
        changesArg.required = true;
        gitCommitPrompt.arguments.push_back(changesArg);
        
        prompts.push_back(gitCommitPrompt);

        // Build response
        nlohmann::json result;
        nlohmann::json promptsJson = nlohmann::json::array();
        for (const auto& prompt : prompts) {
            promptsJson.push_back(mcp::prompts::to_json(prompt));
        }
        result["prompts"] = promptsJson;

        resp.result = result;
        return resp;
    }

} // namespace mcp::routers