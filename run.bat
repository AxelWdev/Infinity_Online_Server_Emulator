@echo off
setlocal

set "SCRIPT_DIR=%~dp0."
set "CONFIG=Release"
set "RUN_ARGS="

if /I "%~1"=="Debug" (
    set "CONFIG=Debug"
    shift
) else if /I "%~1"=="Release" (
    set "CONFIG=Release"
    shift
)

:collect_args
if "%~1"=="" goto args_done
set "RUN_ARGS=%RUN_ARGS% %1"
shift
goto collect_args

:args_done

call "%SCRIPT_DIR%\build.bat" %CONFIG%
if errorlevel 1 goto :fail

set "EXE=%SCRIPT_DIR%\build\%CONFIG%\tcp_lzss_server_cpp.exe"

if not exist "%EXE%" (
    echo.
    echo Executable not found:
    echo %EXE%
    goto :fail
)

echo.
echo Running:
echo %EXE%%RUN_ARGS%
pushd "%SCRIPT_DIR%"
"%EXE%"%RUN_ARGS%
set "RUN_EXIT=%ERRORLEVEL%"
popd
exit /b %RUN_EXIT%

:fail
echo.
echo Run failed.
exit /b 1
