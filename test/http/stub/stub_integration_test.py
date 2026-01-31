#!/usr/bin/env python3
"""
Integration test that runs HTTP client and server binaries.
"""

import sys
import subprocess
import os
import time
import signal
from urllib.request import urlopen
from urllib.error import URLError


def _log(*args, **kwargs) -> None:
    """Print with immediate flush."""
    print("test_runner:", *args, **kwargs, flush=True)


def wait_for_server(url: str, timeout: int = 10) -> bool:
    """Wait for server to be ready."""
    start_time = time.time()
    while time.time() - start_time < timeout:
        try:
            with urlopen(url, timeout=1):
                return True
        except URLError:
            time.sleep(0.1)
    return False


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

    server_process = None
    server_port = 19090
    server_url = f"http://localhost:{server_port}"

    try:
        # Start HTTP server in background
        _log(f"--- Starting HTTP Server on port {server_port} ---")
        server_process = subprocess.Popen(
            [server_binary, "--port", str(server_port)]
        )

        # Wait for server to be ready
        _log("Waiting for server to be ready...")
        if not wait_for_server(f"{server_url}/get", timeout=5):
            _log("Error: Server failed to start within timeout")
            return 1

        _log("Server is ready!")

        # Run client tests
        _log("--- Running HTTP Client Tests ---")
        client_result = subprocess.run([client_binary, "--url", server_url])

        client_exit_code = client_result.returncode
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


if __name__ == "__main__":
    sys.exit(main())
