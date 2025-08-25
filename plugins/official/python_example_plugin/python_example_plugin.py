"""
Example Python Plugin for MCPServer++

This plugin demonstrates how to create a Python plugin that can be compiled into 
a DLL and loaded by MCPServer++.

To compile this plugin into a DLL:
1. Copy the python_plugin_CMakeLists.txt to this directory and rename it to CMakeLists.txt
2. Modify the MCP_SERVER_ROOT path in CMakeLists.txt
3. Build using CMake as described in the CMakeLists.txt file
"""

# Import our new SDK
import sys
import os

# Try multiple methods to find the SDK
def find_sdk_path():
    """Find the SDK path using multiple strategies"""
    # Strategy 1: Relative to this file (development)
    sdk_path = os.path.join(os.path.dirname(__file__), '..', '..', 'sdk')
    if os.path.exists(os.path.join(sdk_path, 'mcp_sdk.py')):
        return sdk_path
    
    # Strategy 2: Relative to plugin location (installed)
    plugin_dir = os.path.dirname(__file__)
    sdk_path = os.path.join(plugin_dir, 'mcp_sdk.py')
    if os.path.exists(sdk_path):
        # SDK is in the same directory as this plugin
        return plugin_dir
    
    # Strategy 3: Check if mcp_sdk.py is directly accessible
    if os.path.exists('mcp_sdk.py'):
        return '.'
    
    # If all else fails, return the original path for error message
    return os.path.join(os.path.dirname(__file__), '..', '..', 'sdk')

# Add the SDK path to sys.path so we can import it
sdk_path = find_sdk_path()
sys.path.insert(0, sdk_path)

try:
    from mcp_sdk import tool, string_param, call_tool, get_tools
except ImportError as e:
    # Create a fallback implementation for testing
    print(f"Warning: Could not import mcp_sdk: {e}")
    print(f"  Tried path: {sdk_path}")
    
    # Minimal fallback implementation for testing
    def tool(name=None, description="", **parameters):
        def decorator(func):
            func._tool_name = name or func.__name__
            func._tool_description = description
            func._tool_parameters = parameters
            return func
        return decorator
    
    def string_param(description="", required=False, default=None):
        return {"type": "string", "description": description, 
                "required": required, "default": default}
    
    # Fallback functions that mimic the real interface
    def get_tools():
        return []
    
    def call_tool(name, args_json):
        import json
        return json.dumps({"error": {"type": "unavailable", "message": "SDK not available"}})

@tool(
    name="python_echo",
    description="Echo back the input text",
    text=string_param(description="Text to echo", required=True)
)
def echo_tool(text: str) -> str:
    """Echo the input text"""
    return text

@tool(
    name="python_calculate",
    description="Perform a simple calculation",
    expression=string_param(description="Mathematical expression to evaluate", required=True)
)
def calculate_tool(expression: str) -> str:
    """Calculate a mathematical expression"""
    try:
        # Note: In a real implementation, you should use a safer evaluation method
        # This is just for demonstration purposes
        result = eval(expression, {"__builtins__": {}}, {})
        return str(result)
    except Exception as e:
        return f"Error: {str(e)}"

# For testing purposes when running the script directly
if __name__ == "__main__":
    # This section is for testing the plugin directly
    tools = get_tools()
    print("Tools:", [getattr(tool, '_tool_name', 'unknown') for tool in [echo_tool, calculate_tool]])
    
    result = call_tool("python_echo", '{"text": "Hello from Python!"}')
    print("Echo result:", result)
    
    result = call_tool("python_calculate", '{"expression": "2+2"}')
    print("Calculation result:", result)