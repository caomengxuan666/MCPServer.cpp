#include "mcp_dispatcher.h"
#include "core/logger.h"
#include "protocol/json_rpc.h"
#include "transport/session.h"
#include <sstream>

using namespace mcp::core;
McpDispatcher::McpDispatcher() {
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
    //oss << "Connection: close\r\n";// force close after response
    oss << "Connection: keep-alive\r\n";
    oss << "\r\n";
    oss << json_body;

    MCP_DEBUG("[Sending Json Response]:\n{}", oss.str());

    auto send_task = [session, response = oss.str()]() mutable -> asio::awaitable<void> {
        try {
            session->clear_buffer();
            co_await session->write(response);
        } catch (const std::exception &e) {
            MCP_ERROR("Failed to send JSON response: {}", e.what());
        }
        //session->close();
    };
    asio::co_spawn(session->get_socket().get_executor(), send_task, asio::detached);
}

void McpDispatcher::send_sse_error_event(std::shared_ptr<mcp::transport::Session> session, const std::string &message) {
    mcp::protocol::Request error_request;
    error_request.method = "error";
    error_request.params = {{"message", message}};

    std::string json_string = nlohmann::json{
            {"jsonrpc", "2.0"},
            {"method", error_request.method},
            {"params", error_request.params}}
                                      .dump();

    std::string sse_error = "event: error\n" + json_string + "\n\n";

    MCP_DEBUG("[Error Sending SSE]: {}", sse_error);
    asio::co_spawn(session->get_socket().get_executor(), [session, sse_error]() -> asio::awaitable<void> {
            co_await session->write(sse_error);
            session->close();
            co_return; }, asio::detached);
}
