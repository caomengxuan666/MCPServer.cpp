<div align="center">
  <img src="docs/logo.png" alt="MCPServer.cpp Logo" width="200"/>
  <h1>MCPServer.cpp</h1>
  <p>A high-performance C++ implementation of the Model Communication Protocol server</p>

[![C++20](https://img.shields.io/badge/C++-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License](https://img.shields.io/github/license/caomengxuan666/MCPServer.cpp)](LICENSE)
[![Build Status](https://img.shields.io/github/actions/workflow/status/caomengxuan666/MCPServer.cpp/build.yml)](https://github.com/caomengxuan666/MCPServer.cpp/actions)
</div>

## Language Versions

- [English (Default)](README.md)
- [中文版](README_zh.md)

## Table of Contents

- [Introduction](#introduction)
- [Features](#features)
- [Architecture](#architecture)
- [Getting Started](#getting-started)
- [Building from Source](#building-from-source)
- [Configuration](#configuration)
- [Authentication](#authentication)
- [HTTPS and Certificate Generation](#https-and-certificate-generation)
- [Plugins](#plugins)
- [API Reference](#api-reference)
- [Docker Deployment](#docker-deployment)
- [CI/CD Pipeline](#cicd-pipeline)
- [Contributing](#contributing)
- [License](#license)

## Introduction

MCPServer.cpp is a high-performance, cross-platform server implementation of the Model Communication Protocol (MCP) written in modern C++. It enables seamless communication between AI models and external tools, providing a standardized interface for extending model capabilities.

The server implements the JSON-RPC 2.0 protocol over HTTP transport and supports both regular request-response and Server-Sent Events (SSE) streaming for real-time communication.

## Features

### MCP Primitives Support Matrix

| Primitive | Status | Notes |
|-----------|--------|-------|
| Tools | ✅ Full Support | Execute tools in isolated plugin environment |
| Prompts | ✅ Basic Support | Prompt templates and management |
| Resources | ✅ Basic Support | Expose data and content to LLMs |
| Sampling | 🚧 Planned | LLM-based sampling operations |
| Roots | 🚧 Planned | Filesystem access control |

### Core Features

- Full implementation of the Model Communication Protocol (MCP)
- JSON-RPC 2.0 over HTTP/HTTPS transport
- Plugin system for extending functionality
- Built-in tools (echo, file operations, HTTP requests, system commands)
- Streaming responses with Server-Sent Events (SSE)
- Comprehensive logging and error handling
- 🚀 **High Performance**: Built with C++20 and optimized with mimalloc for superior performance
- 🔌 **Plugin System**: Extensible architecture with dynamic plugin loading
- 🌐 **HTTP Transport**: Full HTTP/1.1 support with SSE streaming capabilities
- 📦 **JSON-RPC 2.0**: Complete implementation of the JSON-RPC 2.0 specification
- 🛠️ **Built-in Tools**: Includes file operations, HTTP requests, and system commands
- 🧠 **AI Model Ready**: Designed specifically for AI model integration
- 🔄 **Asynchronous I/O**: Powered by ASIO for efficient concurrent handling
- 📊 **Logging**: Comprehensive logging with spdlog
- 📈 **Scalable**: Multi-threaded architecture for handling concurrent requests
- 🌍 **Cross-platform**: Works on Windows, Linux, and macOS
- 📁 **Resource Management**: Expose data and content to LLMs via Resources primitive

## Architecture

MCPServer.cpp uses a modular architecture with clear boundaries between components:

```
┌─────────────────────────────────────────────────────────────┐
│                      MCPServer.cpp                            │
├─────────────────────────────────────────────────────────────┤
│                   Transport Layer                           │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │ HTTP Server │  │  Stdio I/O  │  │   Other Protocols   │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                    Protocol Layer                           │
│              ┌──────────────────────┐                       │
│              │    JSON-RPC 2.0      │                       │
│              └──────────────────────┘                       │
├─────────────────────────────────────────────────────────────┤
│                   Business Logic Layer                      │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │ Tool Reg.   │  │  Plugins    │  │ Request Processing  │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                      Core Services                          │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │   Logger    │  │ Resources   │  │   Configuration   │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### Core Components

1. **Transport Layer**: Handles communication over various protocols (HTTP, stdio, etc.)
2. **Protocol Layer**: Implements JSON-RPC 2.0 message parsing and formatting
3. **Business Logic Layer**: Manages tools, plugins, and request processing
4. **Core Services**: Provides essential services like logging

## Getting Started

### Prerequisites

- C++20 compatible compiler (MSVC, GCC 10+, Clang 12+)
- CMake 3.23 or higher
- Git

### Quick Start

1. Clone the repository:
   ```bash
   git clone https://github.com/caomengxuan666/MCPServer.cpp.git
   cd MCPServer.cpp
   ```

2. Build the project:
   ```bash
   mkdir build
   cd build
   cmake ..
   cmake --build .
   ```

3. Run the server:
   ```bash
   ./bin/mcp-server++
   ```

The server will start on the default port and load the built-in plugins.

## Building from Source

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

### Build Options

| Option | Description | Default |
|--------|-------------|---------|
| `BUILD_TESTS` | Build unit tests | ON |
| `CMAKE_BUILD_TYPE` | Build type (Debug, Release, etc.) | Release |

## Configuration

See [Configuration](#configuration) section for details on how to configure the server.

## Authentication

MCPServer++ supports authentication mechanisms to secure your server from unauthorized access. See [AUTH.md](docs/AUTH.md) for detailed information on how to configure and use authentication.

## HTTPS and Certificate Generation

MCPServer++ supports secure communication over HTTPS. **By default, HTTPS is disabled for security reasons and must be manually enabled in the configuration file.**

To enable HTTPS:
1. Set `enable_https=1` in your config.ini
2. Ensure you have valid SSL certificate files
3. Configure the required SSL file paths

There are two ways to generate SSL/TLS certificates for development and testing:

1. Using the built-in [generate_cert](file:///D:/codespace/MCPServer%2B%2B/tools/generate_cert.cpp#L265-L411) tool (recommended)
2. Using OpenSSL command line tools

For detailed instructions on both methods, please refer to the [HTTPS and Certificate Generation](docs/HTTPS_AND_CERTIFICATES.md) documentation.

## Plugins

MCPServer.cpp supports a powerful plugin system that allows extending functionality without modifying the core server. Plugins are dynamic libraries that implement the MCP plugin interface.

### Official Plugins

- `file_plugin`: File system operations
- `http_plugin`: HTTP client functionality
- `safe_system_plugin`: Secure system command execution
- `example_stream_plugin`: Streaming data example

### Plugin Development

See [plugins/README.md](plugins/README.md) for detailed information on developing custom plugins.

## API Reference

The server implements the JSON-RPC 2.0 protocol over HTTP. All requests should be sent to the `/mcp` endpoint.

### Resource Management

MCPServer++ provides basic support for the MCP Resources primitive, which allows exposing data and content to LLMs. Resources can be accessed through the following JSON-RPC methods:

- `resources/list`: List available resources
- `resources/read`: Read the content of a specific resource
- `resources/write`: Write content to a specific resource (if permitted)

### Example Resource Request

```
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "resources/read",
  "params": {
    "name": "example.txt"
  }
}
```

### Example Resource Response

```
{
  "jsonrpc": "2.0",
  "id": 2,
  "result": {
    "content": "This is the content of the example resource.",
    "contentType": "text/plain",
    "lastModified": "2025-05-13T10:00:00Z"
  }
}
```

### Example Tools Request (Existing)

```
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "tools",
  "params": {}
}
```

### Example Tools Response (Existing)

```
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": [
    {
      "name": "read_file",
      "description": "Read a file",
      "inputSchema": {
        "type": "object",
        "properties": {
          "path": {
            "type": "string",
            "description": "Path to the file to read"
          }
        },
        "required": ["path"]
      }
    }
  ]
}
```

## Docker Deployment

### Build and Run
1. **Multi-stage build** (Image size optimized to 10-15MB)
   ```bash
   docker build -t mcp-server .
   docker run -p 6666:6666 -v $(pwd)/plugins:/plugins -v $(pwd)/certs:/certs mcp-server
   ```

2. **HTTPS Configuration** (Certificate directory mounting required)
   ```bash
   # Enable HTTPS by setting enable_https=1 in config.ini
   # Certificate files must be placed in /certs directory inside container
   docker run -p 6667:6667 -v $(pwd)/certs:/certs mcp-server
   ```

### Plugin System
- **Plugin Path Mapping**: Container plugin directory is `/plugins`, recommend mapping local directory via volume
- **Plugin Loading**: Supports runtime hot-loading (ensure correct file permissions)

### Image Optimization
- Based on minimal `gcr.io/distroless/cc-debian12` base image
- Static linking + debug info stripping (LTO optimized)
- Domestic users can configure mirror accelerators (see configuration examples)

## Docker Hub

Pre-built Docker images are available on Docker Hub: [https://hub.docker.com/r/mgzy/mcp-server](https://hub.docker.com/r/mgzy/mcp-server)

You can pull and run the latest image directly:
```bash
docker pull mgzy/mcp-server
docker run -p 6666:6666 -v $(pwd)/plugins:/plugins -v $(pwd)/certs:/certs mgzy/mcp-server
```

## CI/CD Pipeline

Our project uses GitHub Actions for continuous integration and deployment. The pipeline automatically builds and tests the server on multiple platforms:

### Supported Platforms

- **Ubuntu 22.04** (GitHub Actions latest LTS)
- **Ubuntu 24.04** (GitHub Actions latest Ubuntu release)
- **Windows Server 2022** (GitHub Actions latest Windows)

### Build Variants

We provide two build variants to suit different needs:
1. **Full build**: Includes all libraries and development headers
2. **Minimal build**: Contains only the executable and essential files (no development headers or libraries)

### Packaging Formats

The CI/CD pipeline generates packages in multiple formats:
- **Windows**: ZIP, NSIS installer (EXE)
- **Linux**: DEB, RPM, TAR.GZ, ZIP

## Contributing

We welcome contributions from the community! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines on how to contribute to this project.

### Development Setup

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

<div align="center">
  <p>Built with ❤️ for the AI community</p>
  <p><a href="https://github.com/caomengxuan666/MCPServer.cpp">GitHub</a> | <a href=https://github.com/caomengxuan666/MCPServer.cpp/issues>Issues</a></p>
</div>
