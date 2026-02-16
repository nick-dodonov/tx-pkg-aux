#!/usr/bin/env python3
"""
Integration test that runs HTTP client and server binaries.
"""

import argparse
import os
import signal
import subprocess
import sys
import threading
import time
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any, IO
from urllib.error import URLError
from urllib.request import urlopen


if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, OSError):
        pass


@dataclass
class Options:
    """Configuration for integration test execution."""

    server_binary: Path
    client_binary: Path
    host: str = "localhost"
    port: int = 8080
    timeout: int = 30
    client_args: list[str] | None = None

    @property
    def server_url(self) -> str:
        return f"http://{self.host}:{self.port}"


def _log(*args: Any, **kwargs: Any) -> None:
    """Print with timestamp and immediate flush."""
    timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    print(f"[{timestamp}] [INT]", *args, **kwargs, flush=True)


def _log_process_output(pipe: IO[str], prefix: str) -> None:
    """Read from pipe and log each line with timestamp."""
    for line in iter(pipe.readline, ""):
        if line:
            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            print(f"[{timestamp}] {prefix}{line.rstrip()}", flush=True)
    pipe.close()


def _start_logged_process(command: list[str], log_prefix: str) -> subprocess.Popen[str]:
    """Start a subprocess with automatic output logging."""
    command_str = subprocess.list2cmdline(command)
    _log(f"--- STARTING {log_prefix}{command_str}")

    executable = None # Use default shell (%ComSpec%/cmd on Windows)
    if sys.platform != "win32":
        executable = 'bash' # TODO: make sh_wrapper.cmd working w/ `sh`

    process = subprocess.Popen(
        command_str,
        executable=executable,
        shell=True,

        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        bufsize=1,
    )

    log_thread = threading.Thread(
        target=_log_process_output,
        args=(process.stdout, log_prefix),
        daemon=True,
    )
    log_thread.start()

    return process


def _shutdown_process(process: subprocess.Popen[str] | None, name: str) -> None:
    """Gracefully shutdown a process."""
    if not process:
        return

    if process.poll() is not None:
        _log(f"{name} already terminated with exit code {process.returncode}")
        return

    _log(f"--- Shutting down {name} ---")
    try:
        if sys.platform == "win32":
            process.terminate()
        else:
            process.send_signal(signal.SIGINT)
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        _log(f"{name} did not shutdown gracefully, terminating...")
        process.kill()
        process.wait()

    _log(f"{name} stopped.")


def _wait_for_server(
    url: str,
    server_process: subprocess.Popen[str],
    timeout: int,
) -> bool:
    """Wait for server to become ready."""
    start_time = time.time()
    while time.time() - start_time < timeout:
        if server_process.poll() is not None:
            _log(f"Error: Server process terminated with exit code {server_process.returncode}")
            return False

        try:
            with urlopen(url, timeout=1):
                return True
        except URLError as e:
            _log(f"Server not ready yet: {e}")
            time.sleep(0.5)

    _log("Error: Server failed to start within timeout")
    return False


def parse_args() -> Options:
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(
        description="HTTP integration test bridge",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("server_binary", help="Server binary path")
    parser.add_argument("client_binary", help="Client test binary path")
    parser.add_argument(
        "--host",
        default="localhost",
        help="Server host",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=8080,
        help="Server port",
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=30,
        help="Server startup timeout in seconds",
    )

    args, unknown = parser.parse_known_args()

    return Options(
        server_binary=Path(args.server_binary),
        client_binary=Path(args.client_binary),
        host=args.host,
        port=args.port,
        timeout=args.timeout,
        client_args=unknown if unknown else None,
    )


def main() -> int:
    """Run integration tests for HTTP client and server."""
    options = parse_args()

    if not options.server_binary.exists():
        _log(f"Error: Server binary not found: {options.server_binary}")
        return 1

    if not options.client_binary.exists():
        _log(f"Error: Client binary not found: {options.client_binary}")
        return 1

    _log("=" * 30)
    _log("HTTP Integration Test")
    _log("=" * 30)

    server_process: subprocess.Popen[str] | None = None
    try:
        server_process = _start_logged_process(
            [str(options.server_binary)], "[srv] "
        )

        _log("Waiting for server to be ready...")
        if not _wait_for_server(
            f"{options.server_url}/health",
            server_process,
            options.timeout,
        ):
            return 1
        _log("Server is ready!")

        client_cmd = [str(options.client_binary)]
        if options.client_args:
            client_cmd.extend(options.client_args)
        client_process = _start_logged_process(client_cmd, "[cli] ")

        client_process.wait()
        if client_process.returncode != 0:
            _log(f"Client tests failed with exit code {client_process.returncode}")
            return client_process.returncode

        _log("=" * 30)
        _log("All tests passed!")
        _log("=" * 30)
        return 0

    except Exception as e:
        _log(f"Error during integration test: {e}")
        return 1

    finally:
        _shutdown_process(server_process, "HTTP Server")


if __name__ == "__main__":
    sys.exit(main())
