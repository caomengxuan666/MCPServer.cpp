#include "server.h"
#include "Resources/resource.h"
#include "business/plugin_manager.h"
#include "business/tool_registry.h"
#include "core/logger.h"
#include "executable_path.h"
#include "protocol/tool.h"
#include "transport/session.h"
#include "transport/ssl_session.h"
#include <memory>
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
        // find the executable directory first
        auto abosulute_path = getExecutableDirectory() + "/" + path;
        server_->plugin_directories_.push_back(abosulute_path);
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

    MCPserver::Builder &MCPserver::Builder::with_https_port(unsigned short port) {
        https_port_ = port;
        return *this;
    }

    MCPserver::Builder &MCPserver::Builder::with_ssl_certificates(const std::string &cert_file, const std::string &private_key_file, const std::string &dh_params_file) {
        cert_file_ = cert_file;
        private_key_file_ = private_key_file;
        dh_params_file_ = dh_params_file;
        return *this;
    }

    std::unique_ptr<MCPserver> MCPserver::Builder::build() {
        // init core components
        server_->registry_ = std::make_shared<business::ToolRegistry>();
        server_->resource_manager_ = std::make_shared<resources::ResourceManager>();
        server_->plugin_manager_ = std::make_shared<business::PluginManager>();
        server_->registry_->set_plugin_manager(server_->plugin_manager_);

        server_->request_handler_ = std::make_unique<business::RequestHandler>(
                server_->registry_,

                [server_ptr = server_.get()](const std::string &resp,
                                             std::shared_ptr<transport::Session> session,
                                             [[maybe_unused]] const std::string &session_id) {
                    // pass session_id
                    server_ptr->dispatcher_->send_json_response(session, resp, 200);
                });
        server_->prompt_manager_ = std::make_shared<prompts::PromptManager>();

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

        // Register sample resources
        mcp::resources::Resource sample_resource;
        sample_resource.uri = "file://localhost/resources/sample.txt";
        sample_resource.name = "Sample Text Resource";
        sample_resource.description = "A sample text file demonstrating the Resources primitive";
        sample_resource.mimeType = "text/plain";
        server_->resource_manager_->register_resource(sample_resource);

        mcp::resources::ResourceTemplate file_template;
        file_template.uriTemplate = "file://localhost/{path}";
        file_template.name = "File Resource";
        file_template.description = "Template for accessing files on the server";
        file_template.mimeType = "application/octet-stream";
        server_->resource_manager_->register_resource_template(file_template);

        server_->dispatcher_ = std::make_unique<McpDispatcher>();

        // Register sample prompts
        mcp::prompts::Prompt analyze_code_prompt;
        analyze_code_prompt.name = "analyze-code";
        analyze_code_prompt.description = "analyze-code";
        mcp::prompts::PromptArgument language_arg;
        language_arg.name = "language";
        language_arg.description = "programming language";
        language_arg.required = true;
        analyze_code_prompt.arguments.push_back(language_arg);
        server_->prompt_manager_->register_prompt(analyze_code_prompt);

        mcp::prompts::Prompt git_commit_prompt;
        git_commit_prompt.name = "git-commit";
        git_commit_prompt.description = "generate Git commit message";
        mcp::prompts::PromptArgument changes_arg;
        changes_arg.name = "changes";
        changes_arg.description = "Git diff or changes description";
        changes_arg.required = true;
        git_commit_prompt.arguments.push_back(changes_arg);
        server_->prompt_manager_->register_prompt(git_commit_prompt);

        if (enable_http_transport_) {
            // Automatically start HTTP transport as part of the build process
            if (!server_->start_http_transport(port_, address_)) {
                MCP_ERROR("Failed to start HTTP transport during server build");
            } else {
                // HTTP transport started message is logged in start_http_transport
            }
        }

        if (enable_https_transport_) {
            // Automatically start HTTPS transport as part of the build process
            if (!server_->start_https_transport(https_port_, address_, cert_file_, private_key_file_, dh_params_file_)) {
                MCP_ERROR("Failed to start HTTPS transport during server build");
                MCP_ERROR("HTTPS transport will be disabled");
                // Disable HTTPS transport since it failed to start
                enable_https_transport_ = false;
            } else {
                MCP_INFO("HTTPS Transport started on {}:{}", address_, https_port_);
            }
        }

        if (enable_stdio_transport_) {
            // Automatically start Stdio transport as part of the build process
            if (!server_->start_stdio_transport()) {
                MCP_ERROR("Failed to start Stdio transport during server build");
            } else {
                MCP_INFO("Stdio Transport started");
            }
        }

        // Summary of enabled transports
        if (enable_http_transport_ || enable_https_transport_ || enable_stdio_transport_) {
            MCP_INFO("Enabled transports:");
            if (enable_stdio_transport_) {
                MCP_INFO("  - Stdio transport");
            }
            if (enable_http_transport_) {
                MCP_INFO("  - HTTP transport on port {}", port_);
            }
            if (enable_https_transport_) {
                MCP_INFO("  - HTTPS transport on port {}", https_port_);
            }
        } else {
            MCP_WARN("No transports enabled. Server will not be able to receive messages.");
        }
        for (const auto &directory: server_->plugin_directories_) {
            if (server_->plugin_manager_->start_directory_monitoring(directory)) {
                MCP_INFO("Started monitoring plugin directory: {}", directory);
            } else {
                MCP_WARN("Failed to start monitoring for plugin directory: {}", directory);
            }
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
            http_transport_ = std::make_unique<mcp::transport::HttpTransport>(address, port, auth_manager_);

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

    bool MCPserver::start_https_transport(uint16_t port, const std::string &address,
                                          const std::string &cert_file, const std::string &private_key_file, const std::string &dh_params_file) {
        try {
            https_transport_ = std::make_unique<mcp::transport::HttpsTransport>(address, port, cert_file, private_key_file, dh_params_file, auth_manager_);

            auto success = https_transport_->start([this](const std::string &msg,
                                                          std::shared_ptr<mcp::transport::Session> session,
                                                          const std::string &session_id) {
                MCP_DEBUG("HTTPS message received: \n{}", msg);
                // handle by dispatcher
                // convert to SSL session
                auto ssl_session = std::dynamic_pointer_cast<mcp::transport::SslSession>(session);
                request_handler_->handle_request(msg, ssl_session ? ssl_session : session, session_id);
            });

            if (success) {
                MCP_INFO("HTTPS Transport started on {}:{}", address, port);
            } else {
                MCP_ERROR("Failed to start HTTPS Transport on {}:{}", address, port);
                MCP_ERROR("Please make sure the SSL certificate and private key files exist:");
                MCP_ERROR("  Certificate file: {}", cert_file);
                MCP_ERROR("  Private key file: {}", private_key_file);
                MCP_ERROR("  Diffie-Hellman parameters file: {}", dh_params_file);
            }

            return success;
        } catch (const std::exception &e) {
            MCP_ERROR("Exception when starting HTTPS Transport: {}", e.what());
            MCP_ERROR("Please make sure the SSL certificate and private key files exist:");
            MCP_ERROR("  Certificate file: {}", cert_file);
            MCP_ERROR("  Private key file: {}", private_key_file);
            MCP_ERROR("  Diffie-Hellman parameters file: {}", dh_params_file);
            return false;
        }
    }

    bool MCPserver::start_stdio_transport() {
        try {
            // handle stdio
            auto stdio_handler = [this](const std::string &msg) {
                MCP_DEBUG("STDIO message received: {}", msg);

                // use the same handler as http_transport_
                request_handler_->handle_request(msg, nullptr, "");
            };

            // init the transport with the same registry tools as http_transport_
            stdio_transport_ = mcp::transport::StdioTransport(registry_);

            bool success = stdio_transport_.open(stdio_handler);

            if (success) {
                MCP_INFO("STDIO Transport started");
            } else {
                MCP_ERROR("Failed to start STDIO Transport");
            }

            return success;
        } catch (const std::exception &e) {
            MCP_ERROR("Exception when starting STDIO Transport: {}", e.what());
            return false;
        }
    }

    void MCPserver::run() {
        // Check if we have any transport running
        if (http_transport_ || https_transport_) {
            std::vector<std::thread> threads;

            // Run io_context for HTTP transport
            if (http_transport_) {
                threads.emplace_back([this]() {
                    http_transport_->get_io_context().run();
                });
            }

            // Run io_context for HTTPS transport
            if (https_transport_) {
                threads.emplace_back([this]() {
                    https_transport_->get_io_context().run();
                });
            }

            // Join all threads
            for (auto &t: threads) {
                if (t.joinable()) {
                    t.join();
                }
            }
        } else {
            // for http_transport disable and stdio enable
            MCP_INFO("MCPServer is running in stdio-only mode. Waiting for input on stdin...");
            MCP_INFO("Press Ctrl+C to stop the server.");

            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }

    asio::io_context &MCPserver::get_io_context() {
        // Return HTTP context if available, otherwise HTTPS context
        if (http_transport_) {
            return http_transport_->get_io_context();
        } else if (https_transport_) {
            return https_transport_->get_io_context();
        }
        return io_context_;
    }
}// namespace mcp::core