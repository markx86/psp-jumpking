#ifndef __STATE_H__
#define __STATE_H__

#include <pspctrl.h>
#include <pspgu.h>

typedef struct {
    void (*init)(void);
    void (*update)(long delta);
    void (*render)(void);
    void (*cleanup)(void);
} GameState;

extern const GameState *__currentState;
extern SceCtrlData __ctrlData;

inline void switchState(const GameState *new) {
    __currentState->cleanup();
    __currentState = new;
    __currentState->init();
}

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 272

#define Input __ctrlData
#define timesDelta(expr, delta) ((expr * delta) / 10000)

extern const GameState IN_GAME;

#endif
