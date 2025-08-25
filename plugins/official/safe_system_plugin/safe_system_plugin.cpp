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
    try {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");

        // Return raw business data without RPC wrapper
        return mcp::protocol::generate_result(nlohmann::json{{"current_time", ss.str()}});
    } catch (const std::exception &e) {
        // Return custom error code and message
        return mcp::protocol::generate_error(mcp::protocol::error_code::TOOL_NOT_FOUND,
                                             "Failed to get current time: " + std::string(e.what()));
    }
}

/**
 * @brief Retrieves basic system information (OS and architecture)
 * @return JSON string containing system info
 */
static std::string get_system_info() {
    try {
#ifdef _WIN32
        std::string os = "Windows";
#else
        std::string os = "Unix-like";
#endif
        std::string arch = sizeof(void *) == 8 ? "x86_64" : "x86";

        // Return raw business data without RPC wrapper
        return mcp::protocol::generate_result(nlohmann::json{{"os", os}, {"arch", arch}});
    } catch (const std::exception &e) {
        // Return custom error code and message
        return mcp::protocol::generate_error(mcp::protocol::error_code::TOOL_NOT_FOUND,
                                             "Failed to get system info: " + std::string(e.what()));
    }
}

/**
 * @brief Lists files in specified directory using system commands
 * @param path Directory path to list files from
 * @return JSON string containing file list or error message
 */
static std::string list_files(const std::string &path) {
    try {
        // Prevent path traversal attacks
        if (path.find("..") != std::string::npos) {
            // Return custom error code and message
            return mcp::protocol::generate_error(mcp::protocol::error_code::TOOL_NOT_FOUND,
                                                 "Path traversal is not allowed");
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
            // Return custom error code and message
            return mcp::protocol::generate_error(mcp::protocol::error_code::TOOL_NOT_FOUND,
                                                 "Failed to list files: Pipe creation failed");
        }

        // Read command output
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }

        // Return raw business data
        return mcp::protocol::generate_result(nlohmann::json{{"files", result}});
    } catch (const std::exception &e) {
        // Return custom error code and message
        return mcp::protocol::generate_error(mcp::protocol::error_code::TOOL_NOT_FOUND,
                                             "Failed to list files: " + std::string(e.what()));
    }
}

/**
 * @brief Pings a host to check network connectivity
 * @param host Hostname or IP address to ping
 * @return JSON string containing ping results
 */
static std::string ping_host(const std::string &host) {
    try {
        // Validate host name format
        if (!std::all_of(host.begin(), host.end(), [](char c) {
                return std::isalnum(c) || c == '.' || c == '-';
            })) {
            // Return custom error code and message
            return mcp::protocol::generate_error(mcp::protocol::error_code::TOOL_NOT_FOUND,
                                                 "Invalid host name format");
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
            // Return custom error code and message
            return mcp::protocol::generate_error(mcp::protocol::error_code::TOOL_NOT_FOUND,
                                                 "Ping command failed: Pipe creation failed");
        }

        // Read ping output
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }

        // Determine success status from output
        bool success = result.find("TTL=") != std::string::npos ||
                       result.find("time=") != std::string::npos;

        // Return raw business data
        return mcp::protocol::generate_result(nlohmann::json{{"output", result}, {"success", success}});
    } catch (const std::exception &e) {
        // Return custom error code and message
        return mcp::protocol::generate_error(mcp::protocol::error_code::TOOL_NOT_FOUND,
                                             "Ping command failed: " + std::string(e.what()));
    }
}

/**
 * @brief Checks internet connectivity by pinging 8.8.8.8
 * @return JSON string containing connectivity results
 */
static std::string check_connectivity() {
    try {
        return ping_host("8.8.8.8");// Use Google DNS as test target
    } catch (const std::exception &e) {
        // Return custom error code and message
        return mcp::protocol::generate_error(mcp::protocol::error_code::TOOL_NOT_FOUND,
                                             "Failed to check connectivity: " + std::string(e.what()));
    }
}

/**
 * @brief Retrieves public IP address using external API
 * @return JSON string containing public IP or error
 */
static std::string get_public_ip() {
    try {
        std::array<char, 128> buffer;
        std::string ip;

        // In China, you can try some domestic - accessible IP query services,
        // such as ipip.net. Here we still use curl to get the IP,
        // and you can also use other domestic - oriented IP query APIs in the future.
#ifdef _WIN32
        // For Windows, the following command can be used to try to get the public IP
        // You can change the URL to other valid ones, such as curl ipinfo.io
        std::string cmd = "curl -s myip.ipip.net";
#else
        // For non - Windows systems (like Linux, macOS), the command format is similar
        std::string cmd = "curl -s myip.ipip.net";
#endif

        // Use popen to execute the command and get the result
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (!pipe) {
            // Return custom error code and message
            return mcp::protocol::generate_error(mcp::protocol::error_code::TOOL_NOT_FOUND, "curl command not found");
        }

        if (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            ip = buffer.data();
            // Remove newline characters
            ip.erase(std::remove(ip.begin(), ip.end(), '\n'), ip.end());
        }

        if (!ip.empty() && ip.find('.') != std::string::npos) {
            return mcp::protocol::generate_result(nlohmann::json{{"public_ip", ip}});
        } else {
// If the IP is not obtained correctly, return an error message
// Try to use foreign - accessible IP query services
#ifdef _WIN32
            std::string foreign_cmd = "curl -s https://api.ipify.org";
#else
            std::string foreign_cmd = "curl -s https://api.ipify.org";
#endif
            std::unique_ptr<FILE, decltype(&pclose)> foreign_pipe(popen(foreign_cmd.c_str(), "r"), pclose);
            if (!foreign_pipe) {
                return mcp::protocol::generate_error(mcp::protocol::error_code::TOOL_NOT_FOUND,
                                                     "curl command not found when trying foreign service");
            }
            std::array<char, 128> foreign_buffer;
            if (fgets(foreign_buffer.data(), foreign_buffer.size(), foreign_pipe.get()) != nullptr) {
                ip = foreign_buffer.data();
                ip.erase(std::remove(ip.begin(), ip.end(), '\n'), ip.end());
            }
            if (!ip.empty() && ip.find('.') != std::string::npos) {
                return mcp::protocol::generate_result(nlohmann::json{{"public_ip", ip}});
            } else {
                // Return custom error code and message if still failed
                return mcp::protocol::generate_error(mcp::protocol::error_code::TOOL_NOT_FOUND,
                                                     "Failed to get public IP after trying both domestic and foreign services");
            }
        }
    } catch (const std::exception &e) {
        // If an exception occurs, return an error message with the exception details
        return mcp::protocol::generate_error(mcp::protocol::error_code::TOOL_NOT_FOUND,
                                             "Failed to get public IP: " + std::string(e.what()));
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
static int log_file_next(StreamGenerator generator, const char **result_json, MCPError *error) {
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
extern "C" MCP_API const char *call_tool(const char *name, const char *args_json, MCPError *error) {
    try {
        auto args = nlohmann::json::parse(args_json);
        std::string tool_name = name;

        if (tool_name == "get_current_time") {
            std::string result = get_current_time();
            return strdup(result.c_str());
        } else if (tool_name == "get_system_info") {
            std::string result = get_system_info();
            return strdup(result.c_str());
        } else if (tool_name == "list_files") {
            std::string path = args.value("path", "");
            if (path.empty()) {
                error->code = mcp::protocol::error_code::INVALID_TOOL_INPUT;
                error->message = "Missing 'path' parameter";
                return nullptr;
            }
            std::string result = list_files(path);
            return strdup(result.c_str());
        } else if (tool_name == "ping_host") {
            std::string host = args.value("host", "");
            if (host.empty()) {
                error->code = mcp::protocol::error_code::INVALID_TOOL_INPUT;
                error->message = "Missing 'host' parameter";
                return nullptr;
            }
            std::string result = ping_host(host);
            return strdup(result.c_str());
        } else if (tool_name == "check_connectivity") {
            std::string result = check_connectivity();
            return strdup(result.c_str());
        } else if (tool_name == "get_public_ip") {
            std::string result = get_public_ip();
            auto result_json = nlohmann::json::parse(result);

            // Check if there is an error field, if so, return the error directly
            if (result_json.contains("error")) {
                error->code = result_json["error"]["code"];
                error->message = strdup(result_json["error"]["message"].get<std::string>().c_str());
                return nullptr;// Return nullptr to indicate an error
            }

            return strdup(result.c_str());// Return result on success
        } else if (tool_name == "stream_log_file") {
            std::string path = args.value("path", "");
            if (path.empty()) {
                error->code = mcp::protocol::error_code::INVALID_TOOL_INPUT;
                error->message = "Missing 'path' parameter";
                return nullptr;
            }
            auto *gen = new LogFileGenerator();
            gen->file.open(path);
            if (!gen->file.is_open()) {
                gen->error = nlohmann::json{
                        {"jsonrpc", "2.0"},
                        {"method", "error"},
                        {"params", nlohmann::json{{"message", "Failed to open log file: " + path}}}}
                                     .dump();
            }
            return reinterpret_cast<const char *>(gen);
        } else {
            error->code = mcp::protocol::error_code::TOOL_NOT_FOUND;
            error->message = "Unknown tool";
            return nullptr;
        }
    } catch (const std::exception &e) {
        error->code = mcp::protocol::error_code::INTERNAL_ERROR;
        error->message = e.what();
        return nullptr;
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