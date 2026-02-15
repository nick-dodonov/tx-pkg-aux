#!/usr/bin/env bash
echo "#### Shell ($(uname -sm) $SHELL)"
# read -esp "Press Enter to continue..."

# print execution environment
{
echo "  PWD $(pwd)"
echo "  [0] $0"
index=1
for arg in "$@"; do
    echo "  [$index] $arg"
    index=$((index + 1))
done
echo "  BUILD_WORKING_DIRECTORY=$BUILD_WORKING_DIRECTORY"
echo "  BUILD_WORKSPACE_DIRECTORY=$BUILD_WORKSPACE_DIRECTORY"
echo "  RUNFILES_DIR=$RUNFILES_DIR"
}
