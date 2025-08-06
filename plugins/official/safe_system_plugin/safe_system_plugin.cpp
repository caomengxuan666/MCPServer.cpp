#include "../../sdk/mcp_plugin.h"
#include "core/mcpserver_api.h"
#include "tool_info_parser.h"

// platform-specific includes
#ifdef _WIN32
#include <io.h>// _popen, _pclose
#define popen _popen
#define pclose _pclose
#else
#include <stdio.h>// popen, pclose
#endif

#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>


// glbal tools vector
static std::vector<ToolInfo> g_tools;

std::string get_current_time() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return nlohmann::json{{"current_time", ss.str()}}.dump();
}

std::string get_system_info() {
#ifdef _WIN32
    std::string os = "Windows";
#else
    std::string os = "Unix-like";
#endif
    std::string arch = sizeof(void *) == 8 ? "x86_64" : "x86";
    return nlohmann::json{{"os", os}, {"arch", arch}}.dump();
}

std::string list_files(const std::string &path) {
    if (path.find("..") != std::string::npos) {
        return R"({"error": "Path traversal is not allowed"})";
    }

    std::array<char, 128> buffer;
    std::string result;

#ifdef _WIN32
    std::string cmd = "dir \"" + path + "\" /b";
#else
    std::string cmd = "ls \"" + path + "\"";
#endif

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return R"({"error": "Failed to list files"})";

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return nlohmann::json{{"files", result}}.dump();
}

std::string ping_host(const std::string &host) {
    if (!std::all_of(host.begin(), host.end(), [](char c) {
            return std::isalnum(c) || c == '.' || c == '-';
        })) {
        return R"({"error": "Invalid host name"})";
    }

    std::array<char, 128> buffer;
    std::string result;

#ifdef _WIN32
    std::string cmd = "ping -n 1 -w 1000 " + host;
#else
    std::string cmd = "ping -c 1 -W 1 " + host;
#endif

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return R"({"error": "Ping command failed"})";

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    bool success = result.find("TTL=") != std::string::npos || result.find("time=") != std::string::npos;
    return nlohmann::json{{"output", result}, {"success", success}}.dump();
}

std::string check_connectivity() {
    return ping_host("8.8.8.8");
}

std::string get_public_ip() {
    std::array<char, 128> buffer;
    std::string ip;

#ifdef _WIN32
    std::string cmd = "curl -s https://api.ipify.org  ";
#else
    std::string cmd = "curl -s https://api.ipify.org  ";
#endif

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return R"({"error": "curl command not found"})";

    if (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        ip = buffer.data();
        ip.erase(std::remove(ip.begin(), ip.end(), '\n'), ip.end());
    }

    if (!ip.empty() && ip.find('.') != std::string::npos) {
        return nlohmann::json{{"public_ip", ip}}.dump();
    } else {
        return R"({"error": "Failed to get public IP"})";
    }
}

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
struct LogFileGenerator {
    std::ifstream file;
    bool running = true;
    std::string error;
};

static int log_file_next(StreamGenerator generator, const char **result_json) {
    if (!generator) {
        *result_json = R"({"error": "Invalid generator pointer"})";
        return 1;
    }

    auto *gen = static_cast<LogFileGenerator *>(generator);

    if (!gen->error.empty()) {
        *result_json = gen->error.c_str();
        return 1;
    }

    if (!gen->running || gen->file.eof()) {
        *result_json = nullptr;
        return 1;
    }

    std::string line;
    if (std::getline(gen->file, line)) {
        static thread_local std::string buffer;
        buffer = nlohmann::json({{"jsonrpc", "2.0"},
                                 {"method", "log_line"},
                                 {"params", {{"content", line}}}})
                         .dump();

        *result_json = buffer.c_str();
        return 0;
    }
    return 1;
}

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

extern "C" MCP_API StreamGeneratorNext get_stream_next() {
    return log_file_next;
}

extern "C" MCP_API StreamGeneratorFree get_stream_free() {
    return log_file_free;
}


extern "C" MCP_API const char *call_tool(const char *name, const char *args_json) {
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
                return strdup(R"({"error": "Missing 'path' parameter"})");
            }
            std::string result = list_files(path);
            return strdup(result.c_str());
        } else if (tool_name == "ping_host") {
            std::string host = args.value("host", "");
            if (host.empty()) {
                return strdup(R"({"error": "Missing 'host' parameter"})");
            }
            std::string result = ping_host(host);
            return strdup(result.c_str());
        } else if (tool_name == "check_connectivity") {
            std::string result = check_connectivity();
            return strdup(result.c_str());
        } else if (tool_name == "get_public_ip") {
            std::string result = get_public_ip();
            return strdup(result.c_str());
        } else if (tool_name == "stream_log_file") {
            std::string path = args.value("path", "");
            if (path.empty()) {
                return strdup(R"({"error": "Missing 'path' parameter"})");
            }

            if (!std::filesystem::exists(path)) {
                return strdup(R"({"error": "File not found"})");
            }

            LogFileGenerator *gen = new LogFileGenerator();
            gen->file.open(path);

            if (!gen->file.is_open()) {
                gen->error = nlohmann::json{{"error", "Failed to open file"}}.dump();
            }
            return reinterpret_cast<const char *>(gen);
        } else {
            return strdup(R"({"error": "Unknown tool"})");
        }
    } catch (const std::exception &e) {
        return strdup((R"({"error": ")" + std::string(e.what()) + R"("})").c_str());
    }
}


extern "C" MCP_API void free_result(const char *result) {
    if (result) {
        std::free(const_cast<char *>(result));
    }
}
