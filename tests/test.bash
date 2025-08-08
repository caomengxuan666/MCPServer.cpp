#!/bin/bash

# Server basic configuration
BASE_URL="http://localhost:6666/mcp"
CONTENT_TYPE="application/json"

# Color output functions
info() { echo -e "\033[1;34m[INFO]\033[0m $1"; }
success() { echo -e "\033[1;32m[SUCCESS]\033[0m $1"; }
error() { echo -e "\033[1;31m[ERROR]\033[0m $1"; }

# Test function: execute curl request and display results
test_tool() {
    local tool_name=$1
    local request_body=$2
    local is_stream=$3  # 1 for streaming response, 0 for normal response

    info "Testing tool: $tool_name"
    
    # Build curl command
    if [ $is_stream -eq 1 ]; then
        # Streaming response needs Accept: text/event-stream header
        curl -s -X POST "$BASE_URL" \
            -H "Content-Type: $CONTENT_TYPE" \
            -H "Accept: text/event-stream" \
            -d "$request_body"
    else
        # Normal response
        curl -s -X POST "$BASE_URL" \
            -H "Content-Type: $CONTENT_TYPE" \
            -d "$request_body"
    fi

    # Check curl execution result
    if [ $? -eq 0 ]; then
        success "Tool $tool_name test completed\n"
    else
        error "Tool $tool_name test failed\n"
    fi
}

# 1. Test get current time (get_current_time)
test_tool "get_current_time" '{
    "jsonrpc": "2.0",
    "id": "test_time_1",
    "method": "tools/call",
    "params": {
        "name": "get_current_time",
        "arguments": {}
    }
}' 0

# 2. Test get system info (get_system_info)
test_tool "get_system_info" '{
    "jsonrpc": "2.0",
    "id": "test_system_1",
    "method": "tools/call",
    "params": {
        "name": "get_system_info",
        "arguments": {}
    }
}' 0

# 3. Test list files (list_files) - test current directory
test_tool "list_files (current directory)" '{
    "jsonrpc": "2.0",
    "id": "test_files_1",
    "method": "tools/call",
    "params": {
        "name": "list_files",
        "arguments": {
            "path": "."
        }
    }
}' 0

# 4. Test ping host (ping_host) - test google DNS
test_tool "ping_host (8.8.8.8)" '{
    "jsonrpc": "2.0",
    "id": "test_ping_1",
    "method": "tools/call",
    "params": {
        "name": "ping_host",
        "arguments": {
            "host": "8.8.8.8"
        }
    }
}' 0

# 5. Test network connectivity (check_connectivity)
test_tool "check_connectivity" '{
    "jsonrpc": "2.0",
    "id": "test_conn_1",
    "method": "tools/call",
    "params": {
        "name": "check_connectivity",
        "arguments": {}
    }
}' 0

# 6. Test get public IP (get_public_ip)
test_tool "get_public_ip" '{
    "jsonrpc": "2.0",
    "id": "test_ip_1",
    "method": "tools/call",
    "params": {
        "name": "get_public_ip",
        "arguments": {}
    }
}' 0

# 7. Test stream log file (stream_log_file) - need to replace with actual existing log file path
# Note: This test will continuously output log content, press Ctrl+C to stop
test_tool "stream_log_file" '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
        "name": "stream_log_file",
        "arguments": {
            "path": "./server.log"
        }
    }
}' 1

echo -e "\033[1;33mAll tests completed\033[0m"