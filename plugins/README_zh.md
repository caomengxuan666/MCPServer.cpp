# MCP Server++ 插件系统

该目录包含了 MCP Server++ 的插件系统。插件是动态库，通过提供工具来扩展服务器功能，供 AI 模型使用。

## 语言版本

- [English (默认)](README.md)
- [中文版](README_zh.md)

## 插件架构

每个插件都是一个动态库（Windows 上是 `.dll`，Linux 上是 `.so`），它实现了一组特定的函数来与 MCP 服务器集成：

1. `get_tools` - 返回插件提供的工具信息
2. `call_tool` - 使用给定参数执行特定工具
3. `free_result` - 释放为工具结果分配的内存
4. `get_stream_next` - （可选）返回流式工具的下一个函数
5. `get_stream_free` - （可选）返回流式工具的释放函数

## 插件结构

一个典型的插件包括：

- `plugin_name.cpp` - 插件的主要实现
- `tools.json` - 描述插件提供的工具的 JSON 文件
- `CMakeLists.txt` - 构建插件的 CMake 配置

## 创建新插件

使用 `plugin_ctl` 工具生成新的插件模板：

```bash
./plugin_ctl plugin_name
```

这将在 `plugin_name` 目录中创建基本的插件结构。

## 工具定义

插件在 `tools.json` 文件中定义它们的工具，结构如下：

```json
{
  "tools": [
    {
      "name": "tool_name",
      "description": "工具功能描述",
      "parameters": {
        "type": "object",
        "properties": {
          "param1": {
            "type": "string",
            "description": "param1 的描述"
          }
        },
        "required": ["param1"]
      }
    }
  ]
}
```

## 流式工具

插件也可以提供流式工具，可以逐步返回数据。要创建流式工具：

1. 在 `tools.json` 的工具定义中设置 `"is_streaming": true`
2. 实现 `get_stream_next` 和 `get_stream_free` 函数
3. 从 `call_tool` 返回生成器对象而不是 JSON 字符串

## 构建插件

插件使用 CMake 构建。每个插件目录都包含一个 `CMakeLists.txt` 文件，该文件使用 `configure_plugin` 宏：

```cmake
configure_plugin(plugin_name plugin_name.cpp)
```

## 官方插件

此目录包含几个官方插件，演示了各种功能：

- `file_plugin` - 提供文件系统操作
- `http_plugin` - 启用 HTTP 请求
- `safe_system_plugin` - 允许安全的系统命令执行
- `example_stream_plugin` - 演示流式功能

## 插件 SDK

`sdk` 目录包含开发插件所需的头文件和实用程序：

- `mcp_plugin.h` - 核心插件接口定义
- `tool_info_parser.h` - 用于从 JSON 解析工具定义的实用程序
- `tool_info_parser.cpp` - 工具解析实用程序的实现

## 最佳实践

1. 始终在工具实现中验证输入参数
2. 妥善处理错误并返回有意义的错误消息
3. 在 `free_result` 函数中释放任何分配的内存
4. 使用提供的 `ToolInfoParser` 从 JSON 加载工具定义
5. 对于流式工具，确保在释放函数中正确清理资源
6. 遵循官方插件中演示的错误处理模式