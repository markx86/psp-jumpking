#include "state.h"
#include "level.h"
#include "king.h"

#define SCREEN_SCROLL_SPEED 0.1f

static short kingSX[2], kingSY[2];
static unsigned int vBuffer, frameCounter, currentScreen;
static short currentScroll, targetScroll, minScroll, maxScroll;
static short drawLinesBottom, drawLinesTop;

static void init(void) {
    currentScreen = 0;
    kingSX[0] = 0; kingSX[1] = 0;
    kingSY[0] = 0; kingSY[1] = 0;
    vBuffer = 0;
    frameCounter = 0;
    drawLinesBottom = 0;
    drawLinesTop = 0;

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
    minScroll = currentScroll;
    maxScroll = currentScroll;
    setBackgroundScroll(currentScroll);

    // Copy the entire level screen into the background.
    // This has to be done because the PSP cannot
    // re-render the entire frame @ 60 fps.
    //renderLevelScreen();
}

#include <math.h>

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
            minScroll = currentScroll;
            maxScroll = currentScroll;
            setBackgroundScroll(currentScroll);
        }
        currentScreen = newScreen;
        drawLinesBottom = 0;
        drawLinesTop = 0;
        frameCounter = 0;
        getLevelScreen(currentScreen);
        //renderLevelScreen();
    }
}

static short prevScroll;

static void render(void) {
    if (frameCounter < 2) {
        prevScroll = currentScroll;
        renderLevelScreen(currentScroll);
        ++frameCounter;
    } else {
        // Only decrement by half here since we need the
        // sprite's center Y coordinate for calculating the scroll.
        kingSY[vBuffer] -= PLAYER_SPRITE_HALFH;

        // Compute and output the new screen scroll value.
        short scroll = kingSY[vBuffer] - PSP_SCREEN_HEIGHT / 2;
        if (scroll > SCREEN_MAX_SCROLL) {
            scroll = SCREEN_MAX_SCROLL;
        } else if (scroll < 0) {
            scroll = 0;
        }
        targetScroll = scroll;
        
        // Decrement screen coordinate of the player
        // by half the sprite size to get the player sprite's top-left
        // coordinates. The Y coordinate has to be decremented only
        // by half since we already subtracted half at the top of the
        // else block.
        kingSX[vBuffer] -= PLAYER_SPRITE_HALFW;
        kingSY[vBuffer] -= PLAYER_SPRITE_HALFH;
        
        if (targetScroll != currentScroll) {
            // Linear interpolate between the current screen scroll value
            // and the target screen scroll value.
            currentScroll = currentScroll + (short) roundf(((float) (targetScroll - currentScroll)) * SCREEN_SCROLL_SPEED);
            // Scroll the screen.
            setBackgroundScroll(currentScroll);
            // Workaround to clean graphical glitches that happen when the screen is scrolling.
            forceCleanLevelArtifactAt(kingSX[vBuffer], kingSY[vBuffer], PLAYER_SPRITE_WIDTH, PLAYER_SPRITE_HEIGHT);
        }

        // Render new screen lines
        if (targetScroll < minScroll) {
            renderLevelScreenLinesTop(currentScroll, minScroll - currentScroll);
            if (++frameCounter % 2 == 0) {
                minScroll = prevScroll;
            }
        } else if (targetScroll > maxScroll) {
            renderLevelScreenLinesBottom(currentScroll, currentScroll - maxScroll);
            if (++frameCounter % 2 == 0) {
                maxScroll = prevScroll;
            }
        }

        prevScroll = currentScroll;

        // Paint over where the king was in the previous frame.
        // Add a 4 pixel padding to account for the error introduced by the fixed update loop.
        renderLevelScreenSection(kingSX[vBuffer] - 2, kingSY[vBuffer] - 2, PLAYER_SPRITE_WIDTH + 4, PLAYER_SPRITE_HEIGHT + 4, currentScroll);
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
