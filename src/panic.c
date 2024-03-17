#include "panic.h"
#ifdef DEBUG
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspkernel.h>
#include <stdarg.h>
#include <stdio.h>

extern SceCtrlData _ctrl_data;
#endif

void
panic(const char* fmt, ...) {
#ifdef DEBUG
  va_list list;
  char msg[256];

  va_start(list, fmt);
  vsnprintf(msg, sizeof(msg), fmt, list);
  va_end(list);

  pspDebugScreenInit();
  pspDebugScreenPrintf("%s\n\n[ PRESS X TO RESET ]", msg);

  do {
    sceCtrlReadBufferPositive(&_ctrl_data, 1);
    sceKernelDelayThreadCB(100);
  } while (!(_ctrl_data.Buttons & PSP_CTRL_CROSS));
#endif
  sceKernelExitGame();
}
