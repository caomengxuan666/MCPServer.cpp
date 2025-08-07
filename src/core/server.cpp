#include "server.h"
#include "business/plugin_manager.h"
#include "business/tool_registry.h"
#include "core/logger.h"
#include "protocol/tool.h"
#include <thread>


namespace mcp::core {

    MCPserver::Builder::Builder() {
        server_ = std::unique_ptr<MCPserver>(new MCPserver());
    }

    MCPserver::Builder &MCPserver::Builder::with_builtin_tools() {
        return with_echo_tool();
    }

    MCPserver::Builder &MCPserver::Builder::with_plugin_path(const std::string &path) {
        // Store the plugin directory path for later loading
        server_->plugin_directories_.push_back(path);
        return *this;
    }

    MCPserver::Builder &MCPserver::Builder::with_echo_tool() {
        server_->should_register_echo_tool_ = true;
        return *this;
    }

    MCPserver::Builder &MCPserver::Builder::with_plugin(const std::string &path) {
        std::string plugin_path;
#ifdef _WIN32
        plugin_path = path + ".dll";
#elif defined(__APPLE__)
        plugin_path = path + ".dylib";
#elif defined(_POSIX)
        plugin_path = path + ".so";
#else
        plugin_path = path;
#endif
        server_->plugin_paths_.push_back(plugin_path);
        return *this;
    }

    std::unique_ptr<MCPserver> MCPserver::Builder::build() {
        // init core components
        server_->registry_ = std::make_shared<business::ToolRegistry>();
        server_->plugin_manager_ = std::make_unique<business::PluginManager>();
        server_->registry_->set_plugin_manager(server_->plugin_manager_);

        server_->request_handler_ = std::make_unique<business::RequestHandler>(
                server_->registry_,

                [server_ptr = server_.get()](const std::string &resp,
                                             std::shared_ptr<transport::Session> session,
                                             [[maybe_unused]] const std::string &session_id) {
                    // pass session_id
                    server_ptr->dispatcher_->send_json_response(session, resp, 200);
                });


        MCP_TRACE("Created ToolRegistry (initial size: {})", server_->registry_->get_all_tool_names().size());

        // register built-in tools
        if (server_->should_register_echo_tool_) {
            mcp::protocol::Tool echo_tool = mcp::protocol::make_echo_tool();
            auto executor = [](const nlohmann::json &args) -> nlohmann::json {
                return args.value("text", "no text provided");
            };

            server_->registry_->register_builtin(echo_tool, executor);
            MCP_INFO("Registered built-in echo tool");
        }

        // Load plugins from directories
        for (const auto &directory: server_->plugin_directories_) {
            MCP_TRACE("Loading plugins from directory: {}", directory);
            server_->plugin_manager_->load_plugins_from_directory(directory);
        }

        // Register tools from plugins loaded from directories
        auto all_tools = server_->plugin_manager_->get_all_tools();
        MCP_INFO("Found {} tools from all plugins loaded from directories", all_tools.size());

        for (const auto &tool_info: all_tools) {
            if (!tool_info.name) {
                MCP_WARN("Skipping tool with null name");
                continue;
            }

            std::string tool_name = tool_info.name;
            MCP_DEBUG("Registering tool: '{}' from plugin", tool_name);

            // bind the tool call to the plugin manager
            auto executor = [server_ptr = server_.get(), tool_name](const nlohmann::json &args) {
                return server_ptr->plugin_manager_->call_tool(tool_name, args);
            };

            // register the tool in the registry table
            server_->registry_->register_plugin_tool(tool_info, executor);
        }

        // 3. load plugins and register their tools
        for (const auto &path: server_->plugin_paths_) {
            MCP_TRACE("Processing plugin: {}", path);
            if (!server_->plugin_manager_->load_plugin(path)) {
                MCP_ERROR("Skipping invalid plugin: {}", path);
                continue;
            }

            // get current tools from the plugin and register them
            auto tools = server_->plugin_manager_->get_tools_from_plugin(path);
            MCP_INFO("Found {} tools in plugin: {}", tools.size(), path);

            for (const auto &tool_info: tools) {
                if (!tool_info.name) {
                    MCP_WARN("Skipping tool with null name");
                    continue;
                }

                std::string tool_name = tool_info.name;

                // bind the tool call to the plugin manager
                auto executor = [server_ptr = server_.get(), tool_name](const nlohmann::json &args) {
                    return server_ptr->plugin_manager_->call_tool(tool_name, args);
                };

                // register the tool in the registry table
                server_->registry_->register_plugin_tool(tool_info, executor);
            }
        }

        // debug: print all registered tools
        auto final_tools = server_->registry_->get_all_tool_names();
        MCP_INFO("Final tools in registry (total: {}):", final_tools.size());
        for (const auto &name: final_tools) {
            MCP_INFO("  - '{}'", name);
        }


        server_->dispatcher_ = std::make_unique<McpDispatcher>(server_->registry_);

        // Automatically start HTTP transport as part of the build process
        if (!server_->start_http_transport(6666, "0.0.0.0")) {
            MCP_ERROR("Failed to start HTTP transport during server build");
        } else {
            MCP_INFO("Streamable HTTP Transport started on 0.0.0.0:6666");
        }

        return std::move(server_);
    }

    void MCPserver::start() {
        MCP_INFO("MCPServer.cpp is ready. Send JSON-RPC messages via stdin.");
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    bool MCPserver::start_http_transport(uint16_t port, const std::string &address) {
        try {
            http_transport_ = std::make_unique<mcp::transport::HttpTransport>(address, port);

            auto success = http_transport_->start([this](const std::string &msg,
                                                         std::shared_ptr<mcp::transport::Session> session,
                                                         const std::string &session_id) {
                MCP_DEBUG("HTTP message received: \n{}", msg);
                // handle by dispatcher
                //dispatcher_->handle_request(msg, session, session_id);
                request_handler_->handle_request(msg, session, session_id);
            });

            if (success) {
                MCP_INFO("HTTP Transport started on {}:{}", address, port);
            } else {
                MCP_ERROR("Failed to start HTTP Transport on {}:{}", address, port);
            }

            return success;
        } catch (const std::exception &e) {
            MCP_ERROR("Exception when starting HTTP Transport: {}", e.what());
            return false;
        }
    }

    void MCPserver::run() {
        if (http_transport_) {
            http_transport_->get_io_context().run();
        }
    }

    asio::io_context &MCPserver::get_io_context() {
        return http_transport_->get_io_context();
    }
}// namespace mcp::core