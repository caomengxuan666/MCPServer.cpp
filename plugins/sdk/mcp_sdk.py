"""
MCP Server++ Python SDK

This SDK provides a simple way to create MCP plugins using Python with decorators
and other syntactic sugar to make plugin development more intuitive and less error-prone.
"""

import json
import functools
from typing import Any, Dict, List, Callable, Optional, Union
from dataclasses import dataclass, field
from enum import Enum


class ToolType(Enum):
    """Tool types supported by MCP"""
    STANDARD = "standard"
    STREAMING = "streaming"


@dataclass
class ToolParameter:
    """Represents a tool parameter"""
    type: str
    description: str
    required: bool = False
    default: Any = None


@dataclass
class ToolInfo:
    """Tool information structure"""
    name: str
    description: str
    parameters: Dict[str, ToolParameter] = field(default_factory=dict)
    tool_type: ToolType = ToolType.STANDARD
    
    def to_schema(self) -> str:
        """Convert to JSON schema string"""
        schema = {
            "type": "object",
            "properties": {},
            "required": []
        }
        
        for param_name, param in self.parameters.items():
            prop = {
                "type": param.type,
                "description": param.description
            }
            
            if param.default is not None:
                prop["default"] = param.default
                
            schema["properties"][param_name] = prop
            
            if param.required:
                schema["required"].append(param_name)
                
        return json.dumps(schema)


class MCPPlugin:
    """
    Main plugin class that manages tools and their execution
    """
    
    def __init__(self, name: str = ""):
        self.name = name
        self._tools: Dict[str, Callable] = {}
        self._tool_infos: Dict[str, ToolInfo] = {}
        self._initialized = False
    
    def tool(self, 
             name: Optional[str] = None, 
             description: str = "", 
             tool_type: ToolType = ToolType.STANDARD,
             **parameters: ToolParameter):
        """
        Decorator to register a function as an MCP tool
        
        Args:
            name: Tool name (defaults to function name)
            description: Tool description
            tool_type: Type of tool (standard or streaming)
            **parameters: Tool parameters as ToolParameter objects
        """
        def decorator(func: Callable) -> Callable:
            tool_name = name or func.__name__
            
            # Create tool info
            tool_info = ToolInfo(
                name=tool_name,
                description=description,
                parameters=parameters,
                tool_type=tool_type
            )
            
            # Register tool
            self._tools[tool_name] = func
            self._tool_infos[tool_name] = tool_info
            
            @functools.wraps(func)
            def wrapper(*args, **kwargs):
                return func(*args, **kwargs)
                
            return wrapper
        return decorator
    
    def get_tools(self) -> List[ToolInfo]:
        """Get list of all registered tools"""
        return list(self._tool_infos.values())
    
    def call_tool(self, name: str, args_json: str) -> str:
        """
        Call a specific tool with JSON arguments
        
        Args:
            name: Tool name
            args_json: JSON string with tool arguments
            
        Returns:
            JSON string with tool result
        """
        try:
            args = json.loads(args_json) if args_json else {}
        except json.JSONDecodeError:
            return json.dumps({
                "error": {
                    "type": "invalid_json", 
                    "message": "Invalid JSON in arguments"
                }
            })
        
        if name not in self._tools:
            return json.dumps({
                "error": {
                    "type": "unknown_tool", 
                    "message": f"Unknown tool: {name}"
                }
            })
        
        try:
            # Call the tool function with arguments
            result = self._tools[name](**args)
            
            # Handle streaming tools differently
            if self._tool_infos[name].tool_type == ToolType.STREAMING:
                # For streaming tools, we might need special handling
                # This is a simplified implementation
                if hasattr(result, '__iter__') and not isinstance(result, (str, bytes)):
                    # If it's a generator or iterator, convert to list
                    result = list(result)
            
            # Return result as JSON
            if isinstance(result, dict) and "error" in result:
                return json.dumps(result)
            else:
                return json.dumps({"result": result})
                
        except Exception as e:
            return json.dumps({
                "error": {
                    "type": "execution_error",
                    "message": str(e)
                }
            })


# Global plugin instance
_plugin = MCPPlugin()


def tool(name: Optional[str] = None, 
         description: str = "", 
         tool_type: ToolType = ToolType.STANDARD,
         **parameters: ToolParameter):
    """
    Decorator to register a function as an MCP tool at module level
    
    Args:
        name: Tool name (defaults to function name)
        description: Tool description
        tool_type: Type of tool (standard or streaming)
        **parameters: Tool parameters as ToolParameter objects
    """
    return _plugin.tool(name, description, tool_type, **parameters)


def get_tools():
    """
    Get the list of tools provided by this plugin
    This function will be called by the C++ wrapper
    """
    # Convert our ToolInfo objects to the format expected by C++
    tools = []
    for tool_info in _plugin.get_tools():
        tools.append(type('ToolInfo', (), {
            'name': tool_info.name,
            'description': tool_info.description,
            'parameters': tool_info.to_schema(),
            'is_streaming': tool_info.tool_type == ToolType.STREAMING
        })())
    return tools


def call_tool(name: str, args_json: str) -> str:
    """
    Call a specific tool with arguments
    This function will be called by the C++ wrapper
    
    Args:
        name: Tool name
        args_json: JSON string with tool arguments
        
    Returns:
        JSON string with tool result
    """
    return _plugin.call_tool(name, args_json)


# Convenience functions for creating parameters
def string_param(description: str = "", required: bool = False, default: str = None) -> ToolParameter:
    """Create a string parameter"""
    return ToolParameter(type="string", description=description, required=required, default=default)


def integer_param(description: str = "", required: bool = False, default: int = None) -> ToolParameter:
    """Create an integer parameter"""
    return ToolParameter(type="integer", description=description, required=required, default=default)


def number_param(description: str = "", required: bool = False, default: float = None) -> ToolParameter:
    """Create a number parameter"""
    return ToolParameter(type="number", description=description, required=required, default=default)


def boolean_param(description: str = "", required: bool = False, default: bool = None) -> ToolParameter:
    """Create a boolean parameter"""
    return ToolParameter(type="boolean", description=description, required=required, default=default)


def array_param(items_type: str = "string", description: str = "", required: bool = False) -> ToolParameter:
    """Create an array parameter"""
    return ToolParameter(type="array", description=description, required=required)


def object_param(description: str = "", required: bool = False) -> ToolParameter:
    """Create an object parameter"""
    return ToolParameter(type="object", description=description, required=required)