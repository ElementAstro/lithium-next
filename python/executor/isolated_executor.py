#!/usr/bin/env python3
"""
Isolated Python Executor for Lithium-Next

This script runs in a subprocess and executes Python code isolated from the main
process. It communicates with the parent process via IPC pipes.

Usage: python isolated_executor.py <read_fd> <write_fd>
"""

import json
import os
import signal
import struct
import sys
import time
import traceback
from dataclasses import dataclass, asdict
from io import StringIO
from typing import Any, Dict, List, Optional, Callable
import contextlib


# IPC Constants
MAGIC = 0x4C495448  # "LITH"
VERSION = 1
HEADER_SIZE = 16


class MessageType:
    """Message types matching C++ enum"""

    HANDSHAKE = 0x01
    HANDSHAKE_ACK = 0x02
    SHUTDOWN = 0x03
    SHUTDOWN_ACK = 0x04
    HEARTBEAT = 0x05
    HEARTBEAT_ACK = 0x06
    EXECUTE = 0x10
    RESULT = 0x11
    ERROR = 0x12
    CANCEL = 0x13
    CANCEL_ACK = 0x14
    PROGRESS = 0x20
    LOG = 0x21
    DATA_CHUNK = 0x30
    DATA_END = 0x31
    DATA_ACK = 0x32
    QUERY = 0x40
    QUERY_RESPONSE = 0x41


@dataclass
class MessageHeader:
    """Message header structure"""

    magic: int = MAGIC
    version: int = VERSION
    msg_type: int = 0
    payload_size: int = 0
    sequence_id: int = 0
    flags: int = 0
    reserved: int = 0

    def serialize(self) -> bytes:
        """Serialize header to bytes"""
        return struct.pack(
            ">IBBIIBB",
            self.magic,
            self.version,
            self.msg_type,
            self.payload_size,
            self.sequence_id,
            self.flags,
            self.reserved,
        )

    @classmethod
    def deserialize(cls, data: bytes) -> "MessageHeader":
        """Deserialize header from bytes"""
        if len(data) < HEADER_SIZE:
            raise ValueError("Insufficient data for header")

        magic, version, msg_type, payload_size, sequence_id, flags, reserved = (
            struct.unpack(">IBBIIBB", data[:HEADER_SIZE])
        )

        if magic != MAGIC:
            raise ValueError(f"Invalid magic: {magic:#x}")

        return cls(
            magic=magic,
            version=version,
            msg_type=msg_type,
            payload_size=payload_size,
            sequence_id=sequence_id,
            flags=flags,
            reserved=reserved,
        )


@dataclass
class Message:
    """IPC Message"""

    header: MessageHeader
    payload: bytes

    @classmethod
    def create(cls, msg_type: int, payload: dict, sequence_id: int = 0) -> "Message":
        """Create a message from JSON payload"""
        payload_bytes = json.dumps(payload).encode("utf-8")
        header = MessageHeader(
            msg_type=msg_type, payload_size=len(payload_bytes), sequence_id=sequence_id
        )
        return cls(header=header, payload=payload_bytes)

    def serialize(self) -> bytes:
        """Serialize the message"""
        return self.header.serialize() + self.payload

    def get_payload_json(self) -> dict:
        """Get payload as JSON"""
        if not self.payload:
            return {}
        return json.loads(self.payload.decode("utf-8"))


class IPCChannel:
    """IPC communication channel"""

    def __init__(self, read_fd: int, write_fd: int):
        self.read_fd = read_fd
        self.write_fd = write_fd
        self.sequence_id = 0

    def send(self, msg: Message) -> None:
        """Send a message"""
        data = msg.serialize()
        total_written = 0
        while total_written < len(data):
            written = os.write(self.write_fd, data[total_written:])
            if written <= 0:
                raise IOError("Write failed")
            total_written += written

    def send_json(self, msg_type: int, payload: dict) -> None:
        """Send a JSON message"""
        self.sequence_id += 1
        msg = Message.create(msg_type, payload, self.sequence_id)
        self.send(msg)

    def receive(self, timeout: float = None) -> Optional[Message]:
        """Receive a message with optional timeout"""
        # Read header
        header_data = self._read_exact(HEADER_SIZE, timeout)
        if header_data is None:
            return None

        header = MessageHeader.deserialize(header_data)

        # Read payload
        if header.payload_size > 0:
            payload = self._read_exact(header.payload_size, timeout)
            if payload is None:
                return None
        else:
            payload = b""

        return Message(header=header, payload=payload)

    def _read_exact(self, size: int, timeout: float = None) -> Optional[bytes]:
        """Read exactly size bytes"""
        import select

        data = b""
        while len(data) < size:
            if timeout is not None:
                ready, _, _ = select.select([self.read_fd], [], [], timeout)
                if not ready:
                    return None

            chunk = os.read(self.read_fd, size - len(data))
            if not chunk:
                return None
            data += chunk

        return data

    def close(self) -> None:
        """Close the channel"""
        try:
            os.close(self.read_fd)
        except:
            pass
        try:
            os.close(self.write_fd)
        except:
            pass


class OutputCapture:
    """Context manager to capture stdout/stderr"""

    def __init__(self):
        self.stdout = StringIO()
        self.stderr = StringIO()
        self._old_stdout = None
        self._old_stderr = None

    def __enter__(self):
        self._old_stdout = sys.stdout
        self._old_stderr = sys.stderr
        sys.stdout = self.stdout
        sys.stderr = self.stderr
        return self

    def __exit__(self, *args):
        sys.stdout = self._old_stdout
        sys.stderr = self._old_stderr

    def get_stdout(self) -> str:
        return self.stdout.getvalue()

    def get_stderr(self) -> str:
        return self.stderr.getvalue()


class IsolatedExecutor:
    """Isolated Python executor"""

    def __init__(self, channel: IPCChannel):
        self.channel = channel
        self.cancelled = False
        self.start_time = None
        self.allowed_imports: List[str] = []
        self.blocked_imports: List[str] = []

        # Setup signal handlers
        signal.signal(signal.SIGTERM, self._handle_signal)
        signal.signal(signal.SIGINT, self._handle_signal)

    def _handle_signal(self, signum, frame):
        """Handle termination signals"""
        self.cancelled = True

    def run(self) -> None:
        """Main execution loop"""
        try:
            # Perform handshake
            self._handle_handshake()

            # Main message loop
            while not self.cancelled:
                msg = self.channel.receive(timeout=1.0)
                if msg is None:
                    continue

                self._handle_message(msg)

        except Exception as e:
            self._send_error(str(e), traceback.format_exc())

        finally:
            self.channel.close()

    def _handle_handshake(self) -> None:
        """Handle initial handshake"""
        # Wait for handshake request
        msg = self.channel.receive(timeout=10.0)
        if msg is None or msg.header.msg_type != MessageType.HANDSHAKE:
            raise RuntimeError("Handshake failed: no request received")

        # Send handshake acknowledgment
        self.channel.send_json(
            MessageType.HANDSHAKE_ACK,
            {
                "version": "1.0",
                "python_version": sys.version,
                "capabilities": ["execute", "progress", "cancel"],
                "pid": os.getpid(),
            },
        )

    def _handle_message(self, msg: Message) -> None:
        """Handle incoming message"""
        msg_type = msg.header.msg_type

        if msg_type == MessageType.EXECUTE:
            self._handle_execute(msg)

        elif msg_type == MessageType.CANCEL:
            self.cancelled = True
            self.channel.send_json(MessageType.CANCEL_ACK, {})

        elif msg_type == MessageType.SHUTDOWN:
            self.channel.send_json(MessageType.SHUTDOWN_ACK, {})
            self.cancelled = True

        elif msg_type == MessageType.HEARTBEAT:
            self.channel.send_json(MessageType.HEARTBEAT_ACK, {})

    def _handle_execute(self, msg: Message) -> None:
        """Handle execute request"""
        self.start_time = time.time()

        payload = msg.get_payload_json()
        script_content = payload.get("script_content", "")
        script_path = payload.get("script_path", "")
        function_name = payload.get("function_name", "")
        arguments = payload.get("arguments", {})
        working_directory = payload.get("working_directory", "")
        self.allowed_imports = payload.get("allowed_imports", [])

        # Change working directory if specified
        if working_directory:
            os.chdir(working_directory)

        result = {
            "success": False,
            "result": None,
            "output": "",
            "error_output": "",
            "exception": "",
            "exception_type": "",
            "traceback": "",
            "execution_time_ms": 0,
            "peak_memory_bytes": 0,
        }

        try:
            with OutputCapture() as capture:
                # Execute the script
                if script_content:
                    exec_result = self._execute_script(script_content, arguments)
                elif function_name:
                    exec_result = self._execute_function(
                        payload.get("module_name", ""), function_name, arguments
                    )
                else:
                    raise ValueError("No script content or function specified")

            result["success"] = True
            result["result"] = self._make_serializable(exec_result)
            result["output"] = capture.get_stdout()
            result["error_output"] = capture.get_stderr()

        except Exception as e:
            result["success"] = False
            result["exception"] = str(e)
            result["exception_type"] = type(e).__name__
            result["traceback"] = traceback.format_exc()

        finally:
            result["execution_time_ms"] = int((time.time() - self.start_time) * 1000)

            # Try to get memory usage
            try:
                import resource

                result["peak_memory_bytes"] = (
                    resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024
                )
            except:
                pass

        self.channel.send_json(MessageType.RESULT, result)

    def _execute_script(self, script_content: str, arguments: dict) -> Any:
        """Execute a script string"""
        # Create namespace with arguments
        namespace = {
            "__name__": "__main__",
            "__args__": arguments,
            "args": arguments,
            "report_progress": self._report_progress,
            "log": self._log,
        }

        # Compile and execute
        code = compile(script_content, "<script>", "exec")
        exec(code, namespace)

        # Return result if defined
        return namespace.get("result", None)

    def _execute_function(
        self, module_name: str, function_name: str, arguments: dict
    ) -> Any:
        """Execute a function from a module"""
        import importlib

        # Import module
        module = importlib.import_module(module_name)

        # Get function
        func = getattr(module, function_name)

        # Call function
        if isinstance(arguments, dict):
            return func(**arguments)
        elif isinstance(arguments, (list, tuple)):
            return func(*arguments)
        else:
            return func(arguments)

    def _report_progress(
        self, percentage: float, message: str = "", current_step: str = ""
    ) -> None:
        """Report progress to parent process"""
        elapsed_ms = (
            int((time.time() - self.start_time) * 1000) if self.start_time else 0
        )

        self.channel.send_json(
            MessageType.PROGRESS,
            {
                "percentage": percentage,
                "message": message,
                "current_step": current_step,
                "elapsed_ms": elapsed_ms,
            },
        )

    def _log(self, level: str, message: str) -> None:
        """Send log message to parent process"""
        self.channel.send_json(MessageType.LOG, {"level": level, "message": message})

    def _send_error(self, message: str, tb: str = "") -> None:
        """Send error message"""
        self.channel.send_json(MessageType.ERROR, {"message": message, "traceback": tb})

    def _make_serializable(self, obj: Any) -> Any:
        """Convert object to JSON-serializable form"""
        if obj is None:
            return None
        if isinstance(obj, (bool, int, float, str)):
            return obj
        if isinstance(obj, (list, tuple)):
            return [self._make_serializable(item) for item in obj]
        if isinstance(obj, dict):
            return {str(k): self._make_serializable(v) for k, v in obj.items()}
        if hasattr(obj, "__dict__"):
            return self._make_serializable(obj.__dict__)

        # Try numpy array
        try:
            import numpy as np

            if isinstance(obj, np.ndarray):
                return obj.tolist()
            if isinstance(obj, (np.integer, np.floating)):
                return obj.item()
        except ImportError:
            pass

        # Try pandas
        try:
            import pandas as pd

            if isinstance(obj, pd.DataFrame):
                return obj.to_dict(orient="records")
            if isinstance(obj, pd.Series):
                return obj.to_list()
        except ImportError:
            pass

        # Fallback to string
        return str(obj)


def main():
    """Main entry point"""
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <read_fd> <write_fd>", file=sys.stderr)
        sys.exit(1)

    try:
        read_fd = int(sys.argv[1])
        write_fd = int(sys.argv[2])
    except ValueError:
        print("Invalid file descriptors", file=sys.stderr)
        sys.exit(1)

    channel = IPCChannel(read_fd, write_fd)
    executor = IsolatedExecutor(channel)
    executor.run()


if __name__ == "__main__":
    main()
