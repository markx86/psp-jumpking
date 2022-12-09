#include "panic.h"
#ifdef DEBUG
#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

extern SceCtrlData __ctrlData;
#endif

void panic(const char *fmt, ...) {
#ifdef DEBUG
    va_list list;
    char msg[256];

    va_start(list, fmt);
    vsprintf(msg, fmt, list);
    va_end(list);

    pspDebugScreenInit();
    pspDebugScreenClear();
    pspDebugScreenPrintf("%s\n\n[ PRESS X TO RESET ]", msg);
 
    do {
        sceCtrlReadBufferPositive(&__ctrlData, 1);
        sceKernelDelayThreadCB(100);
    } while (!(__ctrlData.Buttons & PSP_CTRL_CROSS));
#endif
    sceKernelExitGame();
}
