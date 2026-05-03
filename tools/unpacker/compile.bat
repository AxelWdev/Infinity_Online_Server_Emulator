@echo off
setlocal EnableExtensions

pushd "%~dp0"

set "MINGW=C:\msys64\mingw64"
set "GCC=%MINGW%\bin\gcc.exe"
set "INCLUDE=%MINGW%\include"
set "LIB=%MINGW%\lib"

if not exist "%GCC%" (
    echo Error: "%GCC%" was not found.
    echo Install MSYS2 MinGW64 or update compile.bat to point at your gcc toolchain.
    popd
    exit /b 1
)

if not exist "%INCLUDE%\zlib.h" (
    echo Error: zlib headers were not found at "%INCLUDE%\zlib.h".
    echo Install mingw-w64-x86_64-zlib in MSYS2, then run this script again.
    popd
    exit /b 1
)

if not exist "%LIB%\libz.a" if not exist "%LIB%\libz.dll.a" (
    echo Error: zlib library was not found in "%LIB%".
    echo Install mingw-w64-x86_64-zlib in MSYS2, then run this script again.
    popd
    exit /b 1
)

set "COMMON=-O2 -std=c11 -Wall -Wextra -I%INCLUDE%"

echo [1/2] Building unpack.exe...
"%GCC%" %COMMON% -o unpack.exe unpack.c -L%LIB% -static -s -lz
if errorlevel 1 goto :dynamic_unpack
goto :build_repack

:dynamic_unpack
echo [!] Static link failed for unpack.exe, retrying with dynamic zlib...
"%GCC%" %COMMON% -o unpack.exe unpack.c -L%LIB% -s -lz
if errorlevel 1 goto :fail

:build_repack
echo [2/2] Building repack.exe...
"%GCC%" %COMMON% -o repack.exe repack.c -L%LIB% -static -s -lz
if errorlevel 1 goto :dynamic_repack
goto :done

:dynamic_repack
echo [!] Static link failed for repack.exe, retrying with dynamic zlib...
"%GCC%" %COMMON% -o repack.exe repack.c -L%LIB% -s -lz
if errorlevel 1 goto :fail

:done
echo.
echo Build complete:
echo   %cd%\unpack.exe
echo   %cd%\repack.exe
popd
exit /b 0

:fail
echo.
echo Build failed.
popd
exit /b 1
