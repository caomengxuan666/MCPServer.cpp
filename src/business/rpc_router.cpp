#include "rpc_router.h"
#include "business/plugin_manager.h"
#include "core/logger.h"
#include "protocol/json_rpc.h"
#include "protocol/tool.h"
#include "transport/session.h"
#include "version.h"
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <string>

namespace mcp::business {

    RpcRouter::RpcRouter() {}

    void RpcRouter::register_handler(const std::string &method, RpcHandler handler) {
        handlers_[method] = std::move(handler);
    }

    std::optional<RpcHandler> RpcRouter::find_handler(const std::string &method) const {
        auto it = handlers_.find(method);
        if (it != handlers_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    protocol::Response RpcRouter::route_request(
            const protocol::Request &req,
            std::shared_ptr<ToolRegistry> registry,
            std::shared_ptr<transport::Session> session,
            const std::string &session_id) const {

        std::string method = req.method;

        // HTTP URL TO RPC METHOD (e.g. /tools/list in url ->method: tools/list)
        // It is not standard,but you can use this to debug in your browser or other tools conveniently
        if (method == "/tools/list") {
            method = "tools/list";
        } else if (method == "/tools/call") {
            method = "tools/call";
        }

        auto handler = find_handler(method);
        if (handler.has_value()) {
            return handler.value()(req, registry, session, session_id);
        } else {
            protocol::Response resp;
            resp.id = req.id.value_or(nullptr);
            resp.result = nlohmann::json::parse(protocol::make_error(
                    protocol::error_code::METHOD_NOT_FOUND,
                    "Method not supported: " + method,
                    req.id.value_or(nullptr)));
            return resp;
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    // @brief Handle initialize request
    // @param req Request object
    // @param registry Tool registry
    // @param session Session object
    // @param session_id Session ID
    // @return Response object
    ////////////////////////////////////////////////////////////////////////////
    protocol::Response handle_initialize(
            const protocol::Request &req,
            std::shared_ptr<ToolRegistry> /*registry*/,
            std::shared_ptr<transport::Session> /*session*/,
            const std::string & /*session_id*/) {

        protocol::Response resp;
        resp.id = req.id.value_or(nullptr);

        std::string client_protocol_version = req.params.value("protocolVersion", "2.0");
        auto server_version = PROJECT_VERSION;
        auto server_name = PROJECT_NAME;

        resp.result = nlohmann::json{
                {"protocolVersion", client_protocol_version},
                {"capabilities", {{"tools", {{"listChanged", true}}}}},
                {"serverInfo", {{"name", server_name}, {"version", server_version}}}};
        return resp;
    }
    ////////////////////////////////////////////////////////////////////////////
    // @brief Handle tools/list request
    // @param req Request object
    // @param registry Tool registry
    // @param session Session object
    // @param session_id Session ID
    // @return Response object
    ////////////////////////////////////////////////////////////////////////////
    protocol::Response handle_tools_list(
            const protocol::Request &req,
            std::shared_ptr<ToolRegistry> registry,
            std::shared_ptr<transport::Session> /*session*/,
            const std::string & /*session_id*/) {

        protocol::Response resp;
        resp.id = req.id.value_or(nullptr);

        auto tools = registry->get_all_tools();
        nlohmann::json tools_json = nlohmann::json::array();
        for (const auto &tool: tools) {
            nlohmann::json tool_json = {
                    {"name", tool.name},
                    {"description", tool.description}};
            if (!tool.parameters.is_null() && !tool.parameters.empty()) {
                tool_json["inputSchema"] = tool.parameters;
            }
            if (tool.is_streaming) {
                tool_json["isStreaming"] = true;
            }
            tools_json.push_back(tool_json);
        }
        resp.result = nlohmann::json{{"tools", tools_json}};
        return resp;
    }

    ////////////////////////////////////////////////////////////////////////////
    // @brief Handle tools/call request
    // @param req Request object
    // @param registry Tool registry
    // @param session Session object
    // @param session_id Session ID
    // @return Response object
    ////////////////////////////////////////////////////////////////////////////

    protocol::Response handle_tools_call(
            const protocol::Request &req,
            std::shared_ptr<ToolRegistry> registry,
            std::shared_ptr<transport::Session> session,
            [[maybe_unused]] const std::string &session_id) {
        protocol::Response resp;
        resp.id = req.id.value_or(nullptr);

        auto params = req.params;
        std::string tool_name = params.value("name", "");
        auto args = params.value("arguments", nlohmann::json{});

        auto tool_info = registry->get_tool_info(tool_name);
        if (!tool_info) {
            resp.result = nlohmann::json::parse(protocol::make_error(
                    protocol::error_code::METHOD_NOT_FOUND,
                    "Tool not found: " + tool_name,
                    req.id.value_or(nullptr)));
            return resp;
        }

        auto plugin_manager = registry->get_plugin_manager();
        if (!plugin_manager) {
            resp.result = nlohmann::json::parse(protocol::make_error(
                    protocol::error_code::INTERNAL_ERROR,
                    "PluginManager not found",
                    req.id.value_or(nullptr)));
            return resp;
        }

        // Check if the client support SSE.
        std::string accept_header = session->get_accept_header();
        bool client_supports_sse = (accept_header.find("text/event-stream") != std::string::npos);

        MCP_DEBUG("Tool name: {}", tool_name);
        MCP_DEBUG("Tool is_streaming: {}", tool_info->is_streaming);
        MCP_DEBUG("Client supports SSE: {}", client_supports_sse);

        // SSE logics
        if (tool_info->is_streaming && client_supports_sse) {
            MCP_INFO("Upgrading to SSE stream for tool: {}", tool_name);

            // SSE Header
            std::string sse_header = "HTTP/1.1 200 OK\r\n";
            sse_header += "Content-Type: text/event-stream\r\n";
            sse_header += "Cache-Control: no-cache, no-transform\r\n";
            sse_header += "Connection: keep-alive\r\n";
            sse_header += "Mcp-Session-Id: " + session->get_session_id() + "\r\n";
            sse_header += "\r\n";
            sse_header += "\r\n";

            // send SSE header
            asio::co_spawn(session->get_socket().get_executor(), [session, sse_header]() -> asio::awaitable<void> {
            co_await session->write(sse_header);
            co_return; }, asio::detached);

            // send SSE init
            {
                std::string init_event = nlohmann::json{
                        {"jsonrpc", "2.0"},
                        {"id", req.id.value_or(1)},
                        {"session_id", session->get_session_id()}}
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

            // launch concrete plugin tools
            StreamGenerator generator = plugin_manager->start_streaming_tool(tool_name, args);
            if (!generator) {
                // send error event
                std::string error_msg = "Failed to start streaming tool: " + tool_name;
                std::string json_string = nlohmann::json{
                        {"jsonrpc", "2.0"},
                        {"method", "error"},
                        {"params", {{"message", error_msg}}}}
                                                  .dump();
                std::string sse_error = "event: error\n" + json_string + "\n\n";
                asio::co_spawn(session->get_socket().get_executor(), [session, sse_error]() -> asio::awaitable<void> {
                co_await session->write(sse_error);
                session->close();
                co_return; }, asio::detached);

                // mark the response as handled,no more json response
                resp.id = nullptr;
                resp.result = nlohmann::json::value_t::discarded;
                return resp;
            }

            // get the stream function
            auto [stream_next, stream_free] = plugin_manager->get_stream_functions(generator);
            if (!stream_next || !stream_free) {
                std::string error_msg = "Streaming functions not found for tool: " + tool_name;
                if (stream_free) stream_free(generator);

                // send stream function error
                std::string json_string = nlohmann::json{
                        {"jsonrpc", "2.0"},
                        {"method", "error"},
                        {"params", {{"message", error_msg}}}}
                                                  .dump();
                std::string sse_error = "event: error\n" + json_string + "\n\n";
                asio::co_spawn(session->get_socket().get_executor(), [session, sse_error]() -> asio::awaitable<void> {
                co_await session->write(sse_error);
                session->close();
                co_return; }, asio::detached);

                // mark the response as handled
                resp.id = nullptr;
                resp.result = nlohmann::json::value_t::discarded;
                return resp;
            }

            // launch the consumer thread,It is the core of the streaming
            asio::co_spawn(session->get_socket().get_executor(), [session, generator, stream_next, stream_free, req]() -> asio::awaitable<void> {
            const char* result_json = nullptr;
            int status = 0;
            int event_id = 1;  // incr from 1.The init func's event_id is 0
            try {
                while (true) {
                    status = stream_next(generator, &result_json);
                    if (status == 1) {
                        MCP_DEBUG("Stream ended gracefully.");
                        break;
                    } else if (status == -1) {
                        std::string error_msg = result_json ? result_json : "Unknown stream error";
                        MCP_ERROR("Stream error: {}", error_msg);
                        
                        // send error event
                        std::string json_string = nlohmann::json{
                                {"jsonrpc", "2.0"},
                                {"method", "error"},
                                {"params", {{"message", error_msg}}}
                        }.dump();
                        std::string sse_error = "event: error\n" + json_string + "\n\n";
                        co_await session->write(sse_error);
                        break;
                    } else {
                        if (result_json) {
                            // make a copy of the json and apply event and event_id
                            // we can use the session_id to supoort client's reconnection
                            // and we can use the SSE event_it to resume the process of the tool
                            const std::string json_copy(result_json); 
                            
                            if (!json_copy.empty() && json_copy[0] == '{') {
                                std::string event_str = 
                                    "event: message\n"
                                    "id: " + std::to_string(event_id) + "\n"
                                    "data: " + json_copy + "\n\n";
                                co_await session->write(event_str);
                                ++event_id;
                            } else {
                                MCP_ERROR("Invalid JSON data: {}", json_copy);
                            }
                        }
                    }
                }
            } catch (const std::exception &e) {
                MCP_ERROR("Stream consumer exception: {}", e.what());
                std::string error_msg = "Stream error: " + std::string(e.what());
                std::string json_string = nlohmann::json{
                        {"jsonrpc", "2.0"},
                        {"method", "error"},
                        {"params", {{"message", error_msg}}}
                }.dump();
                std::string sse_error = "event: error\n" + json_string + "\n\n";
                
                // write in new coroutine
                asio::co_spawn(session->get_socket().get_executor(), 
                              [session, sse_error]() -> asio::awaitable<void> {
                    co_await session->write(sse_error);
                    co_return;
                }, asio::detached);
            }
            // free the stream and close the session
            stream_free(generator);
            session->close();
            co_return; }, asio::detached);

            // mark the response as SSE
            resp.id = nullptr;
            resp.result = nlohmann::json::value_t::discarded;
            return resp;
        }
        // synchronous call
        else {
            auto result = registry->execute(tool_name, args);
            if (!result) {
                resp.result = nlohmann::json::parse(protocol::make_error(
                        protocol::error_code::METHOD_NOT_FOUND,
                        "Tool not found: " + tool_name,
                        req.id.value_or(nullptr)));
                return resp;
            }

            nlohmann::json content_array = nlohmann::json::array();
            content_array.push_back({{"type", "text"}, {"text", result->dump()}});
            resp.result = nlohmann::json{{"content", content_array}};
            return resp;
        }
    }
    ////////////////////////////////////////////////////////////////////////////
    // @brief Handle exit request
    // @param req Request object
    // @param registry Tool registry
    // @param session Session object
    // @param session_id Session ID
    // @return Response object
    ////////////////////////////////////////////////////////////////////////////
    protocol::Response handle_exit(
            [[maybe_unused]] const protocol::Request &req,
            std::shared_ptr<ToolRegistry> /*registry*/,
            std::shared_ptr<transport::Session> /*session*/,
            [[maybe_unused]] const std::string & /*session_id*/) {
        MCP_INFO("Exit command received");
        std::exit(0);
    }

}// namespace mcp::business
