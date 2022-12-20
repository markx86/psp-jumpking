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

extern const GameState *__currentState;
extern SceCtrlData __ctrlData;
extern SceCtrlLatch __latchData;

inline void switchState(const GameState *new) {
    __currentState->cleanup();
    __currentState = new;
    __currentState->init();
}

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 272

#define Input __ctrlData
#define Latch __latchData

extern const GameState IN_GAME;

#endif
