"""
MCP HTTP Server Tester for Python Plugin
"""

import asyncio
import json
import aiohttp
from typing import Dict, Any

class MCPHTTPTester:
    """MCP Server HTTP Tester"""
    
    def __init__(self, server_url: str = "http://127.0.0.1:6666/mcp"):
        self.server_url = server_url
        self.session = None
        self.headers = {
            "Content-Type": "application/json",
            "Connection": "keep-alive"  # Keep connection alive
        }
    
    async def connect(self):
        """Establish HTTP persistent session"""
        self.session = aiohttp.ClientSession(
            connector=aiohttp.TCPConnector(force_close=False),
            headers=self.headers
        )
        print("🚀 HTTP persistent session established")
    
    async def send_request(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Send JSON-RPC request via HTTP"""
        if not self.session:
            raise RuntimeError("Please call connect() to establish session first")
        
        try:
            async with self.session.post(
                self.server_url,
                data=json.dumps(request)
            ) as response:
                if response.status not in (200, 202):
                    raise RuntimeError(f"HTTP request failed: Status code {response.status}")
                
                response_data = await response.text()
                return json.loads(response_data) if response_data else {}
                
        except Exception as e:
            print(f"Request sending failed: {str(e)}")
            raise
    
    async def test_initialize(self):
        """Test initialization request"""
        print("\n🔍 Testing initialization...")
        
        request = {
            "jsonrpc": "2.0",
            "id": 1,
            "method": "initialize",
            "params": {
                "protocolVersion": "2024-11-05",
                "capabilities": {
                    "tools": {"listChanged": True},
                    "resources": {"subscribe": True, "listChanged": True},
                    "prompts": {"listChanged": True}
                },
                "clientInfo": {
                    "name": "MCP Test Client",
                    "version": "1.0.0"
                }
            }
        }
        
        response = await self.send_request(request)
        print(f"Response: {json.dumps(response, indent=2, ensure_ascii=False)}")
        
        assert response.get("jsonrpc") == "2.0", "Invalid JSON-RPC version"
        assert "id" in response and response["id"] == 1, "Response ID mismatch"
        assert "result" in response, "Response missing result field"
        assert "capabilities" in response["result"], "Response missing capabilities field"
        print("√ Initialization test passed")
        return response
    
    async def test_initialized_notification(self):
        print("\n🔍 Testing initialized notification...")
        
        request = {
            "jsonrpc": "2.0",
            "method": "notifications/initialized",
            "params": {}
        }
        
        try:
            async with asyncio.timeout(5):
                async with self.session.post(
                    self.server_url,
                    data=json.dumps(request)
                ) as response:
                    assert response.status == 202, f"Notification status error: {response.status}"
        except asyncio.TimeoutError:
            print("× Notification request timeout")
            raise
        except Exception as e:
            print(f"× Notification sending error: {str(e)}")
            raise
        
        print("√ initialized notification test completed")
        
    async def test_list_tools(self):
        """Test tool list request"""
        print("\n🔍 Testing tool list...")
        
        request = {
            "jsonrpc": "2.0",
            "id": 2,
            "method": "tools/list",
            "params": {}
        }
        
        response = await self.send_request(request)
        print(f"Response: {json.dumps(response, indent=2, ensure_ascii=False)}")
        
        assert response.get("jsonrpc") == "2.0"
        assert "id" in response and response["id"] == 2
        assert "result" in response
        assert "tools" in response["result"], "Response missing tool list"
        print(f"√ Tool list test passed (found {len(response['result']['tools'])} tools)")
        # Return tool list for subsequent tests
        return response["result"]["tools"]
    
    async def test_tool_invoke(self, tool_name: str, params: Dict[str, Any], request_id: int):
        """Test invoking tool"""
        print(f"\n🔍 Testing tool invocation: {tool_name}...")
        
        request = {
            "jsonrpc": "2.0",
            "id": request_id,
            "method": "tools/call",
            "params": {
                "name": tool_name,
                "arguments": params
            }
        }
        
        response = await self.send_request(request)
        print(f"Response: {json.dumps(response, indent=2, ensure_ascii=False)}")
        
        assert response.get("jsonrpc") == "2.0"
        assert "id" in response and response["id"] == request_id
        assert "error" not in response, f"Tool invocation error: {response.get('error', {}).get('message')}"
        print(f"√ {tool_name} tool test passed")
        return response
    
    async def close(self):
        """Close session"""
        if self.session:
            await self.session.close()
            print("🛑 HTTP session closed")
    
    async def run_python_plugin_tests(self):
        """Run Python plugin tests"""
        try:
            await self.connect()
            
            # Test sequence: initialize -> send initialized notification -> test tools
            await self.test_initialize()
            await asyncio.sleep(0.5)  # Give server time to process
            
            await self.test_initialized_notification()
            await asyncio.sleep(0.5)
            
            # Get tool list for subsequent tests
            tools = await self.test_list_tools()
            await asyncio.sleep(0.5)
            
            # Extract available tool names
            available_tools = [tool["name"] for tool in tools]
            print(f"\n📋 Available tools: {available_tools}")
            
            # Test Python plugin tools
            python_test_cases = [
                ("python_echo", {"text": "Hello from Python!"}, 10),
                ("python_calculate", {"expression": "2+2"}, 11),
                ("python_calculate", {"expression": "3*4"}, 12),
                ("python_calculate", {"expression": "10/2"}, 13),
                ("python_calculate", {"expression": "2**3"}, 14),
            ]
            
            # Run tests only for available tools
            for tool_name, params, req_id in python_test_cases:
                if tool_name in available_tools:
                    await self.test_tool_invoke(tool_name, params, req_id)
                    await asyncio.sleep(0.5)
                else:
                    print(f"  Skipping unregistered tool: {tool_name}")
            
            print("\n🎉 All Python plugin tests passed!")
            
        except AssertionError as e:
            print(f"\n× Test assertion failed: {e}")
        except Exception as e:
            print(f"\n× Test failed: {e}")
        finally:
            await self.close()

async def main():
    """Main function"""
    server_url = "http://127.0.0.1:6666/mcp"  # Match your server address
    tester = MCPHTTPTester(server_url)
    await tester.run_python_plugin_tests()

if __name__ == "__main__":
    asyncio.run(main())