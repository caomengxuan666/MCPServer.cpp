// src/Prompts/prompt.cpp
#include "prompt.h"
#include <algorithm>
#include <sstream>


namespace mcp::prompts {

    void PromptManager::register_prompt(const Prompt &prompt) {
        // Check if a prompt with the same name already exists
        auto it = std::find_if(prompts_.begin(), prompts_.end(),
                               [&prompt](const Prompt &p) { return p.name == prompt.name; });

        if (it != prompts_.end()) {
            // If exists, replace it
            *it = prompt;
        } else {
            // Otherwise add new prompt
            prompts_.push_back(prompt);
        }
    }

    std::vector<Prompt> PromptManager::get_prompts() const {
        return prompts_;
    }

    std::optional<PromptContent> PromptManager::get_prompt_content(const std::string &name, const nlohmann::json &arguments) const {
        // Find matching prompt
        auto it = std::find_if(prompts_.begin(), prompts_.end(),
                               [&name](const Prompt &p) { return p.name == name; });

        if (it == prompts_.end()) {
            return std::nullopt;
        }

        PromptContent content;
        content.description = it->description;

        // Create sample message content
        // In actual implementation, concrete messages should be generated here based on prompt definition and provided arguments
        PromptMessage message;
        message.role = "user";

        // Build information text containing prompt name and arguments
        std::ostringstream text_stream;
        text_stream << "Prompt: " << name << "\n";
        if (it->description.has_value()) {
            text_stream << "Description: " << it->description.value() << "\n";
        }
        text_stream << "Arguments:\n";

        for (const auto &arg: it->arguments) {
            text_stream << "  - " << arg.name << ": ";
            if (arg.description.has_value()) {
                text_stream << arg.description.value();
            }
            if (arguments.contains(arg.name)) {
                text_stream << " = " << arguments[arg.name].dump();
            }
            text_stream << "\n";
        }

        nlohmann::json content_json;
        content_json["type"] = "text";
        content_json["text"] = text_stream.str();
        message.content = content_json;

        content.messages.push_back(message);

        return content;
    }

    void PromptManager::notify_list_changed() {
        // Notify clients that prompt list has changed
        //todo notifications/prompts/list_changed
        // and then the client would refetch the prompt list
    }

}// namespace mcp::prompts