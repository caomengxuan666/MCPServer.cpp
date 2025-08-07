#include "core/mcpserver_api.h"
#include "mcp_plugin.h"
#include <atomic>
#include <chrono>
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>

/**
 * Generator state for sequential number streaming
 * 
 * Tracks:
 * - Current number in sequence
 * - Streaming control flag
 * - Timing for rate control
 * - Event sequence counter
 */
struct NumberGenerator {
    std::atomic<int> current_num{1};// Current number being generated
    std::atomic<bool> running{true};// Controls stream termination
    std::chrono::steady_clock::time_point last_send_time;

    explicit NumberGenerator(int req_id) {}
};


/**
 * @brief Generates the next batch of numbers in the stream
 * 
 * @param generator Opaque pointer to generator state
 * @param result_json Output parameter for JSON result
 * @return int 0=success, 1=end-of-stream, -1=error
 * Generates the next batch of numbers with full tracking
 * 
 * Implements:
 * 1. Rate limiting (10 numbers/second)
 * 2. Sequence tracking
 * 3. Breakpoint resumption support
 */
static int number_stream_next(StreamGenerator generator, const char **result_json) {
    if (!generator) {
        *result_json = R"({"error":"Invalid generator"})";
        return -1;
    }

    auto *gen = static_cast<NumberGenerator *>(generator);

    // End condition check
    if (!gen->running || gen->current_num > 1024) {
        *result_json = nullptr;
        return 1;
    }

    // Rate control
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - gen->last_send_time)
                           .count();
    if (elapsed < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100 - elapsed));
    }

    // Generate batch
    std::vector<int> batch;
    while (batch.size() < 10 && gen->current_num <= 1024) {
        batch.push_back(gen->current_num++);
    }

    // Build response
    nlohmann::json response = {
            {"jsonrpc", "2.0"},
            {"result", {{"batch", batch}, {"remaining", 1024 - gen->current_num + 1}}}};

    static thread_local std::string buffer;
    buffer = response.dump();
    *result_json = buffer.c_str();

    gen->last_send_time = now;
    return 0;
}
/**
 * @brief Cleans up generator resources
 * 
 * @param generator Opaque pointer to generator state
 */
static void number_stream_free(StreamGenerator generator) {
    if (generator) {
        auto *gen = static_cast<NumberGenerator *>(generator);
        gen->running = false;
        delete gen;
    }
}

/**
 * Handles tool initialization with resumption support
 * 
 * Expected params format:
 * {
 *   "id": 123,                // Required: request ID
 *   "last_event_id": 5        // Optional: for resuming
 * }
 */
extern "C" MCP_API const char *call_tool(const char *name, const char *args_json) {
    try {
        // Parse arguments (compatible with both tools/call and direct JSON-RPC)
        nlohmann::json args = args_json ? nlohmann::json::parse(args_json) : nlohmann::json::object();

        // Handle start position
        int last_event_id = 0;
        if (args.contains("last_event_id")) {
            last_event_id = args["last_event_id"].get<int>();
        } else if (args.contains("arguments") && args["arguments"].contains("last_event_id")) {
            last_event_id = args["arguments"]["last_event_id"].get<int>();
        }

        // Create generator
        auto *gen = new NumberGenerator(0);
        if (last_event_id > 0) {
            gen->current_num = 1 + (last_event_id * 10);
        }

        return reinterpret_cast<const char *>(gen);

    } catch (const std::exception &e) {
        return strdup((R"({"error":")" + std::string(e.what()) + R"("})").c_str());
    }
}

/**
 * @brief Returns plugin metadata and capabilities
 * @note You had better use tools.json to generate g_tools array
 * @param count Output parameter for tool count
 * @return ToolInfo* Array of tool descriptors
 */
static std::vector<ToolInfo> g_tools = {
        {
                "example_stream",                                             // Tool name
                "Generates number sequences from 1-1024 at 10 numbers/second",// Description
                R"({
            "type": "object",
            "properties": {
                "start": {"type": "number", "minimum": 1, "maximum": 1024}
            },
            "required": []
        })",                                                                  // JSON Schema
                true                                                          // is_streaming
        }};

extern "C" MCP_API ToolInfo *get_tools(int *count) {
    *count = static_cast<int>(g_tools.size());
    return g_tools.data();
}

/**
 * @brief Releases memory allocated for strings
 * 
 * @param result String pointer to free
 */
extern "C" MCP_API void free_result(const char *result) {
    if (result) {
        std::free(const_cast<char *>(result));
    }
}

// Export streaming functions
extern "C" MCP_API StreamGeneratorNext get_stream_next() {
    return number_stream_next;
}

extern "C" MCP_API StreamGeneratorFree get_stream_free() {
    return number_stream_free;
}