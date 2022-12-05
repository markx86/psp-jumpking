#ifndef __PANIC_H__
#define __PANIC_H__

#ifdef DEBUG
#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

extern SceCtrlData __ctrlData;
#endif

inline void panic(const char *fmt, ...) {
#ifdef DEBUG
    va_list list;
    char msg[256];

    va_start(list, fmt);
    vsprintf(msg, fmt, list);
    va_end(list);

    pspDebugScreenInit();
    pspDebugScreenClear();
    pspDebugScreenPrintf("%s\nPress X to exit.", msg);

    do {
        sceKernelDelayThread(50);
    } while (!(__ctrlData.Buttons & PSP_CTRL_CROSS));
#endif
    sceKernelExitGame();
}

#endif
