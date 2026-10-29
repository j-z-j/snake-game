// Console-subsystem entry used only to export preview PNGs from snake_gui.cpp.
#include <windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int);

int main()
{
    return WinMain(GetModuleHandleW(NULL), NULL, GetCommandLineA(), 0);
}
