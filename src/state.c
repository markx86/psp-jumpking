#include "state.h"

static const GameState *currentState = NULL;

void switchState(const GameState *new) {
    if (currentState != NULL) {
        currentState->cleanup();
    }
    currentState = new;
    currentState->init();
}

void renderCurrentState(void) {
    currentState->render();
}

void updateCurrentState(float delta) {
    currentState->update(delta);
}

void cleanupCurrentState(void) {
    currentState->cleanup();
}
