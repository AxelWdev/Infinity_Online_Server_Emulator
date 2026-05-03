@echo off
setlocal

set "SCRIPT_DIR=%~dp0."
set "BUILD_DIR=%SCRIPT_DIR%\build"
set "CONFIG=%~1"

if "%CONFIG%"=="" set "CONFIG=Release"

echo Configuring CPP_Server...
cmake -S "%SCRIPT_DIR%" -B "%BUILD_DIR%"
if errorlevel 1 goto :fail

echo Building %CONFIG%...
cmake --build "%BUILD_DIR%" --config %CONFIG% --target tcp_lzss_server_cpp
if errorlevel 1 goto :fail

echo.
echo Build complete.
echo Executable:
echo %BUILD_DIR%\%CONFIG%\tcp_lzss_server_cpp.exe
goto :eof

:fail
echo.
echo Build failed.
exit /b 1
