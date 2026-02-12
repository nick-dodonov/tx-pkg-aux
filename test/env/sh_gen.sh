#!/usr/bin/env bash
# This script captures build-time environment information

OUTPUT="$1"

cat > "$OUTPUT" << 'EOF_HEADER'
==================== Build-Time Environment Info ====================
EOF_HEADER

{
    echo "Executed at: $(date)"
    echo "Shell: $SHELL"
    echo "Bash version: $BASH_VERSION"
    echo "Bash path: $(which bash 2>/dev/null || echo 'not found')"
    echo "PWD: $PWD"
    echo "HOME: $HOME"
    echo "USER: ${USER:-${USERNAME:-not set}}"
    echo ""
    echo "==================== Build Environment Variables ===================="
    env | sort
    echo ""
    echo "==================== System Info ===================="
    uname -a 2>/dev/null || echo "uname not available"
} >> "$OUTPUT"
