#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

int main(int argc, char *argv[]) {
    // parse the argv
    // argv[0] is the plugin name
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <plugin_name>" << std::endl;
        return 1;
    }

    // get the current dir
    auto current_dir = fs::current_path();

    // create <plugin_name> dir
    fs::path plugins_dir = current_dir / argv[1];
    if (!fs::exists(plugins_dir)) {
        fs::create_directory(plugins_dir);
    }

    // touch file <plugin_name>.cpp
    fs::path plugin_file = plugins_dir / (std::string(argv[1]) + ".cpp");
    if (!fs::exists(plugin_file)) {
        std::ofstream ofs(plugin_file);
        ofs << "// Plugin: " << argv[1] << "\n";
        ofs << "// This is a template for your plugin implementation.\n";
        ofs << "#include \"core/mcpserver_api.h\"\n";
        ofs << "#include \"mcp_plugin.h\"\n";
        ofs << "#include \"tool_info_parser.h\"\n";
        ofs << "#include <cstdlib>\n";
        ofs << "#include <nlohmann/json.hpp>\n";
        ofs << "#include <string>\n\n";
        ofs << "static std::vector<ToolInfo> g_tools;\n\n";
        ofs << "// Generator structure for streaming tools\n";
        ofs << "struct " << argv[1] << "Generator {\n";
        ofs << "    // Add your generator fields here\n";
        ofs << "    bool running = true;\n";
        ofs << "    std::string error;\n";
        ofs << "};\n\n";
        ofs << "// Generator next function for streaming tools\n";
        ofs << "static int " << argv[1] << "_next(void* generator, const char** result_json, MCPError *error) {\n";
        ofs << "    // Add your streaming logic here\n";
        ofs << "    // Return 0 to continue streaming, 1 to stop\n";
        ofs << "    if (!generator) {\n";
        ofs << "        *result_json = R\"({\"error\": \"Invalid generator pointer\"})\";\n";
        ofs << "        if (error) {\n";
        ofs << "            error->code = 1;\n";
        ofs << "            error->message = \"Invalid generator pointer\";\n";
        ofs << "        }\n";
        ofs << "        return 1;\n";
        ofs << "    }\n\n";
        ofs << "    auto* gen = static_cast<" << argv[1] << "Generator*>(generator);\n";
        ofs << "    \n";
        ofs << "    // Check if there's an error\n";
        ofs << "    if (!gen->error.empty()) {\n";
        ofs << "        *result_json = gen->error.c_str();\n";
        ofs << "        if (error) {\n";
        ofs << "            error->code = 2;\n";
        ofs << "            error->message = gen->error.c_str();\n";
        ofs << "        }\n";
        ofs << "        return 1;\n";
        ofs << "    }\n\n";
        ofs << "    // Check if streaming should stop\n";
        ofs << "    if (!gen->running) {\n";
        ofs << "        *result_json = nullptr;\n";
        ofs << "        return 1;\n";
        ofs << "    }\n\n";
        ofs << "    // TODO: Implement your streaming logic here\n";
        ofs << "    // Example streaming response:\n";
        ofs << "    static thread_local std::string buffer;\n";
        ofs << "    buffer = nlohmann::json({{\"jsonrpc\", \"2.0\"},\n";
        ofs << "                             {\"method\", \"text\"},\n";
        ofs << "                             {\"params\", {{\"text\", \"Example streamed content\"}}}})\n";
        ofs << "                     .dump();\n\n";
        ofs << "    *result_json = buffer.c_str();\n";
        ofs << "    if (error) {\n";
        ofs << "        error->code = 0; // No error\n";
        ofs << "        error->message = nullptr;\n";
        ofs << "    }\n";
        ofs << "    return 0; // Continue streaming\n";
        ofs << "}\n\n";
        ofs << "// Generator free function for streaming tools\n";
        ofs << "static void " << argv[1] << "_free(void* generator) {\n";
        ofs << "    auto* gen = static_cast<" << argv[1] << "Generator*>(generator);\n";
        ofs << "    if (gen) {\n";
        ofs << "        gen->running = false;\n";
        ofs << "        delete gen;\n";
        ofs << "    }\n";
        ofs << "}\n\n";
        ofs << "extern \"C\" MCP_API ToolInfo *get_tools(int *count) {\n";
        ofs << "    try {\n";
        ofs << "        // Load tool information if not already loaded\n";
        ofs << "        if (g_tools.empty()) {\n";
        ofs << "            g_tools = ToolInfoParser::loadFromFile(\"" << argv[1] << "_tools.json\");\n";
        ofs << "        }\n\n";
        ofs << "        *count = static_cast<int>(g_tools.size());\n";
        ofs << "        return g_tools.data();\n";
        ofs << "    } catch (const std::exception &e) {\n";
        ofs << "        *count = 0;\n";
        ofs << "        return nullptr;\n";
        ofs << "    }\n";
        ofs << "}\n\n";
        ofs << "extern \"C\" MCP_API const char *call_tool(const char *name, const char *args_json, MCPError *error) {\n";
        ofs << "    try {\n";
        ofs << "        auto args = nlohmann::json::parse(args_json);\n";
        ofs << "        std::string tool_name = name;\n\n";
        ofs << "        // TODO: Implement your tool logic here\n";
        ofs << "        if (tool_name == \"" << argv[1] << "\") {\n";
        ofs << "            // Example implementation - replace with your actual logic\n";
        ofs << "            std::string result = nlohmann::json{{\"result\", \"Hello from " << argv[1] << "\"}}.dump();\n";
        ofs << "            return strdup(result.c_str());\n";
        ofs << "        }\n\n";
        ofs << "        // For streaming tools, return the generator\n";
        ofs << "        // Example for a streaming tool:\n";
        ofs << "        // if (tool_name == \"stream_" << argv[1] << "\") {\n";
        ofs << "        //     auto* gen = new " << argv[1] << "Generator();\n";
        ofs << "        //     // Initialize your generator here\n";
        ofs << "        //     return reinterpret_cast<const char*>(gen);\n";
        ofs << "        // }\n\n";
        ofs << "        if (error) {\n";
        ofs << "            error->code = 3;\n";
        ofs << "            error->message = (\"Unknown tool: \" + tool_name).c_str();\n";
        ofs << "        }\n";
        ofs << "        return strdup((nlohmann::json{{\"error\", \"Unknown tool: \" + tool_name}}.dump()).c_str());\n";
        ofs << "    } catch (const std::exception &e) {\n";
        ofs << "        if (error) {\n";
        ofs << "            error->code = 4;\n";
        ofs << "            error->message = e.what();\n";
        ofs << "        }\n";
        ofs << "        return strdup((nlohmann::json{{\"error\", e.what()}}.dump()).c_str());\n";
        ofs << "    }\n";
        ofs << "}\n\n";
        ofs << "extern \"C\" MCP_API void free_result(const char *result) {\n";
        ofs << "    if (result) {\n";
        ofs << "        std::free(const_cast<char *>(result));\n";
        ofs << "    }\n";
        ofs << "}\n\n";
        ofs << "// For streaming tools, implement these functions\n";
        ofs << "extern \"C\" MCP_API StreamGeneratorNext get_stream_next() {\n";
        ofs << "    return reinterpret_cast<StreamGeneratorNext>(" << argv[1] << "_next);\n";
        ofs << "}\n\n";
        ofs << "extern \"C\" MCP_API StreamGeneratorFree get_stream_free() {\n";
        ofs << "    return reinterpret_cast<StreamGeneratorFree>(" << argv[1] << "_free);\n";
        ofs << "}\n";
    }

    // create a cmakelists.txt file in the plugins dir
    fs::path cmakelists_file = plugins_dir / "CMakeLists.txt";
    if (!fs::exists(cmakelists_file)) {
        std::ofstream ofs(cmakelists_file);
        ofs << "configure_plugin(" << argv[1] << " " << argv[1] << ".cpp)\n";
    }

    // create tools.json file
    fs::path tools_json_file = plugins_dir / "tools.json";
    if (!fs::exists(tools_json_file)) {
        std::ofstream ofs(tools_json_file);
        ofs << "{\n";
        ofs << "  \"tools\": [\n";
        ofs << "    {\n";
        ofs << "      \"name\": \"" << argv[1] << "\",\n";
        ofs << "      \"description\": \"Description of " << argv[1] << "\",\n";
        ofs << "      \"parameters\": {\n";
        ofs << "        \"type\": \"object\",\n";
        ofs << "        \"properties\": {\n";
        ofs << "          \"param1\": {\n";
        ofs << "            \"type\": \"string\",\n";
        ofs << "            \"description\": \"An example parameter\"\n";
        ofs << "          }\n";
        ofs << "        },\n";
        ofs << "        \"required\": []\n";
        ofs << "      }\n";
        ofs << "    },\n";
        ofs << "    {\n";
        ofs << "      \"name\": \"stream_" << argv[1] << "\",\n";
        ofs << "      \"description\": \"Stream data from " << argv[1] << "\",\n";
        ofs << "      \"parameters\": {\n";
        ofs << "        \"type\": \"object\",\n";
        ofs << "        \"properties\": {\n";
        ofs << "          \"param1\": {\n";
        ofs << "            \"type\": \"string\",\n";
        ofs << "            \"description\": \"An example parameter\"\n";
        ofs << "          }\n";
        ofs << "        },\n";
        ofs << "        \"required\": []\n";
        ofs << "      },\n";
        ofs << "      \"is_streaming\": true\n";
        ofs << "    }\n";
        ofs << "  ]\n";
        ofs << "}\n";
    }

    std::cout << "Plugin template created at: " << plugin_file << std::endl;
    std::cout << "Tools JSON template created at: " << tools_json_file << std::endl;
    std::cout << "You can now implement your plugin logic in " << plugin_file << std::endl;
    std::cout << "Modify the tools.json file to define your tool's interface" << std::endl;

    return 0;
}