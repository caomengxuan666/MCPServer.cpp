// plugins/sdk/tool_info_parser.h
#pragma once

#include "mcp_plugin.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

class ToolInfoParser {
public:
    static std::vector<ToolInfo> loadFromFile(const std::string &json_file_path);

    static std::vector<ToolInfo> parseFromString(const std::string &json_string);

    static std::vector<ToolInfo> parseFromJson(const nlohmann::json &json_data);

    static void freeToolInfoVector(std::vector<ToolInfo> &tools);

private:
    static std::vector<ToolInfo> parseTools(const nlohmann::json &tools_json);
    static ToolInfo parseTool(const nlohmann::json &tool_json);
    static char *stringToCharPtr(const std::string &str);
};