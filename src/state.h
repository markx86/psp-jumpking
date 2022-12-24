#ifndef __STATE_H__
#define __STATE_H__

#include "panic.h"
#include "loader.h"
#include <pspctrl.h>
#include <pspgu.h>

typedef struct {
    void (*init)(void);
    void (*update)(float delta);
    void (*firstRender)(void);
    void (*render)(void);
    void (*cleanup)(void);
} GameState;

typedef struct {
    short u, v;
    short x, y, z;
} Vertex;

extern SceCtrlData __ctrlData;
extern SceCtrlLatch __latchData;

extern void switchState(const GameState *new);

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 272

#define Input __ctrlData
#define Latch __latchData

extern const GameState GAME;

#endif
