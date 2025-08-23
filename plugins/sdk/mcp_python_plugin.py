"""
MCPServer++ Python Plugin SDK

This module provides the base classes and utilities for creating Python plugins
that can be loaded by the MCPServer++.

Example usage:
```python
from mcp_python_plugin import MCPPlugin, ToolInfo

class MyPlugin(MCPPlugin):
    def get_tools(self):
        return [
            ToolInfo(
                name="hello_world",
                description="A simple hello world tool",
                parameters='{"type": "object", "properties": {}}'
            )
        ]
    
    def call_tool(self, name, args_json):
        if name == "hello_world":
            return '{"result": "Hello, World!"}'
        else:
            raise ValueError(f"Unknown tool: {name}")

# Create plugin instance
plugin = MyPlugin()

# Export required functions
get_tools = plugin.get_tools_wrapper
call_tool = plugin.call_tool_wrapper
free_result = plugin.free_result_wrapper
```
"""

import json
import ctypes
from typing import List, Optional, Any
from dataclasses import dataclass
from abc import ABC, abstractmethod

@dataclass
class ToolInfo:
    """Tool information structure"""
    name: str
    description: str
    parameters: str  # JSON Schema string
    is_streaming: bool = False

@dataclass
class MCPError:
    """Error information structure"""
    code: int = 0
    message: str = ""
    details: str = ""
    source: str = ""

class MCPPlugin(ABC):
    """
    Base class for MCP Python plugins
    """
    
    def __init__(self):
        self._results = {}  # Store results to prevent garbage collection
        self._result_counter = 0
        
    @abstractmethod
    def get_tools(self) -> List[ToolInfo]:
        """
        Get the list of tools provided by this plugin
        """
        pass
        
    @abstractmethod
    def call_tool(self, name: str, args_json: str) -> str:
        """
        Call a specific tool with arguments
        
        Args:
            name: Tool name
            args_json: JSON string with tool arguments
            
        Returns:
            JSON string with tool result
        """
        pass
        
    def get_tools_wrapper(self, count) -> ctypes.POINTER(ToolInfo):
        """
        Wrapper for the get_tools function to be exported
        """
        try:
            tools = self.get_tools()
            count.contents.value = len(tools)
            
            # Convert to ctypes array
            ToolInfoArray = ToolInfo * len(tools)
            self._tools_array = ToolInfoArray(*tools)
            return ctypes.cast(self._tools_array, ctypes.POINTER(ToolInfo))
        except Exception as e:
            count.contents.value = 0
            return None
            
    def call_tool_wrapper(self, name: ctypes.c_char_p, args_json: ctypes.c_char_p, error: ctypes.POINTER(MCPError)) -> ctypes.c_char_p:
        """
        Wrapper for the call_tool function to be exported
        """
        try:
            name_str = name.decode('utf-8') if name else ""
            args_str = args_json.decode('utf-8') if args_json else "{}"
            
            result = self.call_tool(name_str, args_str)
            
            # Store result to prevent garbage collection
            self._result_counter += 1
            result_key = self._result_counter
            self._results[result_key] = result
            
            # Return as c_char_p
            return ctypes.c_char_p(result.encode('utf-8'))
        except Exception as e:
            if error:
                error.contents.code = -1
                error.contents.message = str(e)
            return None
            
    def free_result_wrapper(self, result: ctypes.c_char_p):
        """
        Wrapper for the free_result function to be exported
        """
        # In Python, we don't need to manually free memory,
        # but we can remove it from our tracking dict
        try:
            if result:
                # Find and remove the result from our tracking dict
                result_str = ctypes.cast(result, ctypes.c_char_p).value
                if result_str:
                    # Find the key for this result and remove it
                    keys_to_remove = []
                    for key, value in self._results.items():
                        if value.encode('utf-8') == result_str:
                            keys_to_remove.append(key)
                    
                    for key in keys_to_remove:
                        del self._results[key]
        except Exception:
            pass  # Ignore errors in cleanup

# Example plugin implementation
class ExamplePlugin(MCPPlugin):
    """
    Example plugin implementation showing how to create a Python plugin
    """
    
    def get_tools(self) -> List[ToolInfo]:
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
    
    def call_tool(self, name: str, args_json: str) -> str:
        args = json.loads(args_json) if args_json else {}
        
        if name == "python_echo":
            text = args.get("text", "")
            return json.dumps({"result": text})
            
        elif name == "python_calculate":
            expression = args.get("expression", "")
            try:
                result = eval(expression)
                return json.dumps({"result": result})
            except Exception as e:
                return json.dumps({"error": {"type": "calculation_error", "message": str(e)}})
                
        else:
            raise ValueError(f"Unknown tool: {name}")

# For testing purposes
if __name__ == "__main__":
    # This section is for testing the plugin directly
    plugin = ExamplePlugin()
    print("Tools:", plugin.get_tools())
    
    result = plugin.call_tool("python_echo", '{"text": "Hello from Python!"}')
    print("Echo result:", result)
    
    result = plugin.call_tool("python_calculate", '{"expression": "2+2"}')
    print("Calculation result:", result)