# Python Plugins for MCP Server++

Python plugins provide an easy way to extend the functionality of the MCP Server++ using Python. With the new Python SDK, developers can create plugins more intuitively using decorators and other syntactic sugar.

## Table of Contents

- [Introduction](#introduction)
- [Python SDK Overview](#python-sdk-overview)
- [Creating a Python Plugin](#creating-a-python-plugin)
- [Plugin Structure](#plugin-structure)
- [Using the Python SDK](#using-the-python-sdk)
- [Building Python Plugins](#building-python-plugins)
- [Best Practices](#best-practices)

## Introduction

Python plugins are dynamic libraries (DLL on Windows, so on Linux) that wrap Python code using pybind11. They implement the standard MCP plugin interface while allowing the actual plugin logic to be written in Python.

## Python SDK Overview

The Python SDK (`mcp_sdk.py`) provides a set of utilities and decorators to make plugin development more intuitive:

1. `@tool` decorator - Register functions as MCP tools
2. Parameter helper functions - Easily define tool parameters
3. `ToolType` enum - Specify standard or streaming tools
4. Automatic JSON handling - Automatically convert between Python objects and JSON

## Creating a Python Plugin

Use the `plugin_ctl` tool to generate a new Python plugin template:

```bash
./plugin_ctl create -p my_python_plugin
```

This will create a new directory `my_python_plugin` with the basic Python plugin structure.

## Plugin Structure

A typical Python plugin consists of:

- `plugin_name.py` - The main Python plugin implementation using the SDK
- `tools.json` - JSON file describing the tools provided by the plugin
- `CMakeLists.txt` - CMake configuration for building the plugin
- `mcp_sdk.py` - The Python SDK (automatically copied during build)

## Using the Python SDK

### Basic Tool Definition

```python
from mcp_sdk import tool, string_param

@tool(
    name="hello_world",
    description="A simple greeting tool",
    name_param=string_param(description="The name to greet", required=True)
)
def hello_world_tool(name_param: str):
    return f"Hello, {name_param}!"
```

### Streaming Tool Definition

```python
from mcp_sdk import tool, integer_param, ToolType

@tool(
    name="stream_counter",
    description="Stream a sequence of numbers",
    tool_type=ToolType.STREAMING,
    count=integer_param(description="Number of items to stream", default=5)
)
def stream_counter_tool(count: int = 5):
    for i in range(count):
        yield {"number": i, "text": f"Item {i}"}
```

### Parameter Helper Functions

The SDK provides helper functions for defining common parameter types:

- `string_param()` - Define a string parameter
- `integer_param()` - Define an integer parameter
- `number_param()` - Define a float parameter
- `boolean_param()` - Define a boolean parameter
- `array_param()` - Define an array parameter
- `object_param()` - Define an object parameter

## Building Python Plugins

Python plugins are built using CMake, just like C++ plugins. The build process automatically:

1. Compiles the C++ wrapper code
2. Copies the Python plugin file
3. Copies the Python SDK

To build a Python plugin:

```bash
cd my_python_plugin
mkdir build
cd build
cmake ..
cmake --build .
```

The resulting DLL/SO file can then be used with the MCP Server++.

## Best Practices

1. **Use the SDK decorators**: They handle JSON conversion and tool registration automatically
2. **Define clear parameter schemas**: Use parameter helper functions to document your tool's interface
3. **Handle errors gracefully**: Python exceptions will be automatically converted to MCP error responses
4. **Use type hints**: They improve code readability and help catch errors early
5. **Test streaming tools**: Ensure your generators work correctly and clean up resources properly
6. **Document your tools**: Provide clear descriptions for tools and parameters
7. **Keep plugin files organized**: For complex plugins, consider splitting code into multiple modules