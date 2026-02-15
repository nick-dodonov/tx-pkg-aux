@echo off
echo #### Batch (%OS% %PROCESSOR_ARCHITECTURE% %ComSpec%)
:: pause

REM print execution environment
setlocal enabledelayedexpansion
echo PWD %CD%
echo [0] %0
set index=1
for %%a in (%*) do (
    echo   [!index!] %%a
    set /a index+=1
)
echo RUNFILES_DIR=%RUNFILES_DIR%
endlocal
