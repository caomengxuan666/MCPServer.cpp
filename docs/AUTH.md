# Authentication

MCPServer++ supports authentication mechanisms to secure your server from unauthorized access. This document explains how to configure and use authentication features.

## Authentication Types

MCPServer++ currently supports two types of authentication:

1. **X-API-Key**: Header-based authentication using the `X-API-Key` header
2. **Bearer Token**: Header-based authentication using the `Authorization: Bearer` header

## Configuration

Authentication is configured through environment variables. Create a file named `.env.auth` in your server root directory with your keys/tokens, one per line.

Example `.env.auth` file:
```
# This is an example auth file for MCP Server
# Each line should contain a single API key or token
# Lines starting with # are treated as comments and ignored
# Empty lines are also ignored

# Example X-API-Key keys:
sk-1234567890abcdef
sk-0987654321fedcba

# Example Bearer tokens:
eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyfQ.SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c
```

## Usage

### X-API-Key Authentication

To authenticate using X-API-Key, include the following header in your requests:
```
X-API-Key: sk-1234567890abcdef
```

### Bearer Token Authentication

To authenticate using Bearer tokens, include the following header in your requests:
```
Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ...
```

## Client Implementation Examples

### Python Example

```python
import requests

# Using X-API-Key
headers = {
    "X-API-Key": "sk-1234567890abcdef"
}
response = requests.post("http://localhost:6666", headers=headers, json=payload)

# Using Bearer Token
headers = {
    "Authorization": "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ..."
}
response = requests.post("http://localhost:6666", headers=headers, json=payload)
```

### JavaScript/Node.js Example

```javascript
// Using fetch API
const headers = {
    "X-API-Key": "sk-1234567890abcdef"
};

fetch('http://localhost:6666', {
    method: 'POST',
    headers: {
        ...headers,
        'Content-Type': 'application/json'
    },
    body: JSON.stringify(payload)
});

// Using Bearer Token
const headers = {
    "Authorization": "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ..."
};

fetch('http://localhost:6666', {
    method: 'POST',
    headers: {
        ...headers,
        'Content-Type': 'application/json'
    },
    body: JSON.stringify(payload)
});
```

## Security Considerations

1. **HTTPS**: Always use HTTPS in production to encrypt authentication credentials
2. **Key Storage**: Store keys securely and rotate them regularly
3. **Access Control**: Limit access to the `.env.auth` file
4. **Logging**: Avoid logging authentication credentials

## Troubleshooting

If authentication is not working, check the following:

1. Ensure the `.env.auth` file exists in the server root directory
2. Verify that keys/tokens in the file are correctly formatted
3. Check that your client is sending the correct headers
4. Review server logs for authentication-related error messages