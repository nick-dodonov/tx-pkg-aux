#!/usr/bin/env python3
"""
HTTP server for testing HTTP package functionality.
"""

import sys
import argparse
import json
from http.server import HTTPServer, BaseHTTPRequestHandler
from typing import Any


def _log(*args, **kwargs) -> None:
    """Print with immediate flush."""
    print(*args, **kwargs, flush=True)


class HTTPTestHandler(BaseHTTPRequestHandler):
    """HTTP request handler for GET and POST requests."""

    def do_GET(self) -> None:
        """Handle GET requests."""
        _log(f"HTTP Server: Handled GET request: {self.path}")
        if self.path == "/get":
            response = {
                "method": "GET",
                "path": self.path,
                "message": "GET request successful",
            }
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps(response).encode())
        else:
            self.send_error(404, "Not Found")

    def do_POST(self) -> None:
        """Handle POST requests."""
        _log(f"HTTP Server: Handled POST request: {self.path}")
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
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps(response).encode())
        else:
            self.send_error(404, "Not Found")


def main() -> int:
    """Start HTTP server."""
    parser = argparse.ArgumentParser(description="HTTP server for testing")
    parser.add_argument("--port", type=int, default=8080, help="Port to listen on")
    parser.add_argument("--host", default="localhost", help="Host to bind to")

    args = parser.parse_args()

    try:
        _log(f"HTTP Server: Starting on {args.host}:{args.port}")
        server_address = (args.host, args.port)
        httpd = HTTPServer(server_address, HTTPTestHandler)

        _log("HTTP Server: Ready to accept connections")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            _log("HTTP Server: Shutting down")
        finally:
            httpd.server_close()

        return 0
    except Exception as e:
        _log(f"HTTP Server: Error - {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
