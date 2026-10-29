@echo off
setlocal

rem Prefer the MinGW bundled with Code::Blocks, then fall back to PATH.
if exist "E:\code\CodeBlocks\CodeBlocks\MinGW\bin\gcc.exe" set "PATH=E:\code\CodeBlocks\CodeBlocks\MinGW\bin;%PATH%"

where gcc >nul 2>nul
if errorlevel 1 goto nogcc

echo [1/2] compiling...
gcc -std=c11 -O2 -Wall -Wextra -finput-charset=UTF-8 game.c render.c main.c -o snake_hd.exe -lwinmm
if errorlevel 1 goto buildfail

echo [2/2] done: snake_hd.exe
echo Run: snake_hd.exe    Selftest: snake_hd.exe --selftest
goto end

:nogcc
echo [ERROR] gcc not found. Install MinGW-w64 or edit this script.
pause
exit /b 1

:buildfail
echo [ERROR] build failed.
pause
exit /b 1

:end
endlocal
