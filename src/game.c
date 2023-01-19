#include "state.h"
#include "level.h"
#include "king.h"

#define SCREEN_SCROLL_SPEED 0.1f

static short kingSX[2], kingSY[2];
static unsigned int vBuffer, currentFrame, currentScreen;
static int currentScroll, targetScroll;

static void init(void) {
    currentScreen = 0;
    kingSX[0] = 0; kingSX[1] = 0;
    kingSY[0] = 0; kingSY[1] = 0;
    vBuffer = 0;
    currentFrame = 0;

    // Tell the engine to only clear the depth buffer.
    // We don't want to clear the color buffer, since
    // we can't redraw the level background fast enough
    // for every frame.
    setClearFlags(GU_DEPTH_BUFFER_BIT);
    // Load the level.
    loadLevel(0);
    // Initialize the player.
    kingCreate();

    // Initialize screen scroll.
    currentScroll = SCREEN_MAX_SCROLL;
    targetScroll = SCREEN_MAX_SCROLL;
    setBackgroundScroll(currentScroll);

    // Copy the entire level screen into the background.
    // This has to be done because the PSP cannot
    // re-render the entire frame @ 60 fps.
    renderLevelScreen();
}

static void update(float delta) {
    LevelScreen *screen = getLevelScreen(currentScreen);
    unsigned int newScreen = currentScreen;
    kingUpdate(delta, screen, &newScreen);

    if (newScreen != currentScreen) {
        if (newScreen != screen->teleportIndex) {
            if (newScreen > currentScreen) {
                targetScroll = SCREEN_MAX_SCROLL;
                currentScroll = targetScroll;
            } else {
                targetScroll = 0;
                currentScroll = targetScroll;
            }
            setBackgroundScroll(currentScroll);
        }
        currentScreen = newScreen;
        currentFrame = 0;

        getLevelScreen(currentScreen);
        renderLevelScreen();
    }
}

static void render(void) {
    if (currentFrame < 2) {
        ++currentFrame;
    } else {
        kingSX[vBuffer] -= PLAYER_SPRITE_HALFW;
        // Only decrement by half here since we need the
        // sprite's center Y coordinate for calculating the scroll.
        kingSY[vBuffer] -= PLAYER_SPRITE_HALFH;

        // Compute and output the new screen scroll value.
        short scroll = SCREEN_MAX_SCROLL * ((float) (kingSY[vBuffer] - (PSP_SCREEN_HEIGHT / 2.0f)) / SCREEN_MAX_SCROLL);
        if (scroll > SCREEN_MAX_SCROLL) {
            scroll = SCREEN_MAX_SCROLL;
        } else if (scroll < 0) {
            scroll = 0;
        }
        targetScroll = scroll;
        
        // Decrement screen Y coordinate of the player
        // by half once more, since we need the top-left
        // coordinates for the rest of the function.
        kingSY[vBuffer] -= PLAYER_SPRITE_HALFH;

        if (targetScroll != currentScroll) {
            // Linear interpolate between the current screen scroll value
            // and the target screen scroll value.
            currentScroll = currentScroll + (targetScroll - currentScroll) * SCREEN_SCROLL_SPEED;
            // Scroll the screen.
            setBackgroundScroll(currentScroll);
            // Workaround to clean graphical glitches that happen when the screen is scrolling.
            forceCleanLevelArtifactAt(kingSX[vBuffer], kingSY[vBuffer], PLAYER_SPRITE_WIDTH, PLAYER_SPRITE_HEIGHT);
        }

        // Paint over where the king was in the previous frame.
        renderLevelScreenSection(kingSX[vBuffer], kingSY[vBuffer], PLAYER_SPRITE_WIDTH, PLAYER_SPRITE_HEIGHT, currentScroll);
    }
    
    // Render the player.
    kingRender(&kingSX[vBuffer], &kingSY[vBuffer], currentScroll);

    // The last line flips the buffer selector.
    // This is done because the PSP is double buffered, meaning
    // that while one frame is being shown on the screen,
    // the other is being drawn by the Graphics Engine to a
    // separate buffer. This is done to avoid tearing.
    // To compensate for this, we need to remember at which
    // screen coordinates the player's sprite was drawn at, 
    // for each buffer, and paint over them in the right order.
    // For more information on double buffering check out:
    // https://en.wikipedia.org/wiki/Multiple_buffering#Double_buffering_in_computer_graphics
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
