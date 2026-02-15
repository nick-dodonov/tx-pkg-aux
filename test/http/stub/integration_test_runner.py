#!/usr/bin/env python3
"""
Integration test that runs HTTP client and server binaries.
"""

import sys
import subprocess
import os
import time
import threading
from pathlib import Path
from typing import IO, Any
from datetime import datetime
from urllib.request import urlopen
from urllib.error import URLError


# Fix encoding for stdout/stderr on Windows
if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
        sys.stderr.reconfigure(encoding='utf-8', errors='replace')
    except (AttributeError, OSError):
        pass


server_host = "localhost"
server_port = 8080
# noinspection HttpUrlsUsage
server_url = f"http://{server_host}:{server_port}"


def _log(*args: Any, **kwargs: Any) -> None:
    """Print with immediate flush."""
    timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    print(f"[{timestamp}] [INT]", *args, **kwargs, flush=True)


def _log_header(message: str) -> None:
    """Log a message as a header with separator lines."""
    _log("=" * 30)
    _log(message)
    _log("=" * 30)


def _log_process_output(pipe: IO[str], prefix: str) -> None:
    """Read from pipe and log each line with timestamp and prefix."""
    for line in iter(pipe.readline, ""):
        if line:
            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            decoded_line = line.rstrip()
            print(f"[{timestamp}] {prefix}{decoded_line}", flush=True)
    pipe.close()


def _start_logged_process(command: list[str], log_prefix: str) -> subprocess.Popen[str]:
    """Start a subprocess with automatic output logging.

    Args:
        command: Command and arguments to execute
        log_prefix: Prefix for log messages from this process

    Returns:
        The started subprocess
    """

    # Convert list to string for shell=True to properly pass arguments
    command_str = subprocess.list2cmdline(command)
    _log(f"--- STARTING {log_prefix}{command_str}")

    if sys.platform != "win32":
        executable = 'bash' # TODO: make sh_wrapper.cmd working w/ `sh`
    else:
        executable = None # Use default shell (%ComSpec% cmd.exe) on Windows

    process = subprocess.Popen(
        command_str,
        executable=executable,
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding='utf-8',
        errors='replace',
        bufsize=1,
    )

    # Start a thread to log process output
    log_thread = threading.Thread(
        target=_log_process_output,
        args=(process.stdout, log_prefix),
        daemon=True
    )
    log_thread.start()

    return process


def _shutdown_process(process: subprocess.Popen[str] | None, name: str) -> None:
    """Gracefully shutdown a process.

    Args:
        process: Process to shutdown (can be None)
        name: Name for logging purposes
    """
    if not process:
        return

    # Check if the process already terminated
    exit_code = process.poll()
    if exit_code is not None:
        _log(f"{name} already terminated with exit code {exit_code}")
        return

    # Attempt a graceful shutdown
    _log(f"--- Shutting down {name} ---")
    try:
        # On Windows, subprocess.send_signal(SIGINT) is not supported
        # Use terminate() for cross-platform compatibility
        if sys.platform == "win32":
            process.terminate()
        else:
            import signal
            process.send_signal(signal.SIGINT)

        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        _log(f"{name} did not shutdown gracefully, terminating...")
        process.kill()
        process.wait()

    _log(f"{name} stopped.")


def _wait_for_server(
        url: str, server_process: subprocess.Popen[str], timeout: int = 10
) -> bool:
    """Wait for server to be ready.

    Returns:
        True if server is ready, False otherwise
    """
    start_time = time.time()
    while time.time() - start_time < timeout:
        # Check if the server process has crashed
        exit_code = server_process.poll()
        if exit_code is not None:
            _log(f"Error: Server process terminated with exit code {exit_code}")
            return False

        # Try to connect to the server
        try:
            with urlopen(url, timeout=1):
                return True
        except URLError as e:
            _log(f"Server not ready yet: {e}")
            time.sleep(0.5)

    _log("Error: Server failed to start within timeout")
    return False


def main() -> int:
    """Run integration tests for HTTP client and server."""
    # Get binary paths from command line arguments
    if len(sys.argv) < 3:
        _log("Usage: http_integration_test.py <client_binary> <server_binary>")
        return 1

    client_binary = sys.argv[1]
    server_binary = sys.argv[2]

    # Normalize paths to platform-specific format (convert / to \\ on Windows)
    client_binary = str(Path(client_binary))
    server_binary = str(Path(server_binary))

    # Verify binaries exist
    if not os.path.exists(client_binary):
        _log(f"Error: Client binary not found: {client_binary}")
        return 1

    if not os.path.exists(server_binary):
        _log(f"Error: Server binary not found: {server_binary}")
        return 1

    _log_header("HTTP Integration Test")

    server_process: subprocess.Popen[str] | None = None
    try:
        # Start HTTP server in background
        server_process = _start_logged_process(
            #[server_binary, "--host", server_host, "--port", str(server_port), "--ipv6"],
            [server_binary],
            "[srv] "
        )

        # Wait for the server to be ready
        _log("Waiting for server to be ready...")
        if not _wait_for_server(f"{server_url}/get", server_process, timeout=30):
            return 1
        _log("Server is ready!")

        # Run client tests
        client_process = _start_logged_process(
            #[client_binary, "--url", server_url], 
            [client_binary], 
            "[cli] "
        )

        # Wait for the client to complete
        client_process.wait()
        client_exit_code = client_process.returncode
        if client_exit_code != 0:
            _log(f"Client tests failed with exit code {client_exit_code}")
            return client_exit_code

        _log_header("All tests passed!")
        return 0

    except Exception as e:
        _log(f"Error during integration test: {e}")
        return 1

    finally:
        _shutdown_process(server_process, "HTTP Server")


if __name__ == "__main__":
    sys.exit(main())
