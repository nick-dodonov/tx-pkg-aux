#!/usr/bin/env bash
echo "Hello from sh_wrapper"
echo "PWD $(pwd)"
# output all arguments with their index
echo "[0] $0"
index=1
for arg in "$@"; do
    echo "  [$index] $arg"
    index=$((index + 1))
done
echo "SHELL=$SHELL"
# echo "RUNFILES=$RUNFILES"
# echo "RUNFILES_MANIFEST_FILE=$RUNFILES_MANIFEST_FILE"
echo "RUNFILES_DIR=$RUNFILES_DIR"
