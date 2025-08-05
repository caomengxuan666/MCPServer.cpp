// src/protocol/tool.h
#pragma once

#include "nlohmann/json.hpp"
#include <string>

namespace mcp::protocol {

    struct Tool {
        std::string name;
        std::string description;
        nlohmann::json parameters;// JSON Schema
        bool is_streaming = false;
    };

    // this function creates a simple echo tool
    inline Tool make_echo_tool() {
        return Tool{
                .name = "echo",
                .description = "Returns the input text as-is",
                .parameters = nlohmann::json{
                        {"type", "object"},
                        {"properties", {{"text", {{"type", "string"}, {"description", "The text to echo"}}}}},
                        {"required", nlohmann::json::array({"text"})}}};
    }

}// namespace mcp::protocol