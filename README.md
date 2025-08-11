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
- [HTTPS and Certificate Generation](#https-and-certificate-generation)
- [Plugins](#plugins)
- [API Reference](#api-reference)
- [CI/CD Pipeline](#cicd-pipeline)
- [Contributing](#contributing)
- [License](#license)

## Introduction

MCPServer.cpp is a high-performance, cross-platform server implementation of the Model Communication Protocol (MCP) written in modern C++. It enables seamless communication between AI models and external tools, providing a standardized interface for extending model capabilities.

The server implements the JSON-RPC 2.0 protocol over HTTP transport and supports both regular request-response and Server-Sent Events (SSE) streaming for real-time communication.

## Features

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
│  ┌─────────────┐                                             │
│  │   Logger    │                                             │
│  └─────────────┘                                             │
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

The server uses an INI-style configuration file (`config.ini`) for runtime settings. During the build process, CMake automatically copies the example configuration file to the build directory.

### Configuration File

The configuration file contains settings for:

- Server IP address and port
- Logging options (level, path, file size, rotation)
- Plugin directory location
- Transport protocols (stdio, HTTP, HTTPS)

An example configuration file (`config.ini.example`) is provided in the project root. During the build process, CMake copies this file to the build directory as `config.ini`. You can modify this file to customize the server behavior.

Key configuration options include:

- `ip`: Server binding IP address (default: 127.0.0.1)
- `port`: Network port for incoming connections (default: 6666)
- `http_port`: HTTP transport port (set to 0 to disable HTTP)
- `https_port`: HTTPS transport port (set to 0 to disable HTTPS)
- `log_level`: Logging severity (trace, debug, info, warn, error)
- `log_path`: Filesystem path for log storage
- `plugin_dir`: Directory containing plugin modules
- `enable_stdio`: Enable stdio transport (1=enable, 0=disable)
- `enable_http`: Enable HTTP transport (1=enable, 0=disable)
- `enable_https`: Enable HTTPS transport (1=enable, 0=disable) - HTTPS is disabled by default for security reasons
- `ssl_cert_file`: SSL certificate file path (required for HTTPS)
- `ssl_key_file`: SSL private key file path (required for HTTPS)
- `ssl_dh_params_file`: SSL Diffie-Hellman parameters file path (required for HTTPS)

To customize the configuration:

1. Copy `config.ini.example` to `config.ini` in the project root
2. Modify the settings as needed
3. Rebuild the project - CMake will copy your customized config to the build directory

Example configuration:
``ini
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

### Example Request

``json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "tools",
  "params": {}
}
```

### Example Response

``json
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
