#!/usr/bin/env python3
"""
HTTP server for testing HTTP package functionality.
"""

import sys
import argparse
import json
from http.server import HTTPServer, BaseHTTPRequestHandler
from typing import Any


class HTTPTestHandler(BaseHTTPRequestHandler):
    """HTTP request handler for GET and POST requests."""

    def log_message(self, format: str, *args: Any) -> None:
        """Override to customize logging."""
        sys.stderr.write(f"HTTP Server: {format % args}\n")

    def do_GET(self) -> None:
        """Handle GET requests."""
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

    server_address = (args.host, args.port)
    httpd = HTTPServer(server_address, HTTPTestHandler)

    print(f"HTTP Server: Starting on {args.host}:{args.port}", flush=True)
    print("HTTP Server: Ready to accept connections", flush=True)

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nHTTP Server: Shutting down", flush=True)
    finally:
        httpd.server_close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
