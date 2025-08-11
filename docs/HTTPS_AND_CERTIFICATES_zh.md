# HTTPS 和证书生成

MCPServer++ 支持通过 HTTPS 进行安全通信。本文档解释了如何使用提供的证书生成工具生成 SSL/TLS 证书以及如何配置服务器以使用 HTTPS。

## 目录

- [HTTPS 支持](#https-support)
- [证书生成方法](#certificate-generation-methods)
  - [使用内置证书生成工具（推荐）](#using-the-built-in-certificate-generation-tool-recommended)
  - [使用 OpenSSL 命令行工具](#using-openssl-command-line-tools)
- [证书生成工具](#certificate-generation-tool)
  - [概述](#overview)
  - [使用方法](#usage)
  - [命令行选项](#command-line-options)
  - [示例](#examples)
- [HTTPS 配置](#https-configuration)
  - [启用 HTTPS](#enabling-https)
  - [配置参数](#configuration-parameters)
- [安全注意事项](#security-considerations)

## HTTPS 支持

MCPServer++ 使用 OpenSSL 实现 HTTPS 传输，提供客户端和服务器之间的安全通信。HTTPS 支持默认启用，但需要适当的 SSL/TLS 证书才能正常工作。

服务器支持：
- TLS 1.2 和 TLS 1.3 协议
- 强加密套件
- 基于证书的认证
- 使用 DH 参数实现完美前向保密 (PFS)

## 证书生成方法

有两种方法可以为 MCPServer++ 生成 SSL/TLS 证书：

### 使用内置证书生成工具（推荐）

[generate_cert](generate_cert) 工具用于轻松创建完整的公钥基础设施 (PKI) 环境，包括：
- 证书颁发机构 (CA) 证书和密钥
- 由 CA 签名的服务器证书和密钥
- 用于密钥交换的 Diffie-Hellman (DH) 参数

要生成证书，请先构建项目，然后从构建目录运行该工具：

```bash
# 首先构建项目
mkdir build
cd build
cmake ..
cmake --build .

# 运行证书生成工具
./bin/generate_cert
```

默认情况下，这将创建一个包含所有必要文件的 `certs` 目录：
- `ca.crt` - CA 证书
- `ca.key` - CA 私钥
- `server.crt` - 服务器证书
- `server.key` - 服务器私钥
- `dh2048.pem` - DH 参数

### 使用 OpenSSL 命令行工具

或者，您可以使用 OpenSSL 命令行工具生成证书。这种方法可以更好地控制证书生成过程：

#### 先决条件

确保您的系统上安装了 OpenSSL。

#### 证书生成步骤

1. 创建 certs 目录：
   ```bash
   mkdir -p certs
   cd certs
   ```

2. 生成 CA 私钥 (ca.key)：
   ```bash
   openssl genrsa -out ca.key 2048
   ```

3. 生成 CA 根证书 (ca.crt)：
   ```bash
   # 注意：使用双斜杠 //C= 避免 Git Bash 路径解析错误
   openssl req -x509 -new -nodes -key ca.key -sha256 -days 3650 -out ca.crt -subj "/C=US/O=MCPServer++ CA/CN=MCPServer++ Root CA"
   ```

4. 生成服务器私钥 (server.key)：
   ```bash
   openssl genrsa -out server.key 2048
   ```

5. 创建 SAN 配置文件 (san.cnf)：
   ```bash
   cat > san.cnf << EOF
   [req]
   distinguished_name = req_distinguished_name
   req_extensions = v3_req
   prompt = no
   [req_distinguished_name]
   C = US
   O = MCPServer++
   CN = localhost
   [v3_req]
   keyUsage = keyEncipherment, dataEncipherment
   extendedKeyUsage = serverAuth
   subjectAltName = @alt_names
   [alt_names]
   DNS.1 = localhost
   DNS.2 = 127.0.0.1
   IP.1 = 127.0.0.1
   EOF
   ```

6. 生成服务器证书签名请求 (server.csr)：
   ```bash
   openssl req -new -key server.key -out server.csr -config san.cnf
   ```

7. 用 CA 签发服务器证书 (server.crt)：
   ```bash
   openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 365 -sha256 -extfile san.cnf -extensions v3_req
   ```

8. 生成 Diffie-Hellman 参数文件 (dh2048.pem)：
   ```bash
   openssl dhparam -out dh2048.pem 2048
   ```

9. 清理临时文件：
   ```bash
   rm server.csr san.cnf
   ```

10. 验证生成的证书：
    ```bash
    openssl x509 -in ca.crt -text -noout
    openssl x509 -in server.crt -text -noout
    ```

## 证书生成工具

### 概述

[generate_cert](generate_cert) 工具用于轻松创建完整的公钥基础设施 (PKI) 环境，包括：
- 证书颁发机构 (CA) 证书和密钥
- 由 CA 签名的服务器证书和密钥
- 用于密钥交换的 Diffie-Hellman (DH) 参数

该工具生成的证书适用于开发和测试。对于生产环境，应使用受信任的证书颁发机构的证书。

### 使用方法

要生成证书，请先构建项目，然后从构建目录运行该工具：

```bash
# 首先构建项目
mkdir build
cd build
cmake ..
cmake --build .

# 运行证书生成工具
./bin/generate_cert
```

默认情况下，这将创建一个包含所有必要文件的 `certs` 目录：
- `ca.crt` - CA 证书
- `ca.key` - CA 私钥
- `server.crt` - 服务器证书
- `server.key` - 服务器私钥
- `dh2048.pem` - DH 参数

### 命令行选项

| 选项 | 描述 | 默认值 |
|--------|-------------|---------|
| `-h`, `--help` | 显示帮助信息并退出 | |
| `-v`, `--version` | 显示版本信息并退出 | |
| `-d DIR`, `--dir DIR` | 证书输出目录 | `certs` |
| `--install-trust` | 安装 CA 到系统信任存储 (Windows/Linux) | |
| `--dns DNS` | 添加 DNS 主题备用名称 (SAN) | `localhost`, `127.0.0.1` |
| `--ip IP` | 添加 IP 主题备用名称 (SAN) | `127.0.0.1` |
| `-CN NAME`, `--common-name NAME` | 证书通用名称 | `localhost` |

### 示例

1. 使用默认设置生成证书：
   ```bash
   ./bin/generate_cert
   ```

2. 生成证书并安装 CA 到系统信任存储：
   ```bash
   ./bin/generate_cert --install-trust
   ```

3. 为特定域生成证书：
   ```bash
   ./bin/generate_cert --dns myserver.local --ip 192.168.1.100 --common-name myserver.local
   ```

4. 在自定义目录中生成证书：
   ```bash
   ./bin/generate_cert --dir /path/to/certs
   ```

5. 生成具有多个 DNS 名称的证书：
   ```bash
   ./bin/generate_cert --dns localhost --dns myserver.local --dns 127.0.0.1
   ```

## HTTPS 配置

### 启用 HTTPS

HTTPS 在配置文件中默认启用。要禁用它，请在配置中设置 `enable_https=0`。

启用时，服务器将：
1. 加载 SSL 证书和密钥文件
2. 加载 DH 参数文件
3. 在配置的 HTTPS 端口上开始监听（默认：6667）

### 配置参数

`config.ini` 中的以下参数控制 HTTPS 行为：

```ini
[server]
# HTTPS 传输端口（设置为 0 以禁用 HTTPS）
https_port=6667

# 启用 HTTPS 传输（1=启用，0=禁用）
enable_https=1

# SSL 证书文件路径（HTTPS 必需）
ssl_cert_file=certs/server.crt

# SSL 私钥文件路径（HTTPS 必需）
ssl_key_file=certs/server.key

# SSL Diffie-Hellman 参数文件路径（HTTPS 必需）
ssl_dh_params_file=certs/dh2048.pem
```

重要说明：
- 所有文件路径都相对于服务器可执行文件目录
- 证书和密钥文件必须为 PEM 格式
- DH 参数文件对于完美前向保密是必需的
- 如果任何文件丢失或无效，启用 HTTPS 的服务器将无法启动

## 安全注意事项

1. **证书存储**：保持私钥文件安全，并设置适当的权限（例如，仅服务器进程可读）。

2. **证书颁发机构**：在生产环境中，使用受信任 CA 的证书而不是自签名证书。

3. **DH 参数**：提供的 DH 参数适用于开发。在生产环境中，请考虑生成更强的参数。

4. **文件权限**：确保证书文件具有适当的权限以防止未授权访问。

5. **定期轮换**：在证书过期前进行轮换（服务器证书默认有效期为 1 年）。

6. **系统信任存储**：使用 `--install-trust` 时，CA 证书将安装在系统信任存储中，在某些系统上可能需要管理员权限。