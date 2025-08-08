import requests
import json
import time
import threading
from typing import Dict, Optional

class SSEClient:
    def __init__(self, url: str):
        self.url = url
        self.session = requests.Session()
        self.stream = None
        self.running = False
        self.last_event_id = 0  # Record the last received event ID
        self.session_id = ""    # Record session ID

    def _parse_sse_event(self, line: str) -> Optional[Dict]:
        """Parse SSE event format (event: xxx\nid: xxx\ndata: xxx)"""
        event = {}
        lines = [l.strip() for l in line.split('\n') if l.strip()]
        for l in lines:
            if l.startswith('event:'):
                event['event'] = l.split(':', 1)[1].strip()
            elif l.startswith('id:'):
                event['id'] = int(l.split(':', 1)[1].strip())
            elif l.startswith('data:'):
                event['data'] = json.loads(l.split(':', 1)[1].strip())
        return event if lines else None

    def connect(self, is_reconnect: bool = False, last_event_id: int = 0, session_id: str = "") -> None:
        """Establish connection (initial connection or reconnection)"""
        headers = {
            "Content-Type": "application/json",
            "Accept": "text/event-stream",
        }
        # Add previous session information when reconnecting
        if is_reconnect:
            headers["Last-Event-ID"] = str(last_event_id)
            headers["Mcp-Session-Id"] = session_id
            print(f"🔄 Reconnection request - Session-ID: {session_id}, Last-Event-ID: {last_event_id}")
        else:
            print("🚀 Initial connection request...")

        # Tool call parameters (consistent with curl)
        data = {
            "jsonrpc": "2.0",
            "method": "tools/call",
            "params": {"name": "example_stream", "arguments": {}}
        }

        try:
            # Initiate streaming request
            self.running = True
            self.stream = self.session.post(
                self.url,
                headers=headers,
                json=data,
                stream=True,
                timeout=30
            )
            self.stream.raise_for_status()  # Check HTTP errors

            # Process streaming response line by line
            buffer = ""
            for line in self.stream.iter_lines(decode_unicode=True):
                if not self.running:
                    break
                if line:
                    buffer += line + "\n"
                else:
                    # Empty line indicates end of an event
                    event = self._parse_sse_event(buffer)
                    buffer = ""
                    if event:
                        self._handle_event(event, is_reconnect)

        except Exception as e:
            print(f"❌ Connection error: {str(e)}")
        finally:
            self.running = False
            print("🔌 Connection closed")

    def _handle_event(self, event: Dict, is_reconnect: bool) -> None:
        """Handle received SSE events"""
        event_type = event.get('event')
        event_id = event.get('id')
        data = event.get('data')

        # Record session ID (from session_init event)
        if event_type == "session_init":
            self.session_id = data.get('session_id', "")
            print(f"📝 Session initialized - Session-ID: {self.session_id}")
            return

        # Process data messages
        if event_type == "message" and data:
            batch = data.get('result', {}).get('batch', [])
            print(f"📥 Received data - Event-ID: {event_id}, Data: {batch[:3]}...")

            # Update last received ID
            self.last_event_id = event_id

            # Reconnection test: disconnect after receiving 10 data items for the first connection
            if not is_reconnect and self.last_event_id >= 10:
                print("\n⏸️  Disconnect actively (simulate network interruption)")
                self.stop()

    def stop(self) -> None:
        """Stop receiving and close connection"""
        self.running = False
        if self.stream:
            self.stream.close()


def test_reconnect_flow():
    # Server address (modify according to actual situation)
    SERVER_URL = "http://localhost:6666/mcp"
    
    # 1. Initial connection
    client = SSEClient(SERVER_URL)
    connect_thread = threading.Thread(target=client.connect)
    connect_thread.start()
    
    # Wait for initial connection to receive data and disconnect actively
    while client.running:
        time.sleep(1)
    connect_thread.join()

    # Record key information from initial connection
    session_id = client.session_id
    last_event_id = client.last_event_id
    print(f"\n📊 Initial connection info - Session-ID: {session_id}, Last Event-ID: {last_event_id}")

    # 2. Wait 2 seconds before reconnecting
    time.sleep(2)
    print("\n----------------------------------------")
    
    # 3. Reconnect and verify
    reconnect_client = SSEClient(SERVER_URL)
    # Pass previous session information when reconnecting
    reconnect_thread = threading.Thread(
        target=reconnect_client.connect,
        args=(True, last_event_id, session_id)
    )
    reconnect_thread.start()

    # Verify data continuity after reconnection (end after receiving 5 data items)
    retry_count = 0
    while reconnect_client.running and retry_count < 5:
        time.sleep(1)
    
    # Stop reconnection test
    reconnect_client.stop()
    reconnect_thread.join()

    # Output final verification results
    print("\n----------------------------------------")
    print(f"✅ Reconnection test completed!\n"
          f"Initial last Event-ID: {last_event_id}\n"
          f"Reconnection last Event-ID: {reconnect_client.last_event_id}")


if __name__ == "__main__":
    test_reconnect_flow()