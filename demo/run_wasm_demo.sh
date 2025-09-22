#!/bin/bash
# Простой скрипт для запуска WASM demo через emrun
set -e

echo "WASM Runner:"
echo "  exe: $0"
echo "  args: $@"
echo "  cwd: $(pwd)"
if test "${BUILD_WORKSPACE_DIRECTORY+x}"; then
    echo "    BUILD_WORKSPACE_DIRECTORY: $BUILD_WORKSPACE_DIRECTORY"
fi
if test "${BUILD_WORKING_DIRECTORY+x}"; then
    echo "    BUILD_WORKING_DIRECTORY (cd): $BUILD_WORKING_DIRECTORY"
    cd $BUILD_WORKING_DIRECTORY
fi
if [ $# -lt 1 ]; then
    echo "usage: $0 <html> <args>"
    exit 1
fi

HTML_FILE="$1"
shift

if [ ! -f "$HTML_FILE" ]; then
    echo "Error: HTML file not found: $HTML_FILE"
    echo "Please build first: bazel build --config=wasm //..."
    exit 1
fi

echo "Starting WASM application via emrun..."
emrun --kill_start --kill_exit --browser=chrome --browser_args=-headless "$HTML_FILE" "$@"
