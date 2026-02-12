#!/usr/bin/env python3
"""Minimal Python test to verify py_test execution."""

import os
import sys
import unittest


class MinimalTest(unittest.TestCase):
    """Minimal test case."""

    def test_basic(self):
        """Basic test that always passes."""
        self.assertTrue(True)

    def test_python_version(self):
        """Test that prints Python version."""
        print(f"Python version: {sys.version}")
        print(f"Python executable: {sys.executable}")
        self.assertIsNotNone(sys.version)

    def test_environment(self):
        """Test that prints some environment info."""
        print(f"Current directory: {os.getcwd()}")
        print(f"TEST_TMPDIR: {os.environ.get('TEST_TMPDIR', 'not set')}")
        print(f"TEST_WORKSPACE: {os.environ.get('TEST_WORKSPACE', 'not set')}")
        self.assertIsNotNone(os.getcwd())


if __name__ == '__main__':
    unittest.main()
