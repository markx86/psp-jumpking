#ifndef __STATE_H__
#define __STATE_H__

#include "panic.h"
#include "loader.h"
#include <pspctrl.h>
#include <pspgu.h>

typedef struct {
    void (*init)(void);
    void (*update)(float delta);
    void (*render)(void);
    void (*cleanup)(void);
} GameState;

// Singletons...
extern SceCtrlData __ctrlData;
extern SceCtrlLatch __latchData;
// and their aliases.
#define Input __ctrlData
#define Latch __latchData

// Essential functions needed by (almost) every state.
extern void switchState(const GameState *new);
extern void setClearFlags(int flags);
extern void setBackgroundData(void *data, unsigned int width, unsigned int height);
extern void cleanBackgroundAt(void *data, short x, short y, short w, short h, unsigned int stride);
extern void setBackgroundScroll(short offset);
extern void skipWaitForThisFrame(void);

// Stuff needed for rendering.
#define STATE_SCREEN_WIDTH 480
#define STATE_SCREEN_HEIGHT 360
#define PSP_SCREEN_WIDTH 480
#define PSP_SCREEN_HEIGHT 272
#define SCREEN_MAX_SCROLL (STATE_SCREEN_HEIGHT - PSP_SCREEN_HEIGHT)
// NOTE: It's better to only use one vertex type,
//       so I've defined one here for the whole game.
//       It uses 16-bit texture (UV) coordinates and 
//       16-bit vertex coordinates.
typedef struct {
    short u, v;
    short x, y, z;
} Vertex;

extern const GameState GAME;

#endif
