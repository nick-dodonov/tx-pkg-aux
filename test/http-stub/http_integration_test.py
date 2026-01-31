#!/usr/bin/env python3
"""
Integration test that runs HTTP client and server binaries.
"""

import sys
import subprocess
import os


def run_binary(binary_path: str, args: list[str] | None = None) -> int:
    """Run a binary and return its exit code and output."""
    cmd = [binary_path]
    if args:
        cmd.extend(args)
    
    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True)
    print(f"Exit code: {result.returncode}")
    print(f"Output:\n{result.stdout}")
    if result.stderr:
        print(f"Stderr:\n{result.stderr}")
    
    return result.returncode


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
    
    # Test server
    print("\n--- Testing HTTP Server ---")
    server_exit_code = run_binary(server_binary, ["--port", "9090"])
    if server_exit_code != 0:
        print(f"Server test failed with exit code {server_exit_code}")
        return 1
    
    # Test client
    print("\n--- Testing HTTP Client ---")
    client_exit_code = run_binary(client_binary, ["--url", "http://localhost:9090"])
    if client_exit_code != 0:
        print(f"Client test failed with exit code {client_exit_code}")
        return 1
    
    print("\n" + "=" * 60)
    print("All tests passed!")
    print("=" * 60)
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
