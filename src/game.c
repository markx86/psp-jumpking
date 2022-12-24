#include "state.h"
#include "level.h"
#include "king.h"

static int currentScreen;
static short kingSX[2], kingSY[2];
static unsigned int vBuffer;

static void init(void) {
    kingSX[0] = 0; kingSX[1] = 0;
    kingSY[0] = 0; kingSY[1] = 0;
    vBuffer = 0;
    currentScreen = 0;
    loadLevel();
    kingCreate();
}

static void update(float delta) {
    short playerScreenX, playerScreenY;
    LevelScreen *screen = getLevelScreen(currentScreen);
    kingUpdate(delta, screen, &kingSX[!vBuffer], &kingSY[!vBuffer]);
}

static void firstRender(void) {
    renderLevelScreen();
}

static void render(void) {
    renderLevelScreenSection(kingSX[vBuffer] - 16, kingSY[vBuffer] - 32, 32, 32);
    kingRender();
    vBuffer = !vBuffer;
}

static void cleanup(void) {
    kingDestroy();
    unloadLevel();
}

const GameState GAME = {
    .init = &init,
    .update = &update,
    .firstRender = &firstRender,
    .render = &render,
    .cleanup = &cleanup
};
