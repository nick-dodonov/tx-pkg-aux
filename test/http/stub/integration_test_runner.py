#!/usr/bin/env python3
"""
Integration test that runs HTTP client and server binaries.
"""

import sys
import subprocess
import os
import time
import signal
import threading
from typing import IO, Any
from datetime import datetime
from urllib.request import urlopen
from urllib.error import URLError


def _log(*args: Any, **kwargs: Any) -> None:
    """Print with immediate flush."""
    timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    print(f"[{timestamp}] runner:", *args, **kwargs, flush=True)


def _log_server_output(pipe: IO[str], prefix: str) -> None:
    """Read from pipe and log each line with timestamp and prefix."""
    for line in iter(pipe.readline, ''):
        if line:
            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            decoded_line = line.rstrip()
            print(f"[{timestamp}] {prefix}: {decoded_line}", flush=True)
    pipe.close()


def wait_for_server(url: str, server_process: subprocess.Popen[str], timeout: int = 10) -> tuple[bool, int | None]:
    """Wait for server to be ready.
    
    Returns:
        tuple: (success, exit_code) - success is True if server is ready,
               exit_code is set if server process terminated
    """
    start_time = time.time()
    while time.time() - start_time < timeout:
        # Check if server process has crashed
        exit_code = server_process.poll()
        if exit_code is not None:
            return False, exit_code
        
        # Try to connect to server
        try:
            with urlopen(url, timeout=1):
                return True, None
        except URLError as e:
            _log(f"Server not ready yet: {e}")
            time.sleep(0.1)
    
    return False, None


def main() -> int:
    """Run integration tests for HTTP client and server."""
    # Get binary paths from command line arguments
    if len(sys.argv) < 3:
        _log("Usage: http_integration_test.py <client_binary> <server_binary>")
        return 1

    client_binary = sys.argv[1]
    server_binary = sys.argv[2]

    # Verify binaries exist
    if not os.path.exists(client_binary):
        _log(f"Error: Client binary not found: {client_binary}")
        return 1

    if not os.path.exists(server_binary):
        _log(f"Error: Server binary not found: {server_binary}")
        return 1

    _log("=" * 30)
    _log("HTTP Integration Test")
    _log("=" * 30)

    server_process: subprocess.Popen[str] | None = None
    server_port = 19090
    server_url = f"http://localhost:{server_port}"

    try:
        # Start HTTP server in background
        _log(f"--- Starting HTTP Server on port {server_port} ---")
        server_process = subprocess.Popen(
            [server_binary, "--port", str(server_port)],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1
        )

        # Start thread to log server output
        log_thread = threading.Thread(
            target=_log_server_output,
            args=(server_process.stdout, "server"),
            daemon=True
        )
        log_thread.start()

        # Wait for server to be ready
        _log("Waiting for server to be ready...")
        success, exit_code = wait_for_server(f"{server_url}/get", server_process, timeout=5)
        if not success:
            if exit_code is not None:
                _log(f"Error: Server process terminated with exit code {exit_code}")
            else:
                _log("Error: Server failed to start within timeout")
            return 1

        _log("Server is ready!")

        # Run client tests
        _log("--- Running HTTP Client Tests ---")
        client_process = subprocess.Popen(
            [client_binary, "--url", server_url],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1
        )

        # Start thread to log client output
        client_log_thread = threading.Thread(
            target=_log_server_output,
            args=(client_process.stdout, "client"),
            daemon=True
        )
        client_log_thread.start()

        # Wait for client to complete
        client_process.wait()
        client_exit_code = client_process.returncode
        if client_exit_code != 0:
            _log(f"Client tests failed with exit code {client_exit_code}")
            return client_exit_code

        _log("=" * 30)
        _log("All tests passed!")
        _log("=" * 30)

        return 0

    except Exception as e:
        _log(f"Error during integration test: {e}")
        return 1

    finally:
        # Shutdown server
        if server_process:
            # Check if server is still running
            if server_process.poll() is None:
                _log("--- Shutting down HTTP Server ---")
                try:
                    # On Windows, subprocess.send_signal(SIGINT) is not supported
                    # Use terminate() for cross-platform compatibility
                    if sys.platform == "win32":
                        server_process.terminate()
                    else:
                        server_process.send_signal(signal.SIGINT)
                    
                    server_process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    _log("Server did not shutdown gracefully, terminating...")
                    server_process.kill()
                    server_process.wait()
                _log("Server stopped.")
            else:
                _log(f"Server already terminated with exit code {server_process.returncode}")


if __name__ == "__main__":
    sys.exit(main())
