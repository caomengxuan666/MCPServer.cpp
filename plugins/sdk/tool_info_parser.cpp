// plugins/sdk/tool_info_parser.cpp
#include "tool_info_parser.h"
#include "core/executable_path.h"
#include "core/logger.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

std::vector<ToolInfo> ToolInfoParser::loadFromFile(const std::string &json_file_path) {
    std::string full_path = (std::filesystem::path(mcp::core::getExecutableDirectory()) / json_file_path).string();
    MCP_TRACE("Loading tool info from: {}", full_path);

    std::ifstream file(full_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open tool info file: " + full_path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    return parseFromString(buffer.str());
}

std::vector<ToolInfo> ToolInfoParser::parseFromString(const std::string &json_string) {
    auto json_data = nlohmann::json::parse(json_string);
    return parseFromJson(json_data);
}

std::vector<ToolInfo> ToolInfoParser::parseFromJson(const nlohmann::json &json_data) {
    if (!json_data.contains("tools") || !json_data["tools"].is_array()) {
        throw std::runtime_error("Invalid JSON format: missing or invalid 'tools' array");
    }

    return parseTools(json_data["tools"]);
}

std::vector<ToolInfo> ToolInfoParser::parseTools(const nlohmann::json &tools_json) {
    std::vector<ToolInfo> tools;
    for (const auto &tool_json: tools_json) {
        tools.push_back(parseTool(tool_json));
    }
    return tools;
}

ToolInfo ToolInfoParser::parseTool(const nlohmann::json &tool_json) {
    ToolInfo tool{};

    if (!tool_json.contains("name") || !tool_json.contains("description") || !tool_json.contains("parameters")) {
        throw std::runtime_error("Invalid tool format: missing required fields (name, description, parameters)");
    }

    tool.name = stringToCharPtr(tool_json["name"]);
    tool.description = stringToCharPtr(tool_json["description"]);

    // parameters can be a string or a JSON object
    if (tool_json["parameters"].is_string()) {
        tool.parameters = stringToCharPtr(tool_json["parameters"]);
    } else {
        tool.parameters = stringToCharPtr(tool_json["parameters"].dump());
    }

    tool.is_streaming = tool_json.value("is_streaming", false);

    return tool;
}

char *ToolInfoParser::stringToCharPtr(const std::string &str) {
    char *result = new char[str.length() + 1];
    std::strcpy(result, str.c_str());
    return result;
}

void ToolInfoParser::freeToolInfoVector(std::vector<ToolInfo> &tools) {
    for (auto &tool: tools) {
        delete[] const_cast<char *>(tool.name);
        delete[] const_cast<char *>(tool.description);
        delete[] const_cast<char *>(tool.parameters);
    }
    tools.clear();
}