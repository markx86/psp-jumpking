#ifndef __ENGINE_H__
#define __ENGINE_H__

#include <pspctrl.h>

#define PSP_SCREEN_WIDTH 480
#define PSP_SCREEN_HEIGHT 272
#define PSP_SCREEN_MAX_SCROLL (STATE_SCREEN_HEIGHT - PSP_SCREEN_HEIGHT)

void setClearFlags(int flags);
void queueDisplayBufferUpdate(short x, short y, short w, short h);
void setBackgroundScroll(short offset);

// Singletons.
extern SceCtrlData __ctrlData;
extern SceCtrlLatch __latchData;

// NOTE: It's better to only use one vertex type,
//       so I've defined one here for the whole game.
//       It uses 16-bit texture (UV) coordinates and 
//       16-bit vertex coordinates.
typedef struct {
    short u, v;
    short x, y, z;
} Vertex;

#endif
