@goto(){
# Shell script here
# TODO: via runfiles and fallback: $(dirname "$0")/sh_wrapper.sh
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
echo "  RUNFILES_MANIFEST_FILE=$RUNFILES_MANIFEST_FILE"
}

# Check if no arguments provided
if [ $# -eq 0 ]; then
    ARGS_FILE="${0%.cmd}.args"
    if [ -f "$ARGS_FILE" ]; then
        echo "#### Loading arguments from: $ARGS_FILE"
        # Read arguments from file (portable, works on macOS)
        read -r ARGS_LINE < "$ARGS_FILE"
        set -- $ARGS_LINE
        echo "#### Loaded arguments: $@"
    else
        echo "#### ERROR: No arguments provided and args file not found: $ARGS_FILE" >&2
        exit 1
    fi
fi

echo "#### Execute: $@"
# exec "$@"
"$@"
_ERR=$?
echo "#### Errorcode: $_ERR"
exit $_ERR
}

@goto $@
exit

:(){
:: Batch script here
@echo off
:: TODO: via runfiles and fallback: call "%~dp0sh_wrapper.bat"
echo #### Batch (%OS% %PROCESSOR_ARCHITECTURE% %ComSpec%)
:: pause # "Press any key to continue..."

REM print execution environment
setlocal enabledelayedexpansion
echo   PWD %CD%
echo   [0] %0
set index=1
for %%a in (%*) do (
    echo   [!index!] %%a
    set /a index+=1
)
echo   BUILD_WORKING_DIRECTORY=%BUILD_WORKING_DIRECTORY%
echo   BUILD_WORKSPACE_DIRECTORY=%BUILD_WORKSPACE_DIRECTORY%
echo   RUNFILES_DIR=%RUNFILES_DIR%
echo   RUNFILES_MANIFEST_FILE=%RUNFILES_MANIFEST_FILE%

REM Check if no arguments provided
if "%~1"=="" (
    set "ARGS_FILE=%~dpn0.args"
    if exist "!ARGS_FILE!" (
        echo #### Loading arguments from: !ARGS_FILE!
        set /p ARGS_LINE=<"!ARGS_FILE!"
        echo #### Loaded arguments: !ARGS_LINE!
        
        REM Convert forward slashes to backslashes for Windows
        REM TODO: generate with backslashes in the first place to avoid this step
        set "ARGS_LINE=!ARGS_LINE:/=\!"
        echo #### Execute: !ARGS_LINE!
        cmd /c !ARGS_LINE!
        set _ERR=!ERRORLEVEL!
        echo #### Error: !_ERR!
        exit /b !_ERR!
    ) else (
        echo #### ERROR: No arguments provided and args file not found: !ARGS_FILE! >&2
        exit /b 1
    )
)

echo #### Execute: %*
"%1" %2 %3 %4 %5 %6 %7 %8 %9
set _ERR=%ERRORLEVEL%
echo #### Error: %_ERR%
exit /b %_ERR%
