#!/usr/bin/env python3

import json
import math
import os
import queue
import shutil
import socket
import subprocess
import threading
import time
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Callable
from urllib.parse import urlparse


ROOT = Path(__file__).resolve().parents[1]
DEMO_DIR = ROOT / "demo"
HOST = "127.0.0.1"
PORT = 8765
KVSTORE_PORT = 6380


class EventBus:
    def __init__(self) -> None:
        self._subscribers: set[queue.Queue[str]] = set()
        self._lock = threading.Lock()

    def subscribe(self) -> queue.Queue[str]:
        subscriber: queue.Queue[str] = queue.Queue()
        with self._lock:
            self._subscribers.add(subscriber)
        return subscriber

    def unsubscribe(self, subscriber: queue.Queue[str]) -> None:
        with self._lock:
            self._subscribers.discard(subscriber)

    def publish(self, payload: dict) -> None:
        message = f"data: {json.dumps(payload, ensure_ascii=False)}\n\n"
        with self._lock:
            subscribers = list(self._subscribers)
        for subscriber in subscribers:
            subscriber.put(message)


class DemoController:
    def __init__(self) -> None:
        self.bus = EventBus()
        self._lock = threading.Lock()
        self._demo_thread: threading.Thread | None = None
        self._stop_event = threading.Event()
        self._server_proc: subprocess.Popen[str] | None = None
        self._current_mode = "protocol"

    def _publish(self, payload: dict) -> None:
        self.bus.publish(payload)

    def reset(self) -> None:
        with self._lock:
            self._stop_event.set()
            thread = self._demo_thread
            self._demo_thread = None
        if thread is not None and thread.is_alive():
            thread.join(timeout=2.0)
        self._stop_event = threading.Event()
        self._stop_server()
        self._publish({"type": "reset"})
        self._publish({"type": "phase", "text": "空闲"})
        self._publish({
            "type": "metric",
            "qps": 0,
            "p95": 0,
            "active": 0,
            "keys": 0,
            "bars": [0, 0, 0, 0],
            "progress": [],
        })

    def start(self, mode: str) -> None:
        self.reset()
        self._current_mode = mode
        self._publish({"type": "mode", "mode": mode})
        runner = {
            "protocol": self._run_protocol_demo,
            "pressure": self._run_pressure_demo,
            "recovery": self._run_recovery_demo,
        }[mode]
        thread = threading.Thread(target=self._run_guarded, args=(runner,), daemon=True)
        with self._lock:
            self._demo_thread = thread
        thread.start()

    def _run_guarded(self, runner: Callable[[], None]) -> None:
        try:
            runner()
            if not self._stop_event.is_set():
                self._publish({"type": "phase", "text": "完成"})
        except Exception as exc:  # noqa: BLE001
            self._publish({"type": "phase", "text": "错误"})
            self._publish({"type": "log", "client": 0, "kind": "system",
                           "text": f"演示失败: {exc}"})

    def _start_server(self, with_wal: bool) -> None:
        self._stop_server()
        data_dir = ROOT / "data"
        if data_dir.exists():
            shutil.rmtree(data_dir)
        data_dir.mkdir(parents=True, exist_ok=True)

        args = ["./bin/kvstore_server"]
        if not with_wal:
            args.append("--no-wal")
        self._server_proc = subprocess.Popen(
            args,
            cwd=ROOT,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            text=True,
        )
        deadline = time.time() + 5.0
        while time.time() < deadline:
            if self._stop_event.is_set():
                return
            try:
                with socket.create_connection((HOST, KVSTORE_PORT), timeout=0.2):
                    return
            except OSError:
                time.sleep(0.05)
        raise RuntimeError("kvstore_server 未能在 5 秒内启动")

    def _stop_server(self) -> None:
        if self._server_proc is None:
            return
        proc = self._server_proc
        self._server_proc = None
        proc.terminate()
        try:
            proc.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=1.0)

    def _open_client(self) -> tuple[socket.socket, bytearray]:
        client = socket.create_connection((HOST, KVSTORE_PORT), timeout=2.0)
        client.settimeout(2.0)
        return client, bytearray()

    def _read_line(self, sock: socket.socket, buffer: bytearray) -> str:
        while True:
            marker = buffer.find(b"\r\n")
            if marker >= 0:
                line = bytes(buffer[:marker]).decode("utf-8")
                del buffer[:marker + 2]
                return line
            chunk = sock.recv(4096)
            if not chunk:
                raise RuntimeError("服务端提前关闭连接")
            buffer.extend(chunk)

    def _run_command(self,
                     client_index: int,
                     sock: socket.socket,
                     buffer: bytearray,
                     command: str,
                     progress: list[int],
                     total_steps: int,
                     metrics: dict) -> str:
        if self._stop_event.is_set():
            raise RuntimeError("演示被终止")
        self._publish({"type": "log", "client": client_index, "kind": "command",
                       "text": f"> {command}"})
        begin = time.perf_counter()
        sock.sendall((command + "\r\n").encode("utf-8"))
        response = self._read_line(sock, buffer)
        elapsed_us = (time.perf_counter() - begin) * 1_000_000.0
        self._publish({"type": "log", "client": client_index, "kind": "response",
                       "text": f"< {response}"})
        with metrics["lock"]:
            if metrics["first_command_at"] is None:
                metrics["first_command_at"] = begin
            metrics["latencies"].append(elapsed_us)
            metrics["count"] += 1
            metrics["commands"][command.split(" ", 1)[0]] = (
                metrics["commands"].get(command.split(" ", 1)[0], 0) + 1
            )
            progress[client_index] = min(100, progress[client_index] + math.ceil(100 / total_steps))
            self._publish_metric(metrics, progress)
        return response

    def _publish_metric(self, metrics: dict, progress: list[int], keys: int | None = None) -> None:
        elapsed_start = metrics["first_command_at"] if metrics["first_command_at"] is not None else metrics["start"]
        elapsed = max(time.perf_counter() - elapsed_start, 0.001)
        latencies = sorted(metrics["latencies"])
        p95 = 0
        if latencies:
            index = min(len(latencies) - 1, math.ceil(len(latencies) * 0.95) - 1)
            p95 = int(latencies[index])
        commands = metrics["commands"]
        self._publish({
            "type": "metric",
            "qps": metrics["count"] / elapsed,
            "p95": p95,
            "active": sum(1 for value in progress if value < 100 and value > 0) or metrics["active"],
            "keys": metrics["keys"] if keys is None else keys,
            "bars": [
                commands.get("PUT", 0) * 8,
                commands.get("GET", 0) * 8,
                commands.get("SCAN", 0) * 8,
                commands.get("PING", 0) * 6 + commands.get("DEL", 0) * 6,
            ],
            "progress": progress,
        })

    def _run_protocol_demo(self) -> None:
        self._publish({"type": "phase", "text": "协议链路"})
        self._start_server(with_wal=False)
        self._publish({"type": "terminal_count", "count": 6})
        clients = [self._open_client() for _ in range(6)]
        metrics = {"start": time.perf_counter(), "latencies": [], "count": 0,
                   "commands": {}, "keys": 0, "active": 6,
                   "first_command_at": None, "lock": threading.Lock()}
        progress = [0] * 6
        steps = [
            (0, "PING"),
            (1, "PUT user alice"),
            (2, "GET user"),
            (3, "SCAN a z"),
            (4, "DEL user"),
        ]
        self._publish({"type": "log", "client": 0, "kind": "system", "text": "连接 127.0.0.1:6380"})
        for index, command in steps:
            response = self._run_command(index, clients[index][0], clients[index][1], command,
                                         progress, len(steps) + 1, metrics)
            with metrics["lock"]:
                if command.startswith("PUT ") and response.startswith("OK "):
                    metrics["keys"] = 1
                if command.startswith("DEL ") and response == "OK DELETE":
                    metrics["keys"] = 0
                self._publish_metric(metrics, progress)
            time.sleep(0.18)
        self._run_command(5, clients[5][0], clients[5][1], "QUIT", progress, len(steps) + 1, metrics)
        for sock, _ in clients:
            try:
                sock.close()
            except OSError:
                pass

    def _run_pressure_demo(self) -> None:
        client_count = 8
        operations = 24
        self._publish({"type": "phase", "text": "聚合压测"})
        self._start_server(with_wal=False)
        self._publish({"type": "terminal_count", "count": client_count})
        progress = [0] * client_count
        metrics = {"start": time.perf_counter(), "latencies": [], "count": 0,
                   "commands": {}, "keys": 0, "active": client_count,
                   "first_command_at": None, "lock": threading.Lock()}
        key_count = {"value": 0}

        def worker(client_index: int) -> None:
            sock, buffer = self._open_client()
            self._publish({"type": "log", "client": client_index, "kind": "system",
                           "text": f"bench-c{client_index} 就绪 pipeline=1"})
            try:
                for op in range(operations):
                    key = f"bench-c{client_index}-key-{op:02d}"
                    value = f"bench-c{client_index}-value-{op:02d}"
                    if op % 4 == 3:
                        command = f"SCAN bench-c{client_index}-key-00 bench-c{client_index}-key-zz"
                    elif op % 2 == 0:
                        command = f"PUT {key} {value}"
                    else:
                        command = f"GET bench-c{client_index}-key-{op - 1:02d}"
                    response = self._run_command(
                        client_index,
                        sock,
                        buffer,
                        command,
                        progress,
                        operations + 1,
                        metrics,
                    )
                    if command.startswith("PUT ") and response.startswith("OK "):
                        with metrics["lock"]:
                            if command.startswith("PUT ") and response.startswith("OK "):
                                key_count["value"] += 1
                                metrics["keys"] = key_count["value"]
                            self._publish_metric(metrics, progress)
                    time.sleep(0.002 + client_index * 0.0005)
                self._run_command(client_index, sock, buffer, "QUIT", progress,
                                  operations + 1, metrics)
            finally:
                try:
                    sock.close()
                except OSError:
                    pass

        threads = [threading.Thread(target=worker, args=(index,), daemon=True)
                   for index in range(client_count)]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()

    def _run_recovery_demo(self) -> None:
        self._publish({"type": "phase", "text": "持久化证明"})
        self._start_server(with_wal=True)
        self._publish({"type": "terminal_count", "count": 6})
        progress = [0] * 6
        metrics = {"start": time.perf_counter(), "latencies": [], "count": 0,
                   "commands": {}, "keys": 0, "active": 3,
                   "first_command_at": None, "lock": threading.Lock()}
        sock, buffer = self._open_client()
        self._run_command(0, sock, buffer, "PUT thesis demo-record", progress, 4, metrics)
        with metrics["lock"]:
            metrics["keys"] = 1
            self._publish_metric(metrics, progress)
        self._publish({"type": "log", "client": 1, "kind": "system", "text": "服务端停止"})
        self._publish_metric(metrics, progress, keys=1)
        sock.close()
        self._stop_server()
        time.sleep(0.3)
        self._publish({"type": "log", "client": 2, "kind": "system", "text": "重放 data/wal.log"})
        self._start_server(with_wal=True)
        self._publish({"type": "log", "client": 3, "kind": "system", "text": "从日志恢复键空间"})
        sock2, buffer2 = self._open_client()
        self._run_command(4, sock2, buffer2, "GET thesis", progress, 4, metrics)
        self._run_command(5, sock2, buffer2, "QUIT", progress, 4, metrics)
        sock2.close()


controller = DemoController()


class Handler(BaseHTTPRequestHandler):
    def log_message(self, format: str, *args) -> None:  # noqa: A003
        return

    def do_GET(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)
        if parsed.path == "/api/events":
            self._handle_events()
            return
        if parsed.path in ("/", "/defense_dashboard.html"):
            self._serve_file(DEMO_DIR / "defense_dashboard.html", "text/html; charset=utf-8")
            return
        self.send_error(HTTPStatus.NOT_FOUND, "Not Found")

    def do_POST(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)
        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length) if length > 0 else b"{}"
        data = json.loads(raw.decode("utf-8"))
        if parsed.path == "/api/start":
            controller.start(data.get("mode", "protocol"))
            self._json({"ok": True})
            return
        if parsed.path == "/api/reset":
            controller.reset()
            self._json({"ok": True})
            return
        self.send_error(HTTPStatus.NOT_FOUND, "Not Found")

    def _json(self, payload: dict) -> None:
        body = json.dumps(payload).encode("utf-8")
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _serve_file(self, path: Path, content_type: str) -> None:
        body = path.read_bytes()
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _handle_events(self) -> None:
        subscriber = controller.bus.subscribe()
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", "text/event-stream; charset=utf-8")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self.end_headers()
        self.wfile.write(b"data: {\"type\":\"ready\"}\n\n")
        self.wfile.flush()
        try:
            while True:
                message = subscriber.get()
                self.wfile.write(message.encode("utf-8"))
                self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError):
            pass
        finally:
            controller.bus.unsubscribe(subscriber)


def main() -> None:
    server = ThreadingHTTPServer((HOST, PORT), Handler)
    print(f"Defense demo server running at http://{HOST}:{PORT}/defense_dashboard.html")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        controller.reset()
        server.server_close()


if __name__ == "__main__":
    main()
