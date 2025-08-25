"""
Example Python Plugin for MCPServer++

This plugin demonstrates how to create a Python plugin that can be compiled into 
a DLL and loaded by MCPServer++.

To compile this plugin into a DLL:
1. Copy the python_plugin_CMakeLists.txt to this directory and rename it to CMakeLists.txt
2. Modify the MCP_SERVER_ROOT path in CMakeLists.txt
3. Build using CMake as described in the CMakeLists.txt file
"""

class ToolInfo:
    """Tool information structure"""
    def __init__(self, name, description, parameters, is_streaming=False):
        self.name = name
        self.description = description
        self.parameters = parameters  # JSON Schema string
        self.is_streaming = is_streaming

def get_tools():
    """
    Get the list of tools provided by this plugin
    This function will be called by the C++ wrapper
    """
    return [
        ToolInfo(
            name="python_echo",
            description="Echo back the input text",
            parameters='{"type": "object", "properties": {"text": {"type": "string", "description": "Text to echo"}}, "required": ["text"]}'
        ),
        ToolInfo(
            name="python_calculate",
            description="Perform a simple calculation",
            parameters='{"type": "object", "properties": {"expression": {"type": "string", "description": "Mathematical expression to evaluate"}}, "required": ["expression"]}'
        )
    ]

def call_tool(name, args_json):
    """
    Call a specific tool with arguments
    This function will be called by the C++ wrapper
    
    Args:
        name: Tool name
        args_json: JSON string with tool arguments
        
    Returns:
        JSON string with tool result
    """
    import json
    
    try:
        args = json.loads(args_json) if args_json else {}
    except json.JSONDecodeError:
        return json.dumps({"error": {"type": "invalid_json", "message": "Invalid JSON in arguments"}})
    
    if name == "python_echo":
        text = args.get("text", "")
        return json.dumps({"result": text})
        
    elif name == "python_calculate":
        expression = args.get("expression", "")
        try:
            result = eval(expression, {"__builtins__": {}}, {})
            return json.dumps({"result": result})
        except Exception as e:
            return json.dumps({"error": {"type": "calculation_error", "message": str(e)}})
            
    else:
        return json.dumps({"error": {"type": "unknown_tool", "message": f"Unknown tool: {name}"}})

# For testing purposes when running the script directly
if __name__ == "__main__":
    # This section is for testing the plugin directly
    print("Tools:", [tool.name for tool in get_tools()])
    
    result = call_tool("python_echo", '{"text": "Hello from Python!"}')
    print("Echo result:", result)
    
    result = call_tool("python_calculate", '{"expression": "2+2"}')
    print("Calculation result:", result)