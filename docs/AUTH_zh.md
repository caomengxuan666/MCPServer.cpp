# 认证系统

MCPServer++ 支持认证机制，以保护您的服务器免受未授权访问。本文档解释了如何配置和使用认证功能。

## 认证类型

MCPServer++ 目前支持两种类型的认证：

1. **X-API-Key**：使用 `X-API-Key` 头进行基于头部的认证
2. **Bearer Token**：使用 `Authorization: Bearer` 头进行基于头部的认证

## 配置

认证通过环境变量进行配置。在服务器根目录下创建一个名为 `.env.auth` 的文件，每行放置一个密钥/令牌。

示例 `.env.auth` 文件：
```
# 这是 MCP 服务器的示例认证文件
# 每行应包含一个单独的 API 密钥或令牌
# 以 # 开头的行被视为注释并被忽略
# 空行也会被忽略

# X-API-Key 密钥示例：
sk-1234567890abcdef
sk-0987654321fedcba

# Bearer 令牌示例：
eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyfQ.SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c
```

## 使用方法

### X-API-Key 认证

要使用 X-API-Key 进行认证，请在请求中包含以下头部：
```
X-API-Key: sk-1234567890abcdef
```

### Bearer Token 认证

要使用 Bearer 令牌进行认证，请在请求中包含以下头部：
```
Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ...
```

## 客户端实现示例

### Python 示例

```python
import requests

# 使用 X-API-Key
headers = {
    "X-API-Key": "sk-1234567890abcdef"
}
response = requests.post("http://localhost:6666", headers=headers, json=payload)

# 使用 Bearer Token
headers = {
    "Authorization": "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ..."
}
response = requests.post("http://localhost:6666", headers=headers, json=payload)
```

### JavaScript/Node.js 示例

```javascript
// 使用 fetch API
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

// 使用 Bearer Token
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

## 安全考虑

1. **HTTPS**：在生产环境中始终使用 HTTPS 来加密认证凭证
2. **密钥存储**：安全存储密钥并定期轮换
3. **访问控制**：限制对 `.env.auth` 文件的访问
4. **日志记录**：避免记录认证凭证

## 故障排除

如果认证不工作，请检查以下内容：

1. 确保服务器根目录下存在 `.env.auth` 文件
2. 验证文件中的密钥/令牌格式是否正确
3. 检查客户端是否发送了正确的头部
4. 查看服务器日志中与认证相关的错误信息