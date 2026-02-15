@echo off
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
