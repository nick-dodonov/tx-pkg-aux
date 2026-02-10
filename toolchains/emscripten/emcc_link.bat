@ECHO OFF

call %~dp0\env.bat

py -3 %ROOT_DIR%\external\emsdk+\emscripten_toolchain\link_wrapper.py %*
