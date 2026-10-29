@echo off
setlocal

if exist "E:\code\CodeBlocks\CodeBlocks\MinGW\bin\gcc.exe" set "PATH=E:\code\CodeBlocks\CodeBlocks\MinGW\bin;%PATH%"

where gcc >nul 2>nul
if errorlevel 1 goto nogcc
where g++ >nul 2>nul
if errorlevel 1 goto nogcc

echo [1/3] compiling game.c
gcc -std=c11 -O2 -Wall -Wextra -finput-charset=UTF-8 -c game.c -o game.o
if errorlevel 1 goto fail

echo [2/3] compiling snake_gui.cpp
g++ -std=c++17 -O2 -Wall -Wextra -finput-charset=UTF-8 -c snake_gui.cpp -o snake_gui.o
if errorlevel 1 goto fail

echo [3/3] linking snake_gui.exe
g++ game.o snake_gui.o -o snake_gui.exe -mwindows -static -static-libgcc -static-libstdc++ ^
    -lgdiplus -lwinmm -lole32 -luuid -lshell32 -lgdi32
if errorlevel 1 goto fail

echo Done: snake_gui.exe
echo Keep the emoji, sounds and snake_hd_best.txt files next to the exe.
goto end

:nogcc
echo [ERROR] gcc/g++ not found. Install MinGW-w64 or edit this script.
pause
exit /b 1

:fail
echo [ERROR] build failed.
pause
exit /b 1

:end
endlocal
