#include <stdio.h>
#include <stdlib.h>
#include "mywindows.h"
#include <windows.h>
#include "mmsystem.h"
#pragma comment (lib,"winmm.lib")
int main()
{
    mciSendString("open C:\\Users\\a\\Downloads\\18e8_66c2_6b4e_bc2fc663d2f59f50ad365d718cacbb98.mp3",NULL,0,NULL);
    mciSendString("play C:\\Users\\a\\Downloads\\18e8_66c2_6b4e_bc2fc663d2f59f50ad365d718cacbb98.mp3",NULL,0,NULL);
    initGame();
    while(1);
    return 0;
}
