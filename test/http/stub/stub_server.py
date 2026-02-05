#!/usr/bin/env python3
"""
HTTP server for testing HTTP package functionality.
"""

import sys
import argparse
import json
import time
import socket
import threading
from http.server import HTTPServer, BaseHTTPRequestHandler
from typing import Any
from urllib.request import urlopen
from urllib.error import URLError


class IPv6HTTPServer(HTTPServer):
    """HTTPServer that supports IPv6."""
    address_family = socket.AF_INET6
    allow_reuse_address = True  # Allow socket reuse

    def server_bind(self) -> None:
        """Bind server with dual-stack support if requested."""
        # Allow dual-stack (IPv4 and IPv6) by default on IPv6 sockets
        if hasattr(socket, 'IPV6_V6ONLY'):
            self.socket.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
        super().server_bind()


def _log(*args: Any, **kwargs: Any) -> None:
    """Print with immediate flush."""
    print(*args, **kwargs, flush=True)


def _host_diagnostic(host: str, ipv6: bool = False) -> None:
    """Log host-related diagnostics."""
    _log(f"[DIAG] Platform: {sys.platform}")
    _log(f"[DIAG] Hostname: {socket.gethostname()}")
    try:
        # Use getaddrinfo for IPv6 compatibility
        family = socket.AF_INET6 if ipv6 else socket.AF_INET
        ai_result = socket.getaddrinfo(host, None, family, socket.SOCK_STREAM)
        if ai_result:
            resolved_ips = [str(ai[4][0]) for ai in ai_result]
            _log(f"[DIAG] Target host resolves to: {', '.join(resolved_ips)}")
    except Exception as e:
        _log(f"[DIAG] Target host resolve failed: {e}")


def _bind_diagnostic(host: str, port: int, ipv6: bool = False) -> None:
    """Check if the specified port can be bound."""
    addr_family = socket.AF_INET6 if ipv6 else socket.AF_INET
    test_sock = socket.socket(addr_family, socket.SOCK_STREAM)
    test_sock.settimeout(1)
    try:
        # Allow reuse of address to avoid TIME_WAIT issues
        test_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        # Enable dual-stack for IPv6 if supported
        if ipv6 and hasattr(socket, 'IPV6_V6ONLY'):
            test_sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
        test_sock.bind((host, port))
        _log(f"[DIAG] Port {port} bind check PASSED")
    except OSError as e:
        _log(f"[DIAG] Port {port} bind check: {e} (may be normal if port is intended for server)")
    finally:
        test_sock.close()


def _self_diagnostic(host: str, port: int, ipv6: bool = False) -> None:
    """Run self-diagnostic in a separate thread after server starts."""
    time.sleep(0.2)  # Give server time to start listening

    # Wrap IPv6 addresses in brackets for URLs
    url_host = f"[{host}]" if ":" in host else host
    health_url = f"http://{url_host}:{port}/health"
    _log(f"[DIAG] Running self-diagnostic at {health_url}")

    try:
        # Check if port is listening using socket with appropriate address family
        addr_family = socket.AF_INET6 if ipv6 else socket.AF_INET
        sock = socket.socket(addr_family, socket.SOCK_STREAM)
        sock.settimeout(1)
        result = sock.connect_ex((host, port))
        sock.close()
        
        if result == 0:
            _log(f"[DIAG] Port {port} is listening")
        else:
            _log(f"[DIAG] Port {port} is NOT listening (error code: {result})")
            return

        # Try HTTP request
        with urlopen(health_url, timeout=2) as response:
            health_data = json.loads(response.read().decode('utf-8'))
            _log(f"[DIAG] Self-diagnostic PASSED - {health_data}")
    except URLError as e:
        _log(f"[DIAG] Self-diagnostic FAILED (URLError) - {e}")
    except Exception as e:
        _log(f"[DIAG] Self-diagnostic FAILED (Exception) - {e}")


class HTTPTestHandler(BaseHTTPRequestHandler):
    """HTTP request handler for GET and POST requests."""

    def _send_cors_headers(self) -> None:
        """Send CORS headers to allow cross-origin requests."""
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, Authorization")
        self.send_header("Access-Control-Max-Age", "86400")

    def _send_error_with_cors(self, code: int, message: str) -> None:
        """Send error response with CORS headers."""
        error_response = {
            "error": message,
            "code": code,
        }
        self.send_response(code)
        self._send_cors_headers()
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(json.dumps(error_response).encode())

    def do_OPTIONS(self) -> None:
        """Handle OPTIONS preflight requests for CORS."""
        _log(f"Handled OPTIONS request: {self.path}")
        self.send_response(200)
        self._send_cors_headers()
        self.end_headers()

    def do_GET(self) -> None:
        """Handle GET requests."""
        _log(f"Handled GET request: {self.path}")
        if self.path == "/health":
            response = {
                "status": "healthy",
                "message": "Server is running",
            }
            self.send_response(200)
            self._send_cors_headers()
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps(response).encode())
        elif self.path == "/get":
            response = {
                "method": "GET",
                "path": self.path,
                "message": "GET request successful",
            }
            self.send_response(200)
            self._send_cors_headers()
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps(response).encode())
        else:
            self._send_error_with_cors(404, "Not Found")

    def do_POST(self) -> None:
        """Handle POST requests."""
        _log(f"Handled POST request: {self.path}")
        if self.path == "/post":
            content_length = int(self.headers.get("Content-Length", 0))
            post_data = self.rfile.read(content_length).decode("utf-8")

            try:
                request_body: dict[str, Any] = (
                    json.loads(post_data) if post_data else {}
                )
            except json.JSONDecodeError:
                request_body = {"raw": post_data}

            response: dict[str, Any] = {
                "method": "POST",
                "path": self.path,
                "message": "POST request successful",
                "received_data": request_body,
            }
            self.send_response(200)
            self._send_cors_headers()
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps(response).encode())
        else:
            self._send_error_with_cors(404, "Not Found")


def main() -> int:
    """Start HTTP server."""
    parser = argparse.ArgumentParser(description="HTTP server for testing")
    parser.add_argument("--port", type=int, default=8080, help="Port to listen on")
    parser.add_argument("--host", default=None, help="Host to bind to (default: localhost for IPv4, :: for IPv6 dual-stack)")
    parser.add_argument("--ipv6", action="store_true", help="Enable IPv6 support with dual-stack (accepts both IPv4 and IPv6)")

    args = parser.parse_args()

    # Set default host based on IPv6 mode
    if args.host is None:
        args.host = "::" if args.ipv6 else "localhost"

    try:
        if args.ipv6:
            _log(f"Server starting on {args.host}:{args.port} (IPv6 dual-stack - accepts IPv4 and IPv6)")
        else:
            _log(f"Server starting on {args.host}:{args.port} (IPv4)")

        _host_diagnostic(args.host, args.ipv6)
        _bind_diagnostic(args.host, args.port, args.ipv6)

        server_address = (args.host, args.port)
        server_class = IPv6HTTPServer if args.ipv6 else HTTPServer
        httpd = server_class(server_address, HTTPTestHandler)

        _log("Ready to accept connections")
        _log(f"[DIAG] Server socket: {httpd.socket.getsockname()}")
        family_str = "AF_INET6 (dual-stack)" if args.ipv6 else "AF_INET"
        _log(f"[DIAG] Address family: {family_str}")
        
        # Check dual-stack status for IPv6
        if args.ipv6 and hasattr(socket, 'IPV6_V6ONLY'):
            v6only = httpd.socket.getsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY)
            _log(f"[DIAG] IPV6_V6ONLY: {v6only} (0=dual-stack enabled, 1=IPv6 only)")

        # Start self-diagnostic in background thread
        diag_thread = threading.Thread(
            target=_self_diagnostic,
            args=(args.host, args.port, args.ipv6),
            daemon=True
        )
        diag_thread.start()

        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            _log("Shutting down")
        finally:
            httpd.server_close()

        return 0
    except Exception as e:
        _log(f"Error - {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
