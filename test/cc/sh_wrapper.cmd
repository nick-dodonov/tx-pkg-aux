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
}

echo "#### Execute: $@"
exec "$@"
}
@goto $@
exit

:(){
@echo off
:: Batch script here
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
endlocal

echo #### Execute: %*
call "%1" %2 %3 %4 %5 %6 %7 %8 %9
set _ERR=%ERRORLEVEL%
echo #### Error: %_ERR%
exit /b %_ERR%
