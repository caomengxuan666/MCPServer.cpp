# MCP Server++ 的 Python 插件

Python 插件提供了一种简单的方法，可以使用 Python 扩展 MCP Server++ 的功能。通过新的 Python SDK，开发人员可以使用装饰器和其他语法糖更直观地创建插件。

## 目录

- [简介](#简介)
- [Python SDK 概述](#python-sdk-概述)
- [创建 Python 插件](#创建-python-插件)
- [插件结构](#插件结构)
- [使用 Python SDK](#使用-python-sdk)
- [构建 Python 插件](#构建-python-插件)
- [最佳实践](#最佳实践)

## 简介

Python 插件是包装 Python 代码的动态库（Windows 上是 DLL，Linux 上是 so），使用 pybind11 实现。它们实现了标准的 MCP 插件接口，同时允许使用 Python 编写实际的插件逻辑。

## Python SDK 概述

Python SDK ([mcp_sdk.py](file://d:\codespace\MCPServer++\plugins\sdk\mcp_sdk.py)) 提供了一组实用程序和装饰器，使插件开发更加直观：

1. [@tool](file://d:\codespace\MCPServer++\plugins\sdk\mcp_sdk.py#L173-L186) 装饰器 - 将函数注册为 MCP 工具
2. 参数辅助函数 - 轻松定义工具参数
3. [ToolType](file://d:\codespace\MCPServer++\plugins\sdk\mcp_sdk.py#L17-L20) 枚举 - 指定标准或流式工具
4. 自动 JSON 处理 - 在 Python 对象和 JSON 之间自动转换

## 创建 Python 插件

使用 [plugin_ctl](file:///D:/codespace/MCPServer++/tools/plugin_ctl.cpp#L759-L759) 工具生成新的 Python 插件模板：

```bash
./plugin_ctl create -p my_python_plugin
```

这将创建一个新的目录 [my_python_plugin](file://d:\codespace\MCPServer++\tests\my_python_plugin)，其中包含基本的 Python 插件结构。

## 插件结构

一个典型的 Python 插件包括：

- `plugin_name.py` - 使用 SDK 的主要 Python 插件实现
- `tools.json` - 描述插件提供的工具的 JSON 文件
- `CMakeLists.txt` - 构建插件的 CMake 配置
- [mcp_sdk.py](file://d:\codespace\MCPServer++\plugins\sdk\mcp_sdk.py) - Python SDK（构建期间自动复制）

## 使用 Python SDK

### 基本工具定义

```python
from mcp_sdk import tool, string_param

@tool(
    name="hello_world",
    description="一个简单的问候工具",
    name_param=string_param(description="要问候的名称", required=True)
)
def hello_world_tool(name_param: str):
    return f"Hello, {name_param}!"
```

### 流式工具定义

```python
from mcp_sdk import tool, integer_param, ToolType

@tool(
    name="stream_counter",
    description="流式传输一系列数字",
    tool_type=ToolType.STREAMING,
    count=integer_param(description="要流式传输的项目数", default=5)
)
def stream_counter_tool(count: int = 5):
    for i in range(count):
        yield {"number": i, "text": f"项目 {i}"}
```

### 参数辅助函数

SDK 提供了用于定义常见参数类型的辅助函数：

- [string_param()](file://d:\codespace\MCPServer++\plugins\sdk\mcp_sdk.py#L222-L224) - 定义字符串参数
- [integer_param()](file://d:\codespace\MCPServer++\plugins\sdk\mcp_sdk.py#L227-L229) - 定义整数参数
- [number_param()](file://d:\codespace\MCPServer++\plugins\sdk\mcp_sdk.py#L232-L234) - 定义浮点数参数
- [boolean_param()](file://d:\codespace\MCPServer++\plugins\sdk\mcp_sdk.py#L237-L239) - 定义布尔参数
- [array_param()](file://d:\codespace\MCPServer++\plugins\sdk\mcp_sdk.py#L242-L244) - 定义数组参数
- [object_param()](file://d:\codespace\MCPServer++\plugins\sdk\mcp_sdk.py#L247-L249) - 定义对象参数

## 构建 Python 插件

Python 插件使用 CMake 构建，就像 C++ 插件一样。构建过程会自动：

1. 编译 C++ 包装代码
2. 复制 Python 插件文件
3. 复制 Python SDK

构建 Python 插件：

```bash
cd my_python_plugin
mkdir build
cd build
cmake ..
cmake --build .
```

生成的 DLL/SO 文件可以与 MCP Server++ 一起使用。

## 最佳实践

1. **使用 SDK 装饰器**：它们会自动处理 JSON 转换和工具注册
2. **定义清晰的参数模式**：使用参数辅助函数记录工具的接口
3. **妥善处理错误**：Python 异常将自动转换为 MCP 错误响应
4. **使用类型提示**：它们提高了代码可读性并帮助及早发现错误
5. **测试流式工具**：确保您的生成器正常工作并正确清理资源
6. **记录您的工具**：为工具和参数提供清晰的描述
7. **保持插件文件有序**：对于复杂的插件，考虑将代码拆分为多个模块