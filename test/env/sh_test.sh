#!/usr/bin/env bash

echo "==================== Runtime (Test) Environment Info ===================="
echo "[0]: $0"
echo "Shell: $SHELL"
echo "Bash version: $BASH_VERSION"
echo "Bash path: $(which bash 2>/dev/null || echo 'not found')"
echo "PWD: $PWD"
echo "HOME: $HOME"
echo "USER: $USER"
echo "PATH: $PATH"
echo ""
echo "==================== Test Environment ===================="
echo "TEST_TMPDIR: ${TEST_TMPDIR:-not set}"
echo "TEST_WORKSPACE: ${TEST_WORKSPACE:-not set}"
echo "TEST_SRCDIR: ${TEST_SRCDIR:-not set}"
echo "TEST_BINARY: ${TEST_BINARY:-not set}"
echo "XML_OUTPUT_FILE: ${XML_OUTPUT_FILE:-not set}"
echo ""
echo "==================== System Info ===================="
uname -a 2>/dev/null || echo "uname not available"
echo ""
echo "==================== All Environment Variables ===================="
env | sort
echo ""
echo "==================== Build-Time Environment (if available) ===================="
# TODO: pass as argument instead
if [ -f "${TEST_SRCDIR}/_main/test/sh_gen.txt" ]; then
    cat "${TEST_SRCDIR}/_main/test/sh_gen.txt"
else
    echo "Build environment info not found at: ${TEST_SRCDIR}/_main/test/sh_gen.txt"
fi
echo ""
echo "Test completed successfully!"
