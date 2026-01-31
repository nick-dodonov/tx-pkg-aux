#!/usr/bin/env python3
"""
HTTP client tests for testing HTTP package functionality.
"""

import sys
import argparse
import json
import unittest
from urllib.request import urlopen, Request
from urllib.error import HTTPError, URLError


def _log(*args, **kwargs) -> None:
    """Print with immediate flush."""
    print("test_client:", *args, **kwargs, flush=True)


class HTTPClientTests(unittest.TestCase):
    """HTTP client test cases."""

    base_url = "http://localhost:8080"

    @classmethod
    def setUpClass(cls) -> None:
        """Set up test class."""
        parser = argparse.ArgumentParser()
        parser.add_argument(
            "--url", default="http://localhost:8080", help="Base URL for tests"
        )
        args, _ = parser.parse_known_args()
        cls.base_url = args.url
        _log(f"Running tests against: {cls.base_url}")

    def test_get_request(self) -> None:
        """Test GET request to /get endpoint."""
        url = f"{self.base_url}/get"
        _log(f"Testing GET: {url}")

        try:
            with urlopen(url, timeout=5) as response:
                self.assertEqual(response.status, 200)
                data = json.loads(response.read().decode())
                self.assertEqual(data["method"], "GET")
                self.assertEqual(data["path"], "/get")
                _log(f"GET response: {data}")
        except (HTTPError, URLError) as e:
            self.fail(f"GET request failed: {e}")

    def test_post_request(self) -> None:
        """Test POST request to /post endpoint."""
        url = f"{self.base_url}/post"
        _log(f"Testing POST: {url}")

        post_data: dict[str, str | int] = {"test_key": "test_value", "number": 42}
        json_data = json.dumps(post_data).encode()

        try:
            req = Request(
                url, data=json_data, headers={"Content-Type": "application/json"}
            )
            with urlopen(req, timeout=5) as response:
                self.assertEqual(response.status, 200)
                data = json.loads(response.read().decode())
                self.assertEqual(data["method"], "POST")
                self.assertEqual(data["path"], "/post")
                self.assertEqual(data["received_data"], post_data)
                _log(f"POST response: {data}")
        except (HTTPError, URLError) as e:
            self.fail(f"POST request failed: {e}")

    def test_not_found(self) -> None:
        """Test 404 response for unknown endpoint."""
        url = f"{self.base_url}/unknown"
        _log(f"Testing 404: {url}")

        with self.assertRaises(HTTPError) as context:
            urlopen(url, timeout=5)

        self.assertEqual(context.exception.code, 404)
        _log(f"404 response received as expected")


def main() -> int:
    """Run HTTP client tests."""
    # Run unittest with verbosity
    loader = unittest.TestLoader()
    suite = loader.loadTestsFromTestCase(HTTPClientTests)
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)

    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
