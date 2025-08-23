# Python Plugin for MCPServer++

This is an example Python plugin for MCPServer++. It demonstrates how to create a Python plugin that can be compiled into a DLL and loaded by MCPServer++.

## How it works

Python plugins work by creating a DLL that wraps the Python code with a C interface. The DLL contains the standard MCP plugin functions (`get_tools`, `call_tool`, `free_result`) which interact with the Python code through pybind11.

## Building the Plugin

1. Make sure you have the required dependencies installed:
   - Python (3.6 or higher)
   - CMake (3.23 or higher)
   - A C++ compiler with C++17 support
   - pybind11

2. Copy [python_plugin_CMakeLists.txt](file:///d:/codespace/MCPServer%2B%2B/plugins/sdk/python_plugin_CMakeLists.txt) from the SDK directory to this directory and rename it to `CMakeLists.txt`:
   ```bash
   cp ../../sdk/python_plugin_CMakeLists.txt CMakeLists.txt
   ```

3. Edit `CMakeLists.txt` and set the correct path to your MCPServer++ root directory:
   ```cmake
   set(MCP_SERVER_ROOT "/path/to/mcpserver++")
   ```

4. Create a build directory and build the plugin:
   ```bash
   mkdir build
   cd build
   cmake ..
   cmake --build . --config Release
   ```

5. The resulting DLL file can be placed in the plugins directory of your MCPServer++ installation.

## Plugin Structure

- `python_example_plugin.py` - The main Python plugin code
- `CMakeLists.txt` - Build configuration (copied from the SDK)

## Customizing the Plugin

To create your own Python plugin:

1. Copy this example directory and rename it
2. Modify `python_example_plugin.py` to implement your tools
3. Update the `get_tools()` function to return information about your tools
4. Update the `call_tool()` function to implement the functionality of your tools
5. Build the plugin following the steps above

## API Reference

### ToolInfo Class

The `ToolInfo` class represents a tool provided by the plugin:

```python
class ToolInfo:
    def __init__(self, name, description, parameters, is_streaming=False):
        self.name = name                  # Tool name
        self.description = description    # Tool description
        self.parameters = parameters      # JSON Schema string describing the parameters
        self.is_streaming = is_streaming  # Whether the tool is streaming
```

### get_tools() Function

This function should return a list of `ToolInfo` objects describing the tools provided by the plugin:

```python
def get_tools():
    return [
        ToolInfo(
            name="tool_name",
            description="Tool description",
            parameters='{"type": "object", "properties": {...}}'
        )
    ]
```

### call_tool() Function

This function is called when one of the plugin's tools is invoked:

```python
def call_tool(name, args_json):
    # Parse the arguments
    args = json.loads(args_json)
    
    # Implement tool functionality
    if name == "tool_name":
        # Process the tool call
        result = {"result": "tool result"}
        return json.dumps(result)
    
    # Handle unknown tools
    return json.dumps({"error": {"type": "unknown_tool", "message": f"Unknown tool: {name}"}})
```

## Notes

- The plugin must be compiled into a DLL to be loaded by MCPServer++
- The Python interpreter is embedded in the DLL, so the plugin can run without an external Python installation
- All data exchange between the C++ host and Python code happens through JSON strings
- Error handling should be done carefully to prevent crashes