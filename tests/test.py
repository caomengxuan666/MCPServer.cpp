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
        self.last_event_id = 0  # 记录最后接收的event ID
        self.last_seq_num = 0   # 记录最后接收的seq_num
        self.session_id = ""    # 记录会话ID

    def _parse_sse_event(self, line: str) -> Optional[Dict]:
        """解析SSE事件格式（event: xxx\nid: xxx\ndata: xxx）"""
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
        """建立连接（首次连接或重连）"""
        headers = {
            "Content-Type": "application/json",
            "Accept": "text/event-stream",
        }
        # 重连时添加上次的会话信息
        if is_reconnect:
            headers["Last-Event-ID"] = str(last_event_id)
            headers["Mcp-Session-Id"] = session_id
            print(f"🔄 重连请求 - Session-ID: {session_id}, Last-Event-ID: {last_event_id}")
        else:
            print("🚀 首次连接请求...")

        # 工具调用参数（和curl一致）
        data = {
            "jsonrpc": "2.0",
            "method": "tools/call",
            "params": {"name": "example_stream", "arguments": {}}
        }

        try:
            # 发起流式请求
            self.running = True
            self.stream = self.session.post(
                self.url,
                headers=headers,
                json=data,
                stream=True,
                timeout=30
            )
            self.stream.raise_for_status()  # 检查HTTP错误

            # 逐行处理流式响应
            buffer = ""
            for line in self.stream.iter_lines(decode_unicode=True):
                if not self.running:
                    break
                if line:
                    buffer += line + "\n"
                else:
                    # 空行表示一个事件结束
                    event = self._parse_sse_event(buffer)
                    buffer = ""
                    if event:
                        self._handle_event(event, is_reconnect)

        except Exception as e:
            print(f"❌ 连接错误: {str(e)}")
        finally:
            self.running = False
            print("🔌 连接已关闭")

    def _handle_event(self, event: Dict, is_reconnect: bool) -> None:
        """处理接收到的SSE事件"""
        event_type = event.get('event')
        event_id = event.get('id')
        data = event.get('data')

        # 记录会话ID（从session_init事件中获取）
        if event_type == "session_init":
            self.session_id = data.get('session_id', "")
            print(f"📝 会话初始化 - Session-ID: {self.session_id}")
            return

        # 处理数据消息
        if event_type == "message" and data:
            seq_num = data.get('result', {}).get('seq_num')
            batch = data.get('result', {}).get('batch', [])
            print(f"📥 接收数据 - Event-ID: {event_id}, Seq-Num: {seq_num}, 数据: {batch[:3]}...")

            # 更新最后接收的ID和序号
            self.last_event_id = event_id
            self.last_seq_num = seq_num

            # 重连测试：首次连接接收10条数据后主动断开
            if not is_reconnect and self.last_event_id >= 10:
                print("\n⏸️ 主动断开连接（模拟网络中断）")
                self.stop()

    def stop(self) -> None:
        """停止接收并关闭连接"""
        self.running = False
        if self.stream:
            self.stream.close()


def test_reconnect_flow():
    # 服务器地址（根据实际情况修改）
    SERVER_URL = "http://localhost:6666/mcp"
    
    # 1. 首次连接
    client = SSEClient(SERVER_URL)
    connect_thread = threading.Thread(target=client.connect)
    connect_thread.start()
    
    # 等待首次连接接收数据并主动断开
    while client.running:
        time.sleep(1)
    connect_thread.join()

    # 记录首次连接的关键信息
    session_id = client.session_id
    last_event_id = client.last_event_id
    last_seq_num = client.last_seq_num
    print(f"\n📊 首次连接信息 - Session-ID: {session_id}, 最后Event-ID: {last_event_id}, 最后Seq-Num: {last_seq_num}")

    # 2. 等待2秒后重连
    time.sleep(2)
    print("\n----------------------------------------")
    
    # 3. 重连并验证
    reconnect_client = SSEClient(SERVER_URL)
    # 重连时传入上次的会话信息
    reconnect_thread = threading.Thread(
        target=reconnect_client.connect,
        args=(True, last_event_id, session_id)
    )
    reconnect_thread.start()

    # 重连后验证数据连续性（等待接收5条数据后结束）
    retry_count = 0
    while reconnect_client.running and retry_count < 5:
        if reconnect_client.last_seq_num > last_seq_num:
            retry_count += 1
            print(f"✅ 重连后验证 - 已接收 {retry_count}/5 条连续数据")
        time.sleep(1)
    
    # 停止重连测试
    reconnect_client.stop()
    reconnect_thread.join()

    # 输出最终验证结果
    print("\n----------------------------------------")
    if reconnect_client.last_seq_num > last_seq_num:
        print(f"🎉 断点续传测试成功！\n"
              f"首次最后Seq-Num: {last_seq_num}\n"
              f"重连后最后Seq-Num: {reconnect_client.last_seq_num}")
    else:
        print(f"❌ 断点续传测试失败！\n"
              f"首次最后Seq-Num: {last_seq_num}\n"
              f"重连后最后Seq-Num: {reconnect_client.last_seq_num}")


if __name__ == "__main__":
    test_reconnect_flow()