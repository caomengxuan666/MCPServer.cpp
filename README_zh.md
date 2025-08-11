<div align="center">
  <img src="docs/logo.png" alt="MCPServer.cpp Logo" width="200"/>
  <h1>MCPServer.cpp</h1>
  <p>高性能的 C++ 实现模型通信协议服务器</p>

[![C++20](https://img.shields.io/badge/C++-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License](https://img.shields.io/github/license/caomengxuan666/MCPServer.cpp)](LICENSE)
[![Build Status](https://img.shields.io/github/actions/workflow/status/caomengxuan666/MCPServer.cpp/build.yml)](https://github.com/caomengxuan666/MCPServer.cpp/actions)
</div>

## 语言版本

- [English (默认)](README.md)
- [中文版](README_zh.md)

## 目录

- [简介](#简介)
- [功能特性](#功能特性)
- [架构](#架构)
- [快速开始](#快速开始)
- [从源码构建](#从源码构建)
- [配置](#配置)
- [插件](#插件)
- [API 参考](#api-参考)
- [CI/CD 流水线](#cicd-流水线)
- [贡献](#贡献)
- [许可证](#许可证)

## 简介

MCPServer.cpp 是一个使用现代 C++ 编写的高性能、跨平台的模型通信协议（MCP）服务器实现。它能够实现 AI 模型与外部工具之间的无缝通信，为扩展模型功能提供标准化接口。

该服务器通过 HTTP 传输实现了 JSON-RPC 2.0 协议，并支持常规请求-响应和服务器发送事件（SSE）流式传输，以实现实时通信。

## 功能特性

- 完整实现模型通信协议（MCP）
- 基于 HTTP/HTTPS 的 JSON-RPC 2.0 传输协议
- 插件系统，可扩展功能
- 内置工具（echo、文件操作、HTTP请求、系统命令）
- 使用服务器发送事件（SSE）的流式响应
- 全面的日志记录和错误处理


然后在 config.ini 文件中配置服务器：
```ini
[server]
https_enabled = true
https_port = 6667
ssl_cert_file = server.crt
ssl_private_key_file = server.key
```

服务器将同时监听 HTTP（端口 6666）和 HTTPS（端口 6667）端点，允许客户端选择其首选的连接方式。

## 架构

MCPServer.cpp 采用模块化架构，各组件之间界限清晰：

```
┌─────────────────────────────────────────────────────────────┐
│                      MCPServer.cpp                            │
├─────────────────────────────────────────────────────────────┤
│                    传输层 (Transport Layer)                  │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │ HTTP 服务器 │  │  标准I/O    │  │   其他协议          │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                    协议层 (Protocol Layer)                   │
│              ┌──────────────────────┐                       │
│              │    JSON-RPC 2.0      │                       │
│              └──────────────────────┘                       │
├─────────────────────────────────────────────────────────────┤
│                  业务逻辑层 (Business Logic Layer)           │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │ 工具注册表  │  │   插件      │  │  请求处理           │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                      核心服务 (Core Services)               │
│  ┌─────────────┐                                             │
│  │   日志系统  │                                             │
│  └─────────────┘                                             │
└─────────────────────────────────────────────────────────────┘
```

### 核心组件

1. **传输层**: 处理各种协议的通信（HTTP、stdio 等）
2. **协议层**: 实现 JSON-RPC 2.0 消息解析和格式化
3. **业务逻辑层**: 管理工具、插件和请求处理
4. **核心服务**: 提供日志等基本服务

## 快速开始

### 环境要求

- C++20 兼容编译器 (MSVC, GCC 10+, Clang 12+)
- CMake 3.23 或更高版本
- Git

### 快速开始指南

1. 克隆仓库：
   ```bash
   git clone https://github.com/caomengxuan666/MCPServer.cpp.git
   cd MCPServer.cpp
   ```

2. 构建项目：
   ```bash
   mkdir build
   cd build
   cmake ..
   cmake --build .
   ```

3. 运行服务器：
   ```bash
   ./bin/mcp-server++
   ```

服务器将在默认端口启动并加载内置插件。

## 从源码构建

### Windows

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Linux/macOS

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

### 构建选项

| 选项 | 描述 | 默认值 |
|------|------|--------|
| `BUILD_TESTS` | 构建单元测试 | ON |
| `CMAKE_BUILD_TYPE` | 构建类型 (Debug, Release, 等) | Release |

## 配置

服务器使用 INI 格式的配置文件 ([config.ini](file://d:\codespace\MCPServer++\config.ini)) 进行运行时设置。在构建过程中，CMake 会自动将示例配置文件复制到构建目录。

### 配置文件

配置文件包含以下设置：

- 服务器 IP 地址和端口
- 日志选项（级别、路径、文件大小、轮换）
- 插件目录位置
- 传输协议（stdio、HTTP、HTTPS）

项目根目录中提供了示例配置文件 ([config.ini.example](file://d:\codespace\MCPServer++\config.ini.example))。在构建过程中，CMake 会将此文件复制到构建目录并命名为 [config.ini](file://d:\codespace\MCPServer++\config.ini)。您可以修改此文件来自定义服务器行为。

主要配置选项包括：

- `ip`：服务器绑定的 IP 地址（默认：127.0.0.1）
- `port`：用于传入连接的网络端口（默认：6666）
- `http_port`：HTTP 传输端口
- `https_port`：HTTPS 传输端口 
- `log_level`：日志严重性级别（trace, debug, info, warn, error）
- `log_path`：日志存储的文件系统路径
- `plugin_dir`：包含插件模块的目录
- `enable_stdio`：启用 stdio 传输（1=启用，0=禁用）
- `enable_http`：启用 HTTP 传输（1=启用，0=禁用）
- `enable_https`：启用 HTTPS 传输（1=启用，0=禁用）- 出于安全原因，HTTPS 默认禁用
- `ssl_cert_file`：SSL 证书文件路径（HTTPS 必需）
- `ssl_key_file`：SSL 私钥文件路径（HTTPS 必需）
- `ssl_dh_params_file`：SSL Diffie-Hellman 参数文件路径（HTTPS 必需）

要自定义配置：

1. 在项目根目录中将 [config.ini.example](file://d:\codespace\MCPServer++\config.ini.example) 复制为 [config.ini](file://d:\codespace\MCPServer++\config.ini)
2. 根据需要修改设置
3. 重新构建项目 - CMake 会将您的自定义配置复制到构建目录

配置示例：
```
[server]
ip=0.0.0.0
port=6666
http_port=6666
https_port=6667
log_level=info
log_path=logs/mcp_server.log
plugin_dir=plugins
enable_stdio=1
enable_http=1
enable_https=0
ssl_cert_file=certs/server.crt
ssl_key_file=certs/server.key
ssl_dh_params_file=certs/dh2048.pem
```

## HTTPS 和证书生成

MCPServer++ 支持通过 HTTPS 进行安全通信。**出于安全原因，HTTPS 默认是禁用的，必须在配置文件中手动启用。**

要启用 HTTPS：
1. 在 config.ini 中设置 `enable_https=1`
2. 确保拥有有效的 SSL 证书文件
3. 配置所需的 SSL 文件路径

有两种方法可以为 MCPServer++ 生成 SSL/TLS 证书：

1. 使用内置 [generate_cert](file://d:\codespace\MCPServer++\build\bin\generate_cert.exe) 工具（推荐）
2. 使用 OpenSSL 命令行工具

有关这两种方法的详细说明，请参阅 [HTTPS 和证书生成](docs/HTTPS_AND_CERTIFICATES_zh.md) 文档。

## 插件

MCPServer.cpp 支持强大的插件系统，允许在不修改核心服务器的情况下扩展功能。插件是实现 MCP 插件接口的动态库。

### 官方插件

- `file_plugin`: 文件系统操作
- `http_plugin`: HTTP 客户端功能
- `safe_system_plugin`: 安全系统命令执行
- `example_stream_plugin`: 流式数据示例

### 插件开发

有关开发自定义插件的详细信息，请参阅 [plugins/README_zh.md](plugins/README_zh.md)。

## API 参考

服务器通过 HTTP 实现了 JSON-RPC 2.0 协议。所有请求应发送到 `/mcp` 端点。

### 请求示例

``json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "tools",
  "params": {}
}
```

### 响应示例

``json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": [
    {
      "name": "read_file",
      "description": "读取文件",
      "inputSchema": {
        "type": "object",
        "properties": {
          "path": {
            "type": "string",
            "description": "要读取的文件路径"
          }
        },
        "required": ["path"]
      }
    }
  ]
}
```

## CI/CD 流水线

我们的项目使用 GitHub Actions 进行持续集成和部署。流水线会自动在多个平台上构建和测试服务器：

### 支持的平台

- **Ubuntu 22.04** (GitHub Actions 最新 LTS)
- **Ubuntu 24.04** (GitHub Actions 最新 Ubuntu 版本)
- **Windows Server 2022** (GitHub Actions 最新 Windows)

### 构建变体

我们提供两种构建变体以满足不同需求：
1. **完整构建**: 包含所有库和开发头文件
2. **最小构建**: 仅包含可执行文件和必要文件（无开发头文件或库）

### 打包格式

CI/CD 流水线生成多种格式的包：
- **Windows**: ZIP, NSIS 安装程序 (EXE)
- **Linux**: DEB, RPM, TAR.GZ, ZIP

## 贡献

我们欢迎社区的贡献！请查看 [CONTRIBUTING.md](CONTRIBUTING.md) 了解如何为该项目做出贡献的指南。

### 开发设置

1. Fork 仓库
2. 创建功能分支
3. 进行修改
4. 如适用，添加测试
5. 提交拉取请求

## 许可证

该项目基于 MIT 许可证 - 详情请见 [LICENSE](LICENSE) 文件。

---

<div align="center">
  <p>为 AI 社区 ❤️ 而构建</p>
  <p><a href="https://github.com/caomengxuan666/MCPServer.cpp">GitHub</a> | <a href="https://github.com/caomengxuan666/MCPServer.cpp/issues">问题</a></p>
</div>