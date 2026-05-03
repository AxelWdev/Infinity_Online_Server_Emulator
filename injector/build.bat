@echo off
setlocal EnableExtensions

pushd "%~dp0"

echo ========================================
echo Compiling Hook DLL and Injector
echo ========================================
echo.

set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSINSTALL="

if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "VSINSTALL=%%I"
    )
)

if defined VSINSTALL (
    if exist "%VSINSTALL%\VC\Auxiliary\Build\vcvarsall.bat" (
        echo [*] Loading Visual Studio C++ x86 build environment...
        call "%VSINSTALL%\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul
    ) else if exist "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" (
        echo [*] Loading Visual Studio C++ x86 build environment...
        call "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64 >nul
    )
)

where cl >nul 2>nul
if errorlevel 1 (
    echo Error: cl.exe was not found.
    echo Open an "x86 Native Tools Command Prompt for VS" or install
    echo the Visual Studio C++ x86/x64 build tools, then run build.bat again.
    echo.
    popd
    pause
    exit /b 1
)

REM Clean previous outputs
del /q *.obj *.lib *.exp hook.dll injector.exe 2>nul

REM Compile the hook DLL
echo [1/2] Compiling hook.dll...
cl /nologo /LD /EHsc /O2 hook.cpp user32.lib /link /MACHINE:X86 /OUT:hook.dll
if errorlevel 1 (
    echo Error compiling hook.dll!
    popd
    pause
    exit /b 1
)
echo [OK] hook.dll compiled successfully!
echo.

REM Compile the injector
echo [2/2] Compiling injector.exe...
cl /nologo /EHsc /O2 injector.cpp /link /MACHINE:X86 /OUT:injector.exe
if errorlevel 1 (
    echo Error compiling injector.exe!
    popd
    pause
    exit /b 1
)
echo [OK] injector.exe compiled successfully!
echo.

REM Clean up intermediate files
echo Cleaning up...
del /q *.obj *.exp *.lib 2>nul

echo.
echo ========================================
echo BUILD COMPLETE!
echo ========================================
echo.
echo Files created:
echo   - hook.dll
echo   - injector.exe
echo.
echo Usage:
echo   1. Start xclient.exe
echo   2. Put hook.dll next to injector.exe
echo   3. Run injector.exe
echo.
echo Optional:
echo   injector.exe some-other-hook.dll
echo.

popd
pause
exit /b 0
