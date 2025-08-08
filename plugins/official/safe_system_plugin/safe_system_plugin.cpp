#include "core/mcpserver_api.h"
#include "mcp_plugin.h"
#include "protocol/json_rpc.h"
#include "tool_info_parser.h"

// Platform-specific includes for process handling
#ifdef _WIN32
#include <io.h>// For _popen, _pclose on Windows
#define popen _popen
#define pclose _pclose
#else
#include <stdio.h>// For popen, pclose on Unix-like systems
#endif

#include <algorithm>// For std::all_of
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>


// Global storage for tool metadata
static std::vector<ToolInfo> g_tools;

/**
 * @brief Retrieves current system time as JSON string
 * @return JSON string containing current time
 */
static std::string get_current_time() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");

    // Return raw business data without RPC wrapper
    return nlohmann::json{{"current_time", ss.str()}}.dump();
}

/**
 * @brief Retrieves basic system information (OS and architecture)
 * @return JSON string containing system info
 */
static std::string get_system_info() {
#ifdef _WIN32
    std::string os = "Windows";
#else
    std::string os = "Unix-like";
#endif
    std::string arch = sizeof(void *) == 8 ? "x86_64" : "x86";

    // Return raw business data without RPC wrapper
    return nlohmann::json{{"os", os}, {"arch", arch}}.dump();
}

/**
 * @brief Lists files in specified directory using system commands
 * @param path Directory path to list files from
 * @return JSON string containing file list or error message
 */
static std::string list_files(const std::string &path) {
    // Prevent path traversal attacks
    if (path.find("..") != std::string::npos) {
        return nlohmann::json{{"error", "Path traversal is not allowed"}}.dump();
    }

    std::array<char, 128> buffer;
    std::string result;

    // Platform-specific directory listing command
#ifdef _WIN32
    std::string cmd = "dir \"" + path + "\" /b";// Windows: bare format listing
#else
    std::string cmd = "ls \"" + path + "\"";// Unix: simple listing
#endif

    // Execute command and capture output
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        return nlohmann::json{{"error", "Failed to list files: Pipe creation failed"}}.dump();
    }

    // Read command output
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    // Return raw business data
    return nlohmann::json{{"files", result}}.dump();
}

/**
 * @brief Pings a host to check network connectivity
 * @param host Hostname or IP address to ping
 * @return JSON string containing ping results
 */
static std::string ping_host(const std::string &host) {
    // Validate host name format
    if (!std::all_of(host.begin(), host.end(), [](char c) {
            return std::isalnum(c) || c == '.' || c == '-';
        })) {
        return nlohmann::json{{"error", "Invalid host name format"}}.dump();
    }

    std::array<char, 128> buffer;
    std::string result;

    // Platform-specific ping command
#ifdef _WIN32
    std::string cmd = "ping -n 1 -w 1000 " + host;// Windows: 1 packet, 1s timeout
#else
    std::string cmd = "ping -c 1 -W 1 " + host;// Unix: 1 packet, 1s timeout
#endif

    // Execute ping command
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        return nlohmann::json{{"error", "Ping command failed: Pipe creation failed"}}.dump();
    }

    // Read ping output
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    // Determine success status from output
    bool success = result.find("TTL=") != std::string::npos ||
                   result.find("time=") != std::string::npos;

    // Return raw business data
    return nlohmann::json{{"output", result}, {"success", success}}.dump();
}

/**
 * @brief Checks internet connectivity by pinging 8.8.8.8
 * @return JSON string containing connectivity results
 */
static std::string check_connectivity() {
    return ping_host("8.8.8.8");// Use Google DNS as test target
}

/**
 * @brief Retrieves public IP address using external API
 * @return JSON string containing public IP or error
 */
static std::string get_public_ip() {
    std::array<char, 128> buffer;
    std::string ip;

    // Command to retrieve public IP via API
    std::string cmd = "curl -s https://api.ipify.org";

    // Execute curl command
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        return nlohmann::json{{"error", "Failed to execute curl command"}}.dump();
    }

    // Read IP address from response
    if (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        ip = buffer.data();
        ip.erase(std::remove(ip.begin(), ip.end(), '\n'), ip.end());// Remove newline
    }

    // Validate and return result
    if (!ip.empty() && ip.find('.') != std::string::npos) {
        return nlohmann::json{{"public_ip", ip}}.dump();
    } else {
        return nlohmann::json{{"error", "Failed to retrieve public IP"}}.dump();
    }
}

/**
 * @brief Plugin entry: Returns registered tools metadata
 * @param count Output parameter for number of tools
 * @return Array of ToolInfo structures
 */
extern "C" MCP_API ToolInfo *get_tools(int *count) {
    try {
        if (g_tools.empty()) {
            g_tools = ToolInfoParser::loadFromFile("safe_system_plugin_tools.json");
        }
        *count = static_cast<int>(g_tools.size());
        return g_tools.data();
    } catch (const std::exception &e) {
        *count = 0;
        return nullptr;
    }
}

/**
 * @brief Structure to manage log file streaming state
 */
struct LogFileGenerator {
    std::ifstream file; // Log file stream
    bool running = true;// Streaming state flag
    std::string error;  // Error message storage
};

/**
 * @brief Retrieves next line from streaming log file
 * @param generator Pointer to LogFileGenerator instance
 * @param result_json Output parameter for JSON result
 * @return 0 on success, 1 on completion/error
 */
static int log_file_next(StreamGenerator generator, const char **result_json) {
    if (!generator) {
        static thread_local std::string buffer = nlohmann::json{{"error", "Invalid generator pointer"}}.dump();
        *result_json = buffer.c_str();
        return 1;
    }

    auto *gen = static_cast<LogFileGenerator *>(generator);

    // Return stored error if exists
    if (!gen->error.empty()) {
        static thread_local std::string buffer = gen->error;
        *result_json = buffer.c_str();
        return 1;
    }

    // Check for streaming completion
    if (!gen->running || gen->file.eof()) {
        *result_json = nullptr;
        return 1;
    }

    // Read next line from log file
    std::string line;
    if (std::getline(gen->file, line)) {
        static thread_local std::string buffer = nlohmann::json{
                {"jsonrpc", "2.0"},
                {"method", "log_line"},
                {"params", nlohmann::json{{"content", line}}}}
                                                         .dump();
        *result_json = buffer.c_str();
        return 0;
    }
    return 1;
}

/**
 * @brief Cleans up log file streaming resources
 * @param generator Pointer to LogFileGenerator instance
 */
static void log_file_free(StreamGenerator generator) {
    auto *gen = static_cast<LogFileGenerator *>(generator);
    if (gen) {
        gen->running = false;
        if (gen->file.is_open()) {
            gen->file.close();
        }
        delete gen;
    }
}

/**
 * @brief Plugin entry: Returns stream next function
 * @return Pointer to log_file_next function
 */
extern "C" MCP_API StreamGeneratorNext get_stream_next() {
    return log_file_next;
}

/**
 * @brief Plugin entry: Returns stream cleanup function
 * @return Pointer to log_file_free function
 */
extern "C" MCP_API StreamGeneratorFree get_stream_free() {
    return log_file_free;
}

/**
 * @brief Plugin entry: Executes requested tool
 * @param name Name of tool to execute
 * @param args_json JSON string containing tool arguments
 * @return JSON string with tool results or error
 */
extern "C" MCP_API const char *call_tool(const char *name, const char *args_json) {
    try {
        auto args = nlohmann::json::parse(args_json);
        std::string tool_name = name;

        // Route to appropriate tool implementation
        if (tool_name == "get_current_time") {
            return strdup(get_current_time().c_str());
        } else if (tool_name == "get_system_info") {
            return strdup(get_system_info().c_str());
        } else if (tool_name == "list_files") {
            std::string path = args.value("path", "");
            if (path.empty()) {
                return strdup(nlohmann::json{{"error", "Missing 'path' parameter"}}.dump().c_str());
            }
            return strdup(list_files(path).c_str());
        } else if (tool_name == "ping_host") {
            std::string host = args.value("host", "");
            if (host.empty()) {
                return strdup(nlohmann::json{{"error", "Missing 'host' parameter"}}.dump().c_str());
            }
            return strdup(ping_host(host).c_str());
        } else if (tool_name == "check_connectivity") {
            return strdup(check_connectivity().c_str());
        } else if (tool_name == "get_public_ip") {
            return strdup(get_public_ip().c_str());
        } else if (tool_name == "stream_log_file") {
            std::string path = args.value("path", "");
            if (path.empty()) {
                return strdup(nlohmann::json{{"error", "Missing 'path' parameter"}}.dump().c_str());
            }

            if (!std::filesystem::exists(path)) {
                return strdup(nlohmann::json{{"error", "File not found"}}.dump().c_str());
            }

            // Initialize log file generator
            LogFileGenerator *gen = new LogFileGenerator();
            gen->file.open(path);

            if (!gen->file.is_open()) {
                gen->error = nlohmann::json{{"error", "Failed to open file"}}.dump();
            }
            return reinterpret_cast<const char *>(gen);
        } else {
            return strdup(nlohmann::json{{"error", "Unknown tool: " + tool_name}}.dump().c_str());
        }
    } catch (const std::exception &e) {
        // Handle JSON parsing errors
        return strdup(nlohmann::json{{"error", "Execution failed: " + std::string(e.what())}}.dump().c_str());
    }
}

/**
 * @brief Frees memory allocated for tool results
 * @param result Pointer to result string to free
 */
extern "C" MCP_API void free_result(const char *result) {
    if (result) {
        std::free(const_cast<char *>(result));
    }
}