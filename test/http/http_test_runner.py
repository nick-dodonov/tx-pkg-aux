#!/usr/bin/env python3
"""
Wrapper script to run HTTP client tests with stub server.
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
    """Run HTTP client test with stub server."""
    if len(sys.argv) < 3:
        print("Usage: http_test_runner.py <test_binary> <server_binary>")
        return 1
    
    test_binary = sys.argv[1]
    server_binary = sys.argv[2]
    
    if not os.path.exists(test_binary):
        print(f"Error: Test binary not found: {test_binary}")
        return 1
    
    if not os.path.exists(server_binary):
        print(f"Error: Server binary not found: {server_binary}")
        return 1
    
    server_process = None
    server_port = 9090
    server_url = f"http://localhost:{server_port}"
    
    try:
        # Start HTTP server
        print(f"Starting HTTP server on port {server_port}...")
        server_process = subprocess.Popen(
            [server_binary, "--port", str(server_port)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        
        # Wait for server to be ready
        if not wait_for_server(f"{server_url}/get", timeout=5):
            print("Error: Server failed to start within timeout")
            return 1
        
        print("Server is ready!")
        
        # Run test binary
        print(f"Running test: {test_binary}")
        test_result = subprocess.run(
            [test_binary],
            capture_output=True,
            text=True
        )
        
        print(test_result.stdout)
        if test_result.stderr:
            print(test_result.stderr, file=sys.stderr)
        
        return test_result.returncode
        
    finally:
        # Shutdown server
        if server_process:
            print("Shutting down HTTP server...")
            server_process.send_signal(signal.SIGINT)
            try:
                server_process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                server_process.kill()
                server_process.wait()


if __name__ == "__main__":
    sys.exit(main())
