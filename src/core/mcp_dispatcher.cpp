#include "mcp_dispatcher.h"
#include "business/plugin_manager.h"
#include "core/logger.h"
#include "protocol/json_rpc.h"
#include "protocol/tool.h"
#include "transport/session.h"
#include <sstream>

using namespace mcp::core;
McpDispatcher::McpDispatcher(std::shared_ptr<business::ToolRegistry> registry) : registry_(std::move(registry)) {
    if (!registry_) {
        throw std::runtime_error("ToolRegistry cannot be null");
    }
    plugin_manager_ = registry_->get_plugin_manager();
    if (!plugin_manager_) {
        throw std::runtime_error("PluginManager cannot be null");
    }
}

void McpDispatcher::handle_request(const std::string &json_message,
                                   std::shared_ptr<mcp::transport::Session> session,
                                   const std::string &session_id) {
    auto req = mcp::protocol::parse_request(json_message);
    if (!req) {
        // when parsing fails, send an error response
        auto err = mcp::protocol::make_error(
                mcp::protocol::error_code::PARSE_ERROR,
                "Invalid JSON-RPC request format",
                nullptr);
        send_json_response(session, err, 400);
        return;
    }

    mcp::protocol::Response resp;
    resp.id = req->id.value_or(nullptr);

    try {
        if (req->method == "initialize") {
            MCP_INFO("Received initialize request");
            resp.result = nlohmann::json{{"capabilities", nlohmann::json::object()}};
            send_json_response(session, mcp::protocol::make_response(resp), 200);
        } else if (req->method == "listTools") {
            MCP_INFO("Received listTools request");
            auto tools = registry_->get_all_tools();
            nlohmann::json tools_json = nlohmann::json::array();
            for (const auto &tool: tools) {
                tools_json.push_back({{"name", tool.name},
                                      {"description", tool.description},
                                      {"parameters", tool.parameters}});
            }
            resp.result = tools_json;
            send_json_response(session, mcp::protocol::make_response(resp), 200);
        } else if (req->method == "callTool") {
            auto params = req->params;
            std::string tool_name = params.value("name", "");
            auto args = params.value("arguments", nlohmann::json{});

            auto tool_info = registry_->get_tool_info(tool_name);
            if (!tool_info) {
                throw std::runtime_error("Tool not found: " + tool_name);
            }

            // get Accept header to determine if client supports SSE
            std::string accept_header = session->get_accept_header();
            bool client_supports_sse = (accept_header.find("text/event-stream") != std::string::npos);

            MCP_DEBUG("Tool name: {}", tool_name);
            MCP_DEBUG("Tool is_streaming: {}", tool_info->is_streaming);
            MCP_DEBUG("Client supports SSE: {}", client_supports_sse);

            // Streaming support
            if (tool_info->is_streaming && client_supports_sse) {
                MCP_INFO("Upgrading to SSE stream for tool: {}", tool_name);

                // send SSE headers
                std::string sse_header = "HTTP/1.1 200 OK\r\n";
                sse_header += "Content-Type: text/event-stream\r\n";
                sse_header += "Cache-Control: no-cache, no-transform\r\n";
                sse_header += "Connection: keep-alive\r\n";
                if (!session_id.empty()) {
                    sse_header += "Mcp-Session-Id: " + session_id + "\r\n";
                }
                sse_header += "\r\n";

                // send SSE headers asynchronously
                asio::co_spawn(session->get_socket().get_executor(), [session, sse_header]() -> asio::awaitable<void> {
                        co_await session->write(sse_header);
                        co_return; }, asio::detached);

                // launch streaming tool
                StreamGenerator generator = plugin_manager_->start_streaming_tool(tool_name, args);
                if (!generator) {
                    send_sse_error_event(session, "Failed to start streaming tool: " + tool_name);
                    return;
                }

                auto [stream_next, stream_free] = plugin_manager_->get_stream_functions(generator);
                if (!stream_next || !stream_free) {
                    send_sse_error_event(session, "Streaming functions not found for tool: " + tool_name);
                    if (stream_free) stream_free(generator);
                    return;
                }

                // launch the stream consumer
                asio::co_spawn(session->get_socket().get_executor(), [session, generator, stream_next, stream_free, this]() -> asio::awaitable<void> {
                        const char* result_json = nullptr;
                        int status = 0;
                        try {
                            while(true) {
                                status = stream_next(generator, &result_json);
                                if (status == 1) {
                                    MCP_DEBUG("Stream ended gracefully.");
                                    break;
                                } else if (status == -1) {
                                    std::string error_msg = result_json ? result_json : "Unknown stream error";
                                    MCP_ERROR("Stream error: {}", error_msg);
                                    send_sse_error_event(session, error_msg);
                                    break;
                                } else {
                                    if (result_json) {
                                        std::string event_str = "event: message\n" + std::string(result_json) + "\n\n";
                                        co_await session->write(event_str);
                                    }
                                }
                            }
                        } catch (const std::exception &e) {
                            MCP_ERROR("Stream consumer exception: {}", e.what());
                            send_sse_error_event(session, "Stream error: " + std::string(e.what()));
                        }
                        // release the generator
                        stream_free(generator);
                        // close the session after streaming
                        session->close();
                        co_return; }, asio::detached);
                return;
            } else {
                // synchronous tool call
                auto result = registry_->execute(tool_name, args);
                if (!result) {
                    throw std::runtime_error("Tool not found: " + tool_name);
                }
                resp.result = *result;
                send_json_response(session, mcp::protocol::make_response(resp), 200);
            }
        } else if (req->method == "exit") {
            MCP_INFO("Exit command received, shutting down...");
            std::exit(0);
        } else {
            throw std::runtime_error("Method not supported: " + req->method);
        }
    } catch (const std::exception &e) {
        auto err = mcp::protocol::make_error(
                mcp::protocol::error_code::METHOD_NOT_FOUND,
                e.what(),
                req->id.value_or(nullptr));
        send_json_response(session, err, 400);
    }
}

void McpDispatcher::send_json_response(std::shared_ptr<mcp::transport::Session> session,
                                       const std::string &json_body,
                                       int status_code) {
    // clean buffer before sending response
    session->clear_buffer();

    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " " << (status_code == 200 ? "OK" : "Bad Request") << "\r\n";
    oss << "Content-Type: application/json\r\n";
    oss << "Content-Length: " << json_body.size() << "\r\n";
    oss << "Connection: close\r\n";// force close after response
    oss << "\r\n";
    oss << json_body;

    MCP_DEBUG("【Sending Json Response】:\n{}", oss.str());

    auto send_task = [session, response = oss.str()]() mutable -> asio::awaitable<void> {
        try {
            session->clear_buffer();
            co_await session->write(response);
        } catch (const std::exception &e) {
            MCP_ERROR("Failed to send JSON response: {}", e.what());
        }
        session->close();
    };
    asio::co_spawn(session->get_socket().get_executor(), send_task, asio::detached);
}

void McpDispatcher::send_sse_error_event(std::shared_ptr<mcp::transport::Session> session, const std::string &message) {
    nlohmann::json error_event = {
            {"jsonrpc", "2.0"},
            {"method", "error"},
            {"params", {{"message", message}}}};
    std::string sse_error = "event: error\n" + error_event.dump() + "\n\n";

    MCP_DEBUG("【Error Sending SSE】: {}", sse_error);
    asio::co_spawn(session->get_socket().get_executor(), [session, sse_error]() -> asio::awaitable<void> {
            co_await session->write(sse_error);
            session->close();
            co_return; }, asio::detached);
}