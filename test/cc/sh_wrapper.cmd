#!/usr/bin/env bash
# 2>nul 1>&2 & goto :batch
echo "Running shell ($(uname))"
# commands
read -esp "Press Enter to continue..."
exit

:batch
@echo off
echo Running batch (%OS% %PROCESSOR_ARCHITECTURE%).
:: commands
@rem pause
exit /b
