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

// Stuff needed for rendering.
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 272
// (NOTE) It's better to only use one vertex type,
//        so I've defined one here for the whole game.
//        It uses 16-bit texture (UV) coordinates and 
//        16-bit vertex coordinates.
typedef struct {
    short u, v;
    short x, y, z;
} Vertex;

extern const GameState GAME;

#endif
