# MCP Server++ Plugins

This directory contains the plugin system for the MCP Server++. Plugins are dynamic libraries that extend the server's functionality by providing tools that can be used by AI models.

## Language Versions

- [English (Default)](README.md)
- [中文版](README_zh.md)

## Plugin Architecture

Each plugin is a dynamic library (`.dll` on Windows, `.so` on Linux) that implements a specific set of functions to integrate with the MCP server:

1. `get_tools` - Returns information about the tools provided by the plugin
2. `call_tool` - Executes a specific tool with given arguments
3. `free_result` - Frees memory allocated for tool results
4. `get_stream_next` - (Optional) Returns the next function for streaming tools
5. `get_stream_free` - (Optional) Returns the free function for streaming tools

## Plugin Structure

A typical plugin consists of:

- `plugin_name.cpp` - The main plugin implementation
- `tools.json` - JSON file describing the tools provided by the plugin
- `CMakeLists.txt` - CMake configuration for building the plugin

## Creating a New Plugin

Use the `plugin_ctl` tool to generate a new plugin template:

```bash
./plugin_ctl plugin_name
```

This will create a new directory `plugin_name` with the basic plugin structure.

## Tools Definition

Plugins define their tools in a `tools.json` file with the following structure:

```json
{
  "tools": [
    {
      "name": "tool_name",
      "description": "Description of what the tool does",
      "parameters": {
        "type": "object",
        "properties": {
          "param1": {
            "type": "string",
            "description": "Description of param1"
          }
        },
        "required": ["param1"]
      }
    }
  ]
}
```

## Streaming Tools

Plugins can also provide streaming tools that return data incrementally. To create a streaming tool:

1. Set `"is_streaming": true` in the tool definition in `tools.json`
2. Implement the `get_stream_next` and `get_stream_free` functions
3. Return a generator object from `call_tool` instead of a JSON string

## Building Plugins

Plugins are built using CMake. Each plugin directory contains a `CMakeLists.txt` file that uses the `configure_plugin` macro:

```cmake
configure_plugin(plugin_name plugin_name.cpp)
```

## Official Plugins

This directory includes several official plugins that demonstrate various capabilities:

- `file_plugin` - Provides file system operations
- `http_plugin` - Enables HTTP requests
- `safe_system_plugin` - Allows safe system command execution
- `example_stream_plugin` - Demonstrates streaming capabilities

## Plugin SDK

The `sdk` directory contains the headers and utilities needed to develop plugins:

- `mcp_plugin.h` - Core plugin interface definitions
- `tool_info_parser.h` - Utilities for parsing tool definitions from JSON
- `tool_info_parser.cpp` - Implementation of tool parsing utilities

## Best Practices

1. Always validate input parameters in your tool implementations
2. Handle errors gracefully and return meaningful error messages
3. Free any allocated memory in the `free_result` function
4. Use the provided `ToolInfoParser` to load tool definitions from JSON
5. For streaming tools, ensure proper resource cleanup in the free function
6. Follow the error handling pattern demonstrated in official plugins

## example of usage:
```bash
 curl -X POST http://localhost:6666/mcp -H "Content-Type: application/json" -H "Accept: text/event-stream" -H "Last-Event-ID: 25" -H "Mcp-Session-Id: c1996ba031f882b4f8d0788deed90e1d" -d '{"jsonrpc":"2.0","method":"tools/call","params":{"name":"example_stream","arguments":{}}}'
```

> This is a simple example of a JSON-RPC 2.0 server in C. It uses libcurl for making HTTP requests and libjansson for JSON parsing. The server supports two methods: `tools/list` and `tools/call`. The `tools/list` method returns a list of available tools, while the `tools/call` method calls a tool and returns its result. The server also supports streaming tools, which are tools that return a stream of results.
