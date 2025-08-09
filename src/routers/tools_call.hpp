#include "core/logger.h"
#include "plugin_manager.h"
#include "protocol/json_rpc.h"
#include "request_handler.h"
#include "transport/mcp_cache.h"

namespace mcp::routers {

    // Structure to hold stream generator with its cleanup function
    struct StreamResource {
        StreamGenerator generator;
        StreamGeneratorFree free_func;

        // Default constructor
        StreamResource() : generator(nullptr), free_func(nullptr) {}

        // Constructor with parameters
        StreamResource(StreamGenerator gen, StreamGeneratorFree free_fn)
            : generator(gen), free_func(free_fn) {}
    };
    // Global generator map maintaining session_id -> generator for reconnection support
    static std::mutex generator_mtx_;
    static std::map<std::string, StreamResource> generator_map_;

    /**
     * @brief Clean up expired sessions that haven't reconnected within 5 minutes
     *        Prevents memory leaks from abandoned generators
     */
    static void cleanup_expired_sessions() {
        static std::chrono::system_clock::time_point last_cleanup = std::chrono::system_clock::now();
        auto now = std::chrono::system_clock::now();

        // Clean up every 5 minutes
        if (now - last_cleanup < std::chrono::minutes(5)) {
            return;
        }
        last_cleanup = now;

        std::lock_guard<std::mutex> lock(generator_mtx_);
        auto *cache = mcp::cache::McpCache::GetInstance();
        std::vector<std::string> expired_sessions;

        // Identify expired sessions
        for (const auto &[session_id, resource]: generator_map_) {
            auto state = cache->GetSessionState(session_id);
            if (!state || (now - state->last_update) > std::chrono::minutes(5)) {
                expired_sessions.push_back(session_id);
            }
        }

        // Clean up expired sessions
        for (const auto &session_id: expired_sessions) {
            // Get the resource before erasing it from the map
            auto it = generator_map_.find(session_id);
            if (it != generator_map_.end()) {
                // Call the stream_free function to release plugin resources
                if (it->second.free_func) {
                    it->second.free_func(it->second.generator);
                    MCP_INFO("Freed stream resources for expired session - session: {}", session_id);
                }

                // Remove from generator map
                generator_map_.erase(it);
            }

            // Clean up cache session
            cache->CleanupSession(session_id);
            MCP_INFO("Cleaned up expired session - session: {}", session_id);
        }
    }
    inline protocol::Response handle_tools_call(
            const protocol::Request &req,
            std::shared_ptr<business::ToolRegistry> registry,
            std::shared_ptr<transport::Session> session,
            const std::string & /*session_id*/) {
        protocol::Response resp;
        resp.id = req.id.value_or(nullptr);

        auto params = req.params;
        std::string tool_name = params.value("name", "");
        auto args = params.value("arguments", nlohmann::json{});

        // Validate tool existence
        auto tool_info = registry->get_tool_info(tool_name);
        if (!tool_info) {
            resp.result = nlohmann::json::parse(protocol::make_error(
                    protocol::error_code::METHOD_NOT_FOUND,
                    "Tool not found: " + tool_name,
                    req.id.value_or(nullptr)));
            return resp;
        }

        // Validate plugin manager
        auto plugin_manager = registry->get_plugin_manager();
        if (!plugin_manager) {
            resp.result = nlohmann::json::parse(protocol::make_error(
                    protocol::error_code::INTERNAL_ERROR,
                    "PluginManager not found",
                    req.id.value_or(nullptr)));
            return resp;
        }

        // Check client SSE support
        std::string accept_header = session->get_accept_header();
        bool client_supports_sse = (accept_header.find("text/event-stream") != std::string::npos);

        MCP_DEBUG("Tool name: {}", tool_name);
        MCP_DEBUG("Tool is_streaming: {}", tool_info->is_streaming);
        MCP_DEBUG("Client supports SSE: {}", client_supports_sse);

        // Run expired session cleanup
        cleanup_expired_sessions();

        // Streaming handling with reconnection support
        if (tool_info->is_streaming && client_supports_sse) {
            MCP_INFO("Upgrading to SSE stream for tool: {}", tool_name);
            std::string current_session_id = session->get_session_id();
            auto *cache = mcp::cache::McpCache::GetInstance();

            // 1. Reconnection detection and state recovery
            int last_event_id = 0;
            bool is_reconnect = false;
            const auto &headers = session->get_headers();

            // Get last received event ID from request headers
            if (headers.count("Last-Event-ID")) {
                try {
                    last_event_id = std::stoi(headers.at("Last-Event-ID"));
                    auto session_state = cache->GetSessionState(current_session_id);

                    if (session_state.has_value()) {
                        is_reconnect = true;
                        MCP_INFO("Reconnection detected - session: {}, last_event_id: {}",
                                 current_session_id, last_event_id);
                    }
                } catch (...) {
                    MCP_WARN("Invalid Last-Event-ID format, treating as new connection");
                }
            }

            // 2. Send SSE response headers
            std::string sse_header = "HTTP/1.1 200 OK\r\n";
            sse_header += "Content-Type: text/event-stream\r\n";
            sse_header += "Cache-Control: no-cache, no-transform\r\n";
            sse_header += "Connection: keep-alive\r\n";
            sse_header += "Mcp-Session-Id: " + current_session_id + "\r\n";
            sse_header += "\r\n\r\n";

            asio::co_spawn(session->get_socket().get_executor(), [session, sse_header]() -> asio::awaitable<void> {
                    co_await session->write(sse_header);
                    co_return; }, asio::detached);

            // 3. Send session initialization event
            {
                std::string init_event = nlohmann::json{
                        {"jsonrpc", "2.0"},
                        {"id", req.id.value_or(1)},
                        {"session_id", current_session_id}}
                                                 .dump();
                std::string sse_init =
                        "event: session_init\n"
                        "id: " +
                        std::to_string(static_cast<long long>(req.id.value_or(0))) + "\n"
                                                                                     "data: " +
                        init_event + "\n\n";

                asio::co_spawn(session->get_socket().get_executor(), [session, sse_init]() -> asio::awaitable<void> {
                        co_await session->write(sse_init);
                        co_return; }, asio::detached);
            }

            // 4. Get or create stream generator (reuse for reconnections)
            StreamGenerator generator = nullptr;
            StreamGeneratorFree stream_free_func = nullptr;
            MCPError tool_error = {0, nullptr, nullptr, nullptr};

            if (is_reconnect) {
                std::lock_guard<std::mutex> lock(generator_mtx_);
                auto it = generator_map_.find(current_session_id);

                if (it != generator_map_.end()) {
                    generator = it->second.generator;
                    MCP_INFO("Reusing existing generator - session: {}", current_session_id);
                } else {
                    // Attempt to recreate generator for expired sessions
                    MCP_WARN("Generator expired, recreating for reconnection - session: {}", current_session_id);
                    generator = plugin_manager->start_streaming_tool(tool_name, args);

                    if (!generator) {
                        std::string error_msg = "Session expired, please restart request";
                        std::string sse_error = "event: error\n data: " +
                                                nlohmann::json{{"message", error_msg}}.dump() + "\n\n";

                        asio::co_spawn(session->get_socket().get_executor(), [session, sse_error]() -> asio::awaitable<void> {
                                co_await session->write(sse_error);
                                session->close();
                                co_return; }, asio::detached);

                        resp.id = nullptr;
                        resp.result = nlohmann::json::value_t::discarded;
                        return resp;
                    }

                    // Get the stream functions for the new generator
                    auto stream_functions = plugin_manager->get_stream_functions(generator);
                    stream_free_func = stream_functions.free;

                    // Store the new generator with its free function
                    generator_map_[current_session_id] = StreamResource(generator, stream_free_func);
                }
            } else {
                // Create new generator for fresh connection
                generator = plugin_manager->start_streaming_tool(tool_name, args);
                if (!generator) {
                    std::string error_msg = tool_error.message
                                                    ? tool_error.message
                                                    : "Failed to start streaming tool: " + tool_name;
                    int error_code = tool_error.code ? tool_error.code : -32603;

                    nlohmann::json error_event = {
                            {"code", error_code},
                            {"message", error_msg}};


                    std::string sse_error = "event: error\n data: " +
                                            nlohmann::json{{"message", error_msg}}.dump() + "\n\n";
                    asio::co_spawn(session->get_socket().get_executor(), [session, sse_error]() -> asio::awaitable<void> {
                    co_await session->write(sse_error);
                    session->close();
                    co_return; }, asio::detached);

                    resp.id = nullptr;
                    resp.result = nlohmann::json::value_t::discarded;
                    return resp;
                }

                // Get the stream functions
                auto stream_functions = plugin_manager->get_stream_functions(generator);
                stream_free_func = stream_functions.free;

                // Save generator with its free function for potential reconnection
                std::lock_guard<std::mutex> lock(generator_mtx_);
                generator_map_[current_session_id] = StreamResource(generator, stream_free_func);

                // Initialize new session state
                mcp::cache::SessionState initial_state;
                initial_state.session_id = current_session_id;
                initial_state.tool_name = tool_name;
                initial_state.last_event_id = 0;
                initial_state.last_update = std::chrono::system_clock::now();
                cache->SaveSessionState(initial_state);
            }

            // 5. Get stream processing functions
            auto stream_functions = plugin_manager->get_stream_functions(generator);
            if (!stream_functions.next || !stream_functions.free || stream_functions.error.code != 0) {
                std::string error_msg = "Stream functions not found for tool: " + tool_name;
                if (stream_functions.error.message) {
                    error_msg = stream_functions.error.message;
                }

                // Clean up if generator was created
                if (stream_functions.free) stream_functions.free(generator);

                // Remove from generator map if exists
                {
                    std::lock_guard<std::mutex> lock(generator_mtx_);
                    auto it = generator_map_.find(session->get_session_id());
                    if (it != generator_map_.end()) {
                        generator_map_.erase(it);
                    }
                }

                // Clean up cache session
                cache->CleanupSession(session->get_session_id());

                std::string sse_error = "event: error\n data: " +
                                        nlohmann::json{{"message", error_msg}}.dump() + "\n\n";

                asio::co_spawn(session->get_socket().get_executor(), [session, sse_error]() -> asio::awaitable<void> {
                        co_await session->write(sse_error);
                        session->close();
                        co_return; }, asio::detached);

                resp.id = nullptr;
                resp.result = nlohmann::json::value_t::discarded;
                return resp;
            }

            StreamGeneratorNext stream_next = stream_functions.next;
            StreamGeneratorFree stream_free = stream_functions.free;

            // 6. Reconnection data resend logic (based on Event-ID)
            if (is_reconnect) {
                auto cached_data = cache->GetReconnectData(current_session_id, last_event_id);
                MCP_INFO("Reconnection resend plan - session: {}, items to resend: {}",
                         current_session_id, cached_data.size());

                // wait until all the cached data is sent
                // and then continue streaming
                std::shared_ptr<std::promise<void>> resend_promise = std::make_shared<std::promise<void>>();
                std::future<void> resend_future = resend_promise->get_future();

                asio::co_spawn(session->get_socket().get_executor(), [session, cached_data, last_event_id, current_session_id, resend_promise]() -> asio::awaitable<void> {
                        int event_id = last_event_id + 1;
                        auto* cache = mcp::cache::McpCache::GetInstance();

                        for (const auto& data : cached_data) {
                            if (session->is_closed()) {
                                MCP_INFO("Connection closed during resend - session: {}", current_session_id);
                                break;
                            }
                            
                            std::string json_str = data.dump();
                            std::string event_str = 
                                "event: message\n"
                                "id: " + std::to_string(event_id) + "\n"
                                "data: " + json_str + "\n\n";
                            
                            co_await session->write(event_str);
                            MCP_DEBUG("Resend completed - session: {}, event: {}",
                                     current_session_id, event_id);
                            
                            // Update session state with latest event ID
                            auto state_opt = cache->GetSessionState(current_session_id);
                            if (state_opt.has_value()) {
                                mcp::cache::SessionState updated_state = state_opt.value();
                                updated_state.last_event_id = event_id;
                                updated_state.last_update = std::chrono::system_clock::now();
                                cache->SaveSessionState(updated_state);
                            }
                            
                            event_id++;
                        }
                        
                        // notify data process coro to work
                        resend_promise->set_value();
                        co_return; }, asio::detached);

                // wait util the data is send
                resend_future.wait();
            }

            // 7. Start stream consumer (new data processing + caching)
            asio::co_spawn(session->get_socket().get_executor(), [session, generator, stream_next, stream_free, req, current_session_id, last_event_id, is_reconnect]() -> asio::awaitable<void> {
                    
                const char* result_json = nullptr;
                int status = 0;
                auto* cache = mcp::cache::McpCache::GetInstance();

                // Initialize event ID counter (continue from last on reconnection)

                int event_id = 1;
                if (is_reconnect) {
                    //get the session_id after reconnect
                    auto state_opt = cache->GetSessionState(current_session_id);
                    if (state_opt.has_value()) {
                        event_id = state_opt.value().last_event_id + 1;
                    } else {
                        event_id = last_event_id + 1;
                    }
                }

                try {
                    while (true) {
                        // Check connection status first
                        if (session->is_closed()) {
                            MCP_INFO("Connection closed, stopping stream - session: {}", current_session_id);
                            break;
                        }

                        // Get next stream data
                        MCPError error = {0, nullptr, nullptr, nullptr};
                        status = stream_next(generator, &result_json, &error);
                        
                        // Handle stream termination
                        if (status == 1) {
                            MCP_DEBUG("Stream completed normally - session: {}", current_session_id);
                            
                            // Send stream completion event
                            if (!session->is_closed()) {
                                std::string complete_event = 
                                    "event: complete\n"
                                    "id: " + std::to_string(event_id) + "\n"
                                    "data: " + nlohmann::json{{"message", "Stream completed"}}.dump() + "\n\n";
                                co_await session->write(complete_event);
                            }
                            break;
                        } 
                        // Handle stream errors
                        else if (status == -1) {
                            std::string error_json_str = result_json ? result_json : "";
                            nlohmann::json error_data;
                            
                            try {
                                // Try to parse error JSON
                                if (!error_json_str.empty()) {
                                    error_data = nlohmann::json::parse(error_json_str);
                                }
                            } catch (...) {
                                // Parsing failed, create default error
                                error_data = nlohmann::json{
                                    {"error", {
                                        {"code", protocol::error_code::INTERNAL_ERROR},
                                        {"message", error_json_str.empty() ? "Unknown stream error" : error_json_str}
                                    }}
                                };
                            }
                            
                            // Extract error code and message
                            int error_code = protocol::error_code::INTERNAL_ERROR;
                            std::string error_msg = "Unknown stream error";
                            
                            if (error_data.contains("error")) {
                                error_code = error_data["error"].value("code", error_code);
                                error_msg = error_data["error"].value("message", error_msg);
                            } else if (error.message) {
                                error_msg = error.message;
                            }
                            
                            MCP_ERROR("Stream error - session: {}: {} (code: {})", 
                                    current_session_id, error_msg, error_code);
                            
                            if (!session->is_closed()) {
                                // Send structured error event
                                nlohmann::json error_event = {
                                    {"code", error_code},
                                    {"message", error_msg}
                                };
                                
                                std::string sse_error = "event: error\n data: " + 
                                                        error_event.dump() + "\n\n";
                                co_await session->write(sse_error);
                            }
                            break;
                        } 
                        // Process valid data
                        else if (result_json && *result_json != '\0') {
                            const std::string json_copy(result_json);
                            if (!json_copy.empty() && json_copy[0] == '{') {
                                try {
                                    nlohmann::json data = nlohmann::json::parse(json_copy);
                                    std::string data_str = data.dump();

                                    // Send only if connection is alive
                                    if (!session->is_closed()) {
                                        std::string event_str = 
                                            "event: message\n"
                                            "id: " + std::to_string(event_id) + "\n"
                                            "data: " + data_str + "\n\n";
                                        
                                        co_await session->write(event_str);
                                        MCP_DEBUG("Data sent - session: {}, event: {}",
                                                 current_session_id, event_id);
                                    } else {
                                        MCP_DEBUG("Connection closed, data cached only - session: {}, event: {}",
                                                 current_session_id, event_id);
                                    }

                                    // Cache data with event ID and update state
                                    cache->CacheStreamData(current_session_id, event_id, data);
                                    
                                    mcp::cache::SessionState updated_state;
                                    if (auto state = cache->GetSessionState(current_session_id)) {
                                        updated_state = *state;
                                    } else {
                                        updated_state.session_id = current_session_id;
                                    }
                                    updated_state.last_event_id = event_id;
                                    updated_state.last_update = std::chrono::system_clock::now();
                                    cache->SaveSessionState(updated_state);

                                    event_id++;
                                } catch (const nlohmann::json::parse_error& e) {
                                    MCP_ERROR("JSON parse error: {}", e.what());
                                }
                            } else {
                                MCP_ERROR("Invalid data format: {}", json_copy);
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    MCP_ERROR("Stream consumer exception - session: {}: {}", current_session_id, e.what());
                    if (!session->is_closed()) {
                        std::string error_msg = "Stream error: " + std::string(e.what());
                        std::string sse_error = "event: error\n data: " + 
                            nlohmann::json{{"message", error_msg}}.dump() + "\n\n";
                        
                        asio::co_spawn(session->get_socket().get_executor(), 
                            [session, sse_error]() -> asio::awaitable<void> {
                                co_await session->write(sse_error);
                                co_return;
                            }, asio::detached);
                    }
                }

                // Cleanup only if session is expired (handled by cleanup_expired_sessions)
                // Do NOT remove generator from map here to allow reconnection
                if (!session->is_closed()) {
                    session->close();
                }
                co_return; }, asio::detached);

            // Mark response as SSE stream
            resp.id = nullptr;
            resp.result = nlohmann::json::value_t::discarded;
            return resp;
        }
        // Synchronous tool invocation handling
        else {
            try {
                auto result = registry->execute(tool_name, args);
                if (!result) {
                    // return the error when tools failed
                    resp.error = protocol::Error{
                            protocol::error_code::INTERNAL_ERROR,
                            "Tool execution failed",
                            std::nullopt,
                            req.id.value_or(nullptr)};
                    return resp;
                }

                // check if there is error
                if (result->contains("error")) {
                    // return plugin's error and err msg
                    resp.error = protocol::Error{
                            (*result)["error"].value("code", protocol::error_code::INTERNAL_ERROR),
                            (*result)["error"].value("message", "Unknown error"),
                            std::nullopt,
                            req.id.value_or(nullptr)};
                    return resp;
                }

                // Check if the result is already in the correct MCP format with content array
                if (result->contains("content") && (*result)["content"].is_array()) {
                    // Already in correct format, use directly
                    resp.result = *result;
                } else {
                    // Convert to MCP format
                    nlohmann::json content_array = nlohmann::json::array();

                    // Check if result is a string (text content) or object
                    if (result->is_string()) {
                        // Pure text content from plugin
                        content_array.push_back({{"type", "text"}, {"text", result->get<std::string>()}});
                    } else if (result->is_object() && result->contains("text")) {
                        // Object with text field
                        content_array.push_back({{"type", "text"}, {"text", (*result)["text"]}});
                    } else {
                        // Other JSON content
                        content_array.push_back({{"type", "text"}, {"text", result->dump()}});
                    }

                    resp.result = nlohmann::json{{"content", content_array}};
                }
                return resp;
            } catch (const std::exception &e) {
                resp.error = protocol::Error{
                        protocol::error_code::INTERNAL_ERROR,
                        e.what(),
                        std::nullopt,
                        req.id.value_or(nullptr)};
                return resp;
            }
        }
    }

}// namespace mcp::routers