@echo off
setlocal

cd /d "%~dp0"

if not exist "build" mkdir build

g++ -shared -o build\d3d9.dll main.cpp crc32.cpp ^
    -ld3d9 -ld3dx9 ^
    -Wl,--enable-stdcall-fixup ^
    -O2 -s

if %ERRORLEVEL% neq 0 (
    echo Build failed
    exit /b 1
)

echo Build succeeded: build\d3d9.dll
