#!/bin/bash
# Простой скрипт для запуска WASM demo через emrun
set -e

echo "WASM Runner input:"
echo "  cwd: $(pwd)"
echo "  exe: $0"
echo "  args: $@"
if test "${BUILD_WORKSPACE_DIRECTORY+x}"; then
    echo "    BUILD_WORKSPACE_DIRECTORY: $BUILD_WORKSPACE_DIRECTORY"
fi
if test "${BUILD_WORKING_DIRECTORY+x}"; then
    echo "    BUILD_WORKING_DIRECTORY: $BUILD_WORKING_DIRECTORY (cd into it)"
    cd $BUILD_WORKING_DIRECTORY
fi
if [ $# -lt 1 ]; then
    echo "usage: $0 <html|wasm> <args>"
    exit 1
fi

_FILE="$1"
shift

_FILE="${_FILE%.*}.html"

if [ ! -f "$_FILE" ]; then
    echo "Error: HTML file not found: $_FILE"
    echo "Please build first: bazel build --config=wasm //..."
    exit 1
fi

echo "WASM Runner emrun:"
echo "  cwd: $(pwd)"
echo "  html: $_FILE"
if [ $# -gt 0 ]; then
    echo "  args: $@"
fi
_CMD="emrun --kill_start --kill_exit --browser=chrome --browser_args=-headless $_FILE $*"
echo "  cmd: $_CMD"
$_CMD