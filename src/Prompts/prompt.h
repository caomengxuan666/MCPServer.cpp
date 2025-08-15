// src/Prompts/prompt.h
#pragma once

#include "nlohmann/json.hpp"
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mcp::prompts {

    // Prompt struct, represents a prompt template
    struct Prompt {
        std::string name;                            // Unique identifier for the prompt
        std::optional<std::string> description;      // Human-readable description
        std::vector<struct PromptArgument> arguments;// Optional list of arguments
    };

    // Prompt argument struct
    struct PromptArgument {
        std::string name;                      // Argument identifier
        std::optional<std::string> description;// Argument description
        bool required = false;                 // Whether the argument is required
    };

    // Prompt message struct
    struct PromptMessage {
        std::string role;      // Message role (user, assistant, system)
        nlohmann::json content;// Message content
    };

    // Prompt content struct
    struct PromptContent {
        std::optional<std::string> description;// Prompt description
        std::vector<PromptMessage> messages;   // List of messages
    };


    // Convert Prompt to JSON
    inline nlohmann::json to_json(const Prompt &prompt) {
        nlohmann::json j;
        j["name"] = prompt.name;

        if (!prompt.description.has_value()) {
            j["description"] = prompt.description;
        }

        if (!prompt.arguments.empty()) {
            nlohmann::json args = nlohmann::json::array();
            for (const auto &arg: prompt.arguments) {
                nlohmann::json arg_json;
                arg_json["name"] = arg.name;

                if (arg.description.has_value()) {
                    arg_json["description"] = arg.description.value();
                }

                if (arg.required) {
                    arg_json["required"] = arg.required;
                }

                args.push_back(arg_json);
            }
            j["arguments"] = args;
        }

        return j;
    }

    // Convert PromptArgument to JSON
    inline nlohmann::json to_json(const PromptArgument &argument) {
        nlohmann::json j;
        j["name"] = argument.name;

        if (argument.description.has_value()) {
            j["description"] = argument.description.value();
        }

        if (argument.required) {
            j["required"] = argument.required;
        }

        return j;
    }

    // Convert PromptMessage to JSON
    inline nlohmann::json to_json(const PromptMessage &message) {
        nlohmann::json j;
        j["role"] = message.role;
        j["content"] = message.content;
        return j;
    }

    // Convert PromptContent to JSON
    inline nlohmann::json to_json(const PromptContent &content) {
        nlohmann::json j;

        if (!content.description.has_value()) {
            j["description"] = content.description;
        }

        if (!content.messages.empty()) {
            nlohmann::json messages = nlohmann::json::array();
            for (const auto &msg: content.messages) {
                messages.push_back(to_json(msg));
            }
            j["messages"] = messages;
        }

        return j;
    }


    // Prompt update callback function signature
    using PromptUpdateCallback = std::function<void(const std::string &name)>;

    class PromptManager {
    public:
        PromptManager() = default;

        // Register static prompt
        void register_prompt(const Prompt &prompt);

        // Get all registered prompts
        std::vector<Prompt> get_prompts() const;

        // Get content of a specific prompt
        std::optional<PromptContent> get_prompt_content(const std::string &name, const nlohmann::json &arguments) const;

        // Notify prompt list change
        void notify_list_changed();

    private:
        std::vector<Prompt> prompts_;
        std::unordered_map<std::string, std::vector<PromptUpdateCallback>> subscriptions_;
    };

}// namespace mcp::prompts