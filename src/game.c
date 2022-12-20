#include "state.h"
#include "king.h"

static int currentScreen;

static void init(void) {
    currentScreen = 0;
    king.create();
}

static void update(float delta) {
    king.update(delta, &currentScreen);
}

static void render(void) {
    king.render();
}

static void cleanup(void) {
    king.destroy();
}

const GameState IN_GAME = {
    .init = &init,
    .update = &update,
    .render = &render,
    .cleanup = &cleanup
};
