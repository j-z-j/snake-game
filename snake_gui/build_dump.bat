@echo off
setlocal

rem Console-subsystem build used only to export preview PNGs.
if exist "E:\code\CodeBlocks\CodeBlocks\MinGW\bin\gcc.exe" set "PATH=E:\code\CodeBlocks\CodeBlocks\MinGW\bin;%PATH%"
where gcc >nul 2>nul
if errorlevel 1 goto fail
where g++ >nul 2>nul
if errorlevel 1 goto fail

gcc -std=c11 -O2 -Wall -Wextra -finput-charset=UTF-8 -c game.c -o game.o
if errorlevel 1 goto fail
g++ -std=c++17 -O2 -Wall -Wextra -finput-charset=UTF-8 -c snake_gui.cpp -o snake_gui.o
if errorlevel 1 goto fail
g++ -std=c++17 -O2 -c dump_main.cpp -o dump_main.o
if errorlevel 1 goto fail
g++ game.o snake_gui.o dump_main.o -o snake_gui_dump.exe -mconsole -static -static-libgcc -static-libstdc++ ^
    -lgdiplus -lwinmm -lole32 -luuid -lshell32 -lgdi32
if errorlevel 1 goto fail

echo Done: snake_gui_dump.exe
echo Usage: snake_gui_dump.exe --dump preview.png play
goto end

:fail
echo [ERROR] build failed.
pause
exit /b 1

:end
endlocal
