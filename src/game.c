#include "state.h"
#include "level.h"
#include "king.h"

static int currentScreen;
static short kingSX[2], kingSY[2];
static unsigned int vBuffer, currentFrames;

static void init(void) {
    currentScreen = 0;
    kingSX[0] = 0; kingSX[1] = 0;
    kingSY[0] = 0; kingSY[1] = 0;
    vBuffer = 0;
    currentFrames = 0;
    setClearFlags(GU_DEPTH_BUFFER_BIT);
    loadLevel();
    kingCreate();
}

static void update(float delta) {
    LevelScreen *screen = getLevelScreen(currentScreen);
    kingUpdate(delta, screen);
}

static void render(void) {
    if (currentFrames < 2) {
        // Render the entire screen for the two frames.
        // This has to be done twice, one time for each
        // buffer. See the explanation below for more info
        // on double buffering or check out:
        // https://en.wikipedia.org/wiki/Multiple_buffering#Double_buffering_in_computer_graphics
        renderLevelScreen();
        ++currentFrames;
    } else {
        // Paint over where the king was in the previous frame.
        renderLevelScreenSection(kingSX[vBuffer] - PLAYER_SPRITE_HALFW, kingSY[vBuffer] - PLAYER_SPRITE_HEIGHT, PLAYER_SPRITE_WIDTH, PLAYER_SPRITE_HEIGHT);
    }
    // Render the player.
    kingRender(&kingSX[vBuffer], &kingSY[vBuffer]);
    // The last line flips the buffer selector.
    // This is done because the PSP is double buffered, meaning
    // that while one frame is being shown on the screen,
    // the other is being drawn by the Graphics Engine to a
    // separate buffers. This is done to avoid tearing.
    // To compensate for this remember which coordinates the player's
    // sprite was painted at, for each buffer, and paint over them
    // in the right order.
    vBuffer = !vBuffer;
}

static void cleanup(void) {
    kingDestroy();
    unloadLevel();
}

const GameState GAME = {
    .init = &init,
    .update = &update,
    .render = &render,
    .cleanup = &cleanup
};
