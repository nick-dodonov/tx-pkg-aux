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
        print("Usage: http_integration_test.py <client_binary> <server_binary>")
        return 1

    client_binary = sys.argv[1]
    server_binary = sys.argv[2]

    # Verify binaries exist
    if not os.path.exists(client_binary):
        print(f"Error: Client binary not found: {client_binary}")
        return 1

    if not os.path.exists(server_binary):
        print(f"Error: Server binary not found: {server_binary}")
        return 1

    print("=" * 60)
    print("HTTP Integration Test")
    print("=" * 60)

    server_process = None
    server_port = 9090
    server_url = f"http://localhost:{server_port}"

    try:
        # Start HTTP server in background
        print(f"\n--- Starting HTTP Server on port {server_port} ---")
        server_process = subprocess.Popen(
            [server_binary, "--port", str(server_port)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )

        # Wait for server to be ready
        print("Waiting for server to be ready...")
        if not wait_for_server(f"{server_url}/get", timeout=5):
            print("Error: Server failed to start within timeout")
            if server_process.poll() is not None:
                stdout, stderr = server_process.communicate()
                print(f"Server stdout:\n{stdout}")
                print(f"Server stderr:\n{stderr}")
            return 1

        print("Server is ready!\n")

        # Run client tests
        print("--- Running HTTP Client Tests ---")
        client_result = subprocess.run(
            [client_binary, "--url", server_url], capture_output=True, text=True
        )

        print(f"Client output:\n{client_result.stdout}")
        if client_result.stderr:
            print(f"Client stderr:\n{client_result.stderr}")

        client_exit_code = client_result.returncode

        if client_exit_code != 0:
            print(f"\nClient tests failed with exit code {client_exit_code}")
            return client_exit_code

        print("\n" + "=" * 60)
        print("All tests passed!")
        print("=" * 60)

        return 0

    except Exception as e:
        print(f"Error during integration test: {e}")
        return 1

    finally:
        # Shutdown server
        if server_process:
            print("\n--- Shutting down HTTP Server ---")
            server_process.send_signal(signal.SIGINT)
            try:
                server_process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                print("Server did not shutdown gracefully, terminating...")
                server_process.kill()
                server_process.wait()
            print("Server stopped.")


if __name__ == "__main__":
    sys.exit(main())
