#ifndef __STATE_H__
#define __STATE_H__

#include "engine.h"
#include "panic.h"
#include "loader.h"
#include <pspgu.h>

typedef struct {
    void (*init)(void);
    void (*update)(float delta);
    void (*render)(void);
    void (*cleanup)(void);
} GameState;

// Aliases.
#define Input __ctrlData
#define Latch __latchData

extern const GameState *currentState;

static inline void switchState(const GameState *new) {
    if (currentState != NULL) {
        currentState->cleanup();
    }
    currentState = new;
    currentState->init();
}

#define renderCurrentState() currentState->render()
#define updateCurrentState(delta) currentState->update(delta);
#define cleanupCurrentState() currentState->cleanup();

// Stuff needed for rendering.
#define STATE_SCREEN_WIDTH 480
#define STATE_SCREEN_HEIGHT 360

extern const GameState gameState;

#endif
