# HTTPS and Certificate Generation

MCPServer++ supports secure communication over HTTPS. This document explains how to generate SSL/TLS certificates using the provided certificate generation tool and configure the server for HTTPS.

## Table of Contents

- [HTTPS Support](#https-support)
- [Certificate Generation Methods](#certificate-generation-methods)
  - [Using the Built-in Certificate Generation Tool (Recommended)](#using-the-built-in-certificate-generation-tool-recommended)
  - [Using OpenSSL Command Line Tools](#using-openssl-command-line-tools)
- [Certificate Generation Tool](#certificate-generation-tool)
  - [Overview](#overview)
  - [Usage](#usage)
  - [Command Line Options](#command-line-options)
  - [Examples](#examples)
- [HTTPS Configuration](#https-configuration)
  - [Enabling HTTPS](#enabling-https)
  - [Configuration Parameters](#configuration-parameters)
- [Security Considerations](#security-considerations)

## HTTPS Support

MCPServer++ implements HTTPS transport using OpenSSL, providing secure communication between clients and the server. HTTPS support is enabled by default but requires proper SSL/TLS certificates to function.

The server supports:
- TLS 1.2 and TLS 1.3 protocols
- Strong cipher suites
- Certificate-based authentication
- Perfect Forward Secrecy (PFS) with DH parameters

## Certificate Generation Methods

There are two ways to generate SSL/TLS certificates for MCPServer++:

### Using the Built-in Certificate Generation Tool (Recommended)

The `generate_cert` tool is provided to easily create a full Public Key Infrastructure (PKI) environment, including:
- A Certificate Authority (CA) certificate and key
- A server certificate and key, signed by the CA
- Diffie-Hellman (DH) parameters for key exchange

To generate certificates, build the project and run the tool from the build directory:

```bash
# Build the project first
mkdir build
cd build
cmake ..
cmake --build .

# Run the certificate generation tool
./bin/generate_cert
```

By default, this will create a `certs` directory with all necessary files:
- `ca.crt` - CA certificate
- `ca.key` - CA private key
- `server.crt` - Server certificate
- `server.key` - Server private key
- `dh2048.pem` - DH parameters

### Using OpenSSL Command Line Tools

Alternatively, you can use OpenSSL command line tools to generate certificates. This method provides more control over the certificate generation process:

#### Prerequisites

Make sure you have OpenSSL installed on your system.

#### Certificate Generation Steps

1. Create certs directory:
   ```bash
   mkdir -p certs
   cd certs
   ```

2. Generate CA private key (ca.key):
   ```bash
   openssl genrsa -out ca.key 2048
   ```

3. Generate CA root certificate (ca.crt):
   ```bash
   # Note: Use double slash //C= to avoid Git Bash path parsing errors
   openssl req -x509 -new -nodes -key ca.key -sha256 -days 3650 -out ca.crt -subj "/C=US/O=MCPServer++ CA/CN=MCPServer++ Root CA"
   ```

4. Generate server private key (server.key):
   ```bash
   openssl genrsa -out server.key 2048
   ```

5. Create SAN configuration file (san.cnf):
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

6. Generate server certificate signing request (server.csr):
   ```bash
   openssl req -new -key server.key -out server.csr -config san.cnf
   ```

7. Sign server certificate with CA (server.crt):
   ```bash
   openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 365 -sha256 -extfile san.cnf -extensions v3_req
   ```

8. Generate Diffie-Hellman parameters file (dh2048.pem):
   ```bash
   openssl dhparam -out dh2048.pem 2048
   ```

9. Clean up temporary files:
   ```bash
   rm server.csr san.cnf
   ```

10. Verify generated certificates:
    ```bash
    openssl x509 -in ca.crt -text -noout
    openssl x509 -in server.crt -text -noout
    ```

## Certificate Generation Tool

### Overview

The `generate_cert` tool is provided to easily create a full Public Key Infrastructure (PKI) environment, including:
- A Certificate Authority (CA) certificate and key
- A server certificate and key, signed by the CA
- Diffie-Hellman (DH) parameters for key exchange

The tool generates certificates that are suitable for development and testing. For production environments, you should use certificates from a trusted Certificate Authority.

### Usage

To generate certificates, build the project and run the tool from the build directory:

```bash
# Build the project first
mkdir build
cd build
cmake ..
cmake --build .

# Run the certificate generation tool
./bin/generate_cert
```

By default, this will create a `certs` directory with all necessary files:
- `ca.crt` - CA certificate
- `ca.key` - CA private key
- `server.crt` - Server certificate
- `server.key` - Server private key
- `dh2048.pem` - DH parameters

### Command Line Options

| Option | Description | Default |
|--------|-------------|---------|
| `-h`, `--help` | Show help message and exit | |
| `-v`, `--version` | Show version information and exit | |
| `-d DIR`, `--dir DIR` | Output directory for certificates | `certs` |
| `--install-trust` | Install CA to system trust store (Windows/Linux) | |
| `--dns DNS` | Add DNS Subject Alternative Name (SAN) | `localhost`, `127.0.0.1` |
| `--ip IP` | Add IP Subject Alternative Name (SAN) | `127.0.0.1` |
| `-CN NAME`, `--common-name NAME` | Certificate Common Name | `localhost` |

### Examples

1. Generate certificates with default settings:
   ```bash
   ./bin/generate_cert
   ```

2. Generate certificates and install CA to system trust store:
   ```bash
   ./bin/generate_cert --install-trust
   ```

3. Generate certificates for a specific domain:
   ```bash
   ./bin/generate_cert --dns myserver.local --ip 192.168.1.100 --common-name myserver.local
   ```

4. Generate certificates in a custom directory:
   ```bash
   ./bin/generate_cert --dir /path/to/certs
   ```

5. Generate certificates with multiple DNS names:
   ```bash
   ./bin/generate_cert --dns localhost --dns myserver.local --dns 127.0.0.1
   ```

## HTTPS Configuration

### Enabling HTTPS

HTTPS is enabled by default in the configuration file. To disable it, set `enable_https=0` in the configuration.

When enabled, the server will:
1. Load the SSL certificate and key files
2. Load the DH parameters file
3. Start listening on the configured HTTPS port (default: 6667)

### Configuration Parameters

The following parameters in `config.ini` control HTTPS behavior:

```ini
[server]
# HTTPS transport port (set to 0 to disable HTTPS)
https_port=6667

# Enable HTTPS transport (1=enable, 0=disable)
enable_https=1

# SSL certificate file path (required for HTTPS)
ssl_cert_file=certs/server.crt

# SSL private key file path (required for HTTPS)
ssl_key_file=certs/server.key

# SSL Diffie-Hellman parameters file path (required for HTTPS)
ssl_dh_params_file=certs/dh2048.pem
```

Important notes:
- All file paths are relative to the server executable directory
- The certificate and key files must be in PEM format
- The DH parameters file is required for perfect forward secrecy
- If any file is missing or invalid, the server will fail to start with HTTPS enabled

## Security Considerations

1. **Certificate Storage**: Keep private key files secure and with restricted permissions (e.g., readable only by the server process).

2. **Certificate Authority**: For production use, use certificates from a trusted CA rather than self-signed certificates.

3. **DH Parameters**: The provided DH parameters are suitable for development. For production, consider generating stronger parameters.

4. **File Permissions**: Ensure certificate files have appropriate permissions to prevent unauthorized access.

5. **Regular Rotation**: Rotate certificates before they expire (default is 1 year for server certificates).

6. **System Trust Store**: When using `--install-trust`, the CA certificate is installed in the system trust store, which may require administrator privileges on some systems.