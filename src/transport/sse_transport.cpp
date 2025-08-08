/*
#include "sse_transport.h"
#include "core/logger.h"
#include "protocol/json_rpc.h"

namespace mcp::transport {

    SSETransport::SSETransport(std::shared_ptr<business::ToolRegistry> registry)
        : registry_(std::move(registry)) {
        if (!registry_) {
            throw std::runtime_error("ToolRegistry cannot be null");
        }
        plugin_manager_ = registry_->get_plugin_manager();
        if (!plugin_manager_) {
            throw std::runtime_error("PluginManager cannot be null");
        }
    }

    bool SSETransport::start(uint16_t port, const std::string &address) {
        running_ = true;
        MCP_INFO("SSE Transport started on {}:{}", address, port);
        return true;
    }

    void SSETransport::stop() {
        running_ = false;
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        active_sse_sessions_.clear();
        MCP_INFO("SSE Transport stopped");
    }

    void SSETransport::send_error_event(std::shared_ptr<Session> session, const std::string &message) {
        nlohmann::json error_event = {
                {"jsonrpc", "2.0"},
                {"method", "error"},
                {"params", {{"message", message}}}};
        std::string sse_error = "event: error\n" + error_event.dump() + "\n\n";
        asio::co_spawn(session->get_socket().get_executor(), [session, sse_error]() -> asio::awaitable<void> {
                co_await session->write(sse_error);
                co_return; }, asio::detached);
    }

    void SSETransport::handle_request(const std::string &json_message,
                                      std::shared_ptr<Session> session,
                                      const std::string &session_id) {
        auto req = mcp::protocol::parse_request(json_message);
        if (!req) {
            send_error_event(session, "Invalid JSON-RPC request format");
            return;
        }

        try {
            if (req->method == "callTool") {
                auto params = req->params;
                std::string tool_name = params.value("name", "");
                auto args = params.value("arguments", nlohmann::json{});

                auto tool_info = registry_->get_tool_info(tool_name);
                if (!tool_info) {
                    throw std::runtime_error("Tool not found: " + tool_name);
                }

                if (tool_info->is_streaming) {
                    MCP_INFO("Handling streaming tool: {}", tool_name);
                    std::string sse_header = "HTTP/1.1 200 OK\r\n";
                    sse_header += "Content-Type: text/event-stream\r\n";
                    sse_header += "Cache-Control: no-cache, no-transform\r\n";
                    sse_header += "Connection: keep-alive\r\n";
                    if (!session_id.empty()) {
                        sse_header += "Mcp-Session-Id: " + session_id + "\r\n";
                    }
                    sse_header += "\r\n";

                    asio::co_spawn(session->get_socket().get_executor(), [session, sse_header]() -> asio::awaitable<void> {
                            co_await session->write(sse_header);
                            co_return; }, asio::detached);
                    {
                        std::lock_guard<std::mutex> lock(sessions_mutex_);
                        active_sse_sessions_[session_id] = session;
                    }
                    StreamGenerator generator = plugin_manager_->start_streaming_tool(tool_name, args);
                    if (!generator) {
                        send_error_event(session, "Failed to start streaming tool: " + tool_name);
                        return;
                    }

                    auto [stream_next, stream_free] = plugin_manager_->get_stream_functions(generator);
                    if (!stream_next || !stream_free) {
                        send_error_event(session, "Streaming functions not found for tool: " + tool_name);
                        if (stream_free) stream_free(generator);
                        return;
                    }

                    asio::co_spawn(session->get_socket().get_executor(), [session, session_id, generator, stream_next, stream_free, this]() -> asio::awaitable<void> {
                            MCP_INFO("Starting stream consumer for session: {}", session_id);
                            const char *result_json = nullptr;
                            int status = 0;
                            try {
                                while (true) {
                                    status = stream_next(generator, &result_json);
                                    if (status == 1) {
                                        MCP_DEBUG("Stream ended gracefully for session: {}", session_id);
                                        break;
                                    } else if (status == -1) {
                                        std::string error_msg = result_json ? result_json : "Unknown stream error";
                                        MCP_ERROR("Stream error for session {}: {}", session_id, error_msg);
                                        send_error_event(session, error_msg);
                                        break;
                                    } else {
                                        if (result_json) {
                                            std::string event_str = "event: message\n" + std::string(result_json) + "\n\n";
                                            co_await session->write(event_str);
                                        }
                                    }
                                }
                                stream_free(generator);
                            } catch (const std::exception &e) {
                                MCP_ERROR("Exception in stream consumer for session {}: {}", session_id, e.what());
                                send_error_event(session, "Stream consumer error: " + std::string(e.what()));
                            }
                            {
                                std::lock_guard<std::mutex> lock(sessions_mutex_);
                                active_sse_sessions_.erase(session_id);
                            }
                            MCP_INFO("Stream consumer ended for session: {}", session_id);
                            co_return; }, asio::detached);
                } else {
                    MCP_WARN("Non-streaming tool requested via SSE transport: {}", tool_name);
                    send_error_event(session, "Non-streaming tool requested via SSE transport");
                }
            } else {
                MCP_WARN("Unsupported method for SSE transport: {}", req->method);
                send_error_event(session, "Unsupported method for SSE transport: " + req->method);
            }
        } catch (const std::exception &e) {
            MCP_ERROR("Error handling SSE request: {}", e.what());
            asio::post(session->get_socket().get_executor(),
                       [this, session, msg = std::string(e.what())]() {
                           send_error_event(session, "Request handling error: " + msg);
                       });
        }
    }

}// namespace mcp::transport
*/