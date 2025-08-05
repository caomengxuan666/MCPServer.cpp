#include "../../sdk/mcp_plugin.h"
#include "core/mcpserver_api.h"
#include <atomic>
#include <chrono>
#include <ctime>
#include <nlohmann/json.hpp>
#include <thread>


struct NumberGenerator {
    std::atomic<int> current_num{1};
    std::atomic<bool> running{true};
    std::chrono::steady_clock::time_point last_send_time;
};


static int number_stream_next(StreamGenerator generator, const char **result_json) {
    if (!generator) {
        *result_json = R"({"error": "Invalid generator pointer"})";
        return 1;
    }

    auto *gen = static_cast<NumberGenerator *>(generator);

    if (!gen->running || gen->current_num > 1024) {
        *result_json = nullptr;// return nullptr to indicate end of stream
        return 1;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - gen->last_send_time).count();
    auto wait_ms = 100 - elapsed;
    if (wait_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
    }

    nlohmann::json numbers_json;
    numbers_json["jsonrpc"] = "2.0";
    numbers_json["method"] = "number_batch";
    numbers_json["params"]["batch"] = nlohmann::json::array();
    numbers_json["params"]["timestamp"] = std::time(nullptr);

    int count = 0;
    while (count < 10 && gen->current_num <= 1024) {
        numbers_json["params"]["batch"].push_back(gen->current_num++);
        count++;
    }

    gen->last_send_time = std::chrono::steady_clock::now();

    static thread_local std::string buffer;
    buffer = numbers_json.dump();
    *result_json = buffer.c_str();

    return 0;
}

static void number_stream_free(StreamGenerator generator) {
    auto *gen = static_cast<NumberGenerator *>(generator);
    if (gen) {
        gen->running = false;
        delete gen;
    }
}

extern "C" MCP_API StreamGeneratorNext get_stream_next() {
    return number_stream_next;
}

extern "C" MCP_API StreamGeneratorFree get_stream_free() {
    return number_stream_free;
}

extern "C" MCP_API const char *call_tool(const char *name, const char *args_json) {
    try {
        std::string tool_name = name;
        if (tool_name == "example_stream") {
            NumberGenerator *gen = new NumberGenerator();
            gen->last_send_time = std::chrono::steady_clock::now();
            return reinterpret_cast<const char *>(gen);
        } else {
            return _strdup(R"({"error": "Unknown tool: example_stream"})");
        }
    } catch (const std::exception &e) {
        return _strdup((R"({"error": ")" + std::string(e.what()) + R"("})").c_str());
    }
}

extern "C" MCP_API void free_result(const char *result) {
    if (result) {
        std::free(const_cast<char *>(result));
    }
}

static std::vector<ToolInfo> g_tools = {
        {"example_stream",
         "Stream numbers from 1 to 1024, exactly 10 numbers per second",
         R"({
            "type": "object",
            "properties": {},
            "required": []
        })",
         true}};

extern "C" MCP_API ToolInfo *get_tools(int *count) {
    *count = static_cast<int>(g_tools.size());
    return g_tools.data();
}
