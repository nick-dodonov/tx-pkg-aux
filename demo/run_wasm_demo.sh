#!/bin/bash
set -e

echo "Running: $0 $@"

# Простой скрипт для запуска WASM demo через emrun
SCRIPT_DIR="$(dirname "$0")"
HTML_FILE="$SCRIPT_DIR/log_demo.html"

if [ ! -f "$HTML_FILE" ]; then
    echo "Error: HTML file not found: $HTML_FILE"
    echo "Please build first: bazel build --config=wasm-debug //demo:log_demo"
    exit 1
fi

echo "Starting WASM application via emrun..."
emrun --kill_start --kill_exit --browser=chrome --browser_args=-headless "$HTML_FILE"
