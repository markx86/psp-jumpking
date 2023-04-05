#include "state.h"
#include "level.h"
#include "king.h"
#include <math.h>

#define SCREEN_SCROLL_SPEED 0.1f

static short kingSX[2], kingSY[2];
static short prevKingSX[2], prevKingSY[2];
static unsigned int vBuffer, frameCounter, currentScreenIndex;
static short currentScroll, targetScroll, minScroll, maxScroll;

static void init(void) {
    currentScreenIndex = 0;
    kingSX[0] = 0; kingSX[1] = 0;
    kingSY[0] = 0; kingSY[1] = 0;
    vBuffer = 0;
    frameCounter = 0;

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
    currentScroll = PSP_SCREEN_MAX_SCROLL;
    targetScroll = PSP_SCREEN_MAX_SCROLL;
    minScroll = currentScroll;
    maxScroll = currentScroll;
    setBackgroundScroll(currentScroll);
}

static void update(float delta) {
    // Update the player.
    LevelScreen *screen = getLevelScreen(currentScreenIndex);
    unsigned int newScreenIndex = currentScreenIndex;
    kingUpdate(delta, screen, &newScreenIndex);

    // Check if we need to change the screen.
    if (newScreenIndex != currentScreenIndex) {
        // Check if we're moving vertically in the world.
        // The teleportIndex stores the screen the game
        // has to switch to if the player goes walks 
        // out of the level screen bounds from
        // the left or the right side.
        if (newScreenIndex != screen->teleportIndex) {
            if (newScreenIndex > currentScreenIndex) {
                // If the player has moved up a screen,
                // we want to render the new screen from
                // the bottom.
                targetScroll = PSP_SCREEN_MAX_SCROLL;
                currentScroll = targetScroll;
            } else {
                // If the player has moved down a screen,
                // we want to render the new screen from
                // the top.
                targetScroll = 0;
                currentScroll = targetScroll;
            }
            minScroll = currentScroll;
            maxScroll = currentScroll;
            setBackgroundScroll(currentScroll);
        }
        currentScreenIndex = newScreenIndex;
        frameCounter = 0;
        // Trigger the level texture loader.
        getLevelScreen(currentScreenIndex);
    }
}

static void render(void) {
    Vertex *sectionVertices = NULL;

    if (frameCounter < 2) {
        // Render the entire screen for the two frames.
        // This has to be done twice, one time for each
        // buffer. See explanation at the end of the function.
        renderLevelScreen(currentScroll);
        ++frameCounter;
    } else {
        // Only decrement by half here since we need the
        // sprite's center Y coordinate for calculating the scroll.
        kingSY[vBuffer] -= PLAYER_SPRITE_HALFH;

        // Compute and output the new screen scroll value.
        short scroll = kingSY[vBuffer] - PSP_SCREEN_HEIGHT / 2;
        if (scroll > PSP_SCREEN_MAX_SCROLL) {
            scroll = PSP_SCREEN_MAX_SCROLL;
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
            currentScroll = currentScroll + (short) ceilf(((float) (targetScroll - currentScroll)) * SCREEN_SCROLL_SPEED);
            // Scroll the screen.
            setBackgroundScroll(currentScroll);
            // Workaround to clear scrolling artifacts.
            forceCleanLevelArtifactAt(prevKingSX[vBuffer], prevKingSY[vBuffer], PLAYER_SPRITE_WIDTH, PLAYER_SPRITE_HEIGHT);
        }

        // Render new screen lines.
        if (currentScroll < minScroll) {
            // If we're scrolling upwards, render the lines at the top of the screen.
            renderLevelScreenLinesTop(currentScroll, minScroll - currentScroll);
            // This value keeps track of the lowest scroll value ever reached
            // during the time the current screen has been shown. Remeber that
            // the lower the scroll value, the higher we are in the level screen.
            minScroll = currentScroll;
        } else if (currentScroll > maxScroll) {
            // If we're scrolling downwards, render the lines at the bottom of the screen.
            renderLevelScreenLinesBottom(currentScroll, currentScroll - maxScroll);
            // This value keeps track of the highest scroll value ever reached
            // during the time the current screen has been shown. Remeber that
            // the higher the scroll value, the lower we are in the level screen.
            maxScroll = currentScroll;
        }
        // The minScroll and maxScroll values are used to remember how much of the
        // current screen we have already rendered. Once a line has been rendered,
        // it stays in the draw buffer until we switch screens. 
        
        // Paint over where the king was in the previous frame.
        // Add a 4 pixel padding to account for the error introduced by the fixed update loop.
        sectionVertices = renderLevelScreenSection(kingSX[vBuffer] - 2, kingSY[vBuffer] - 2, PLAYER_SPRITE_WIDTH + 4, PLAYER_SPRITE_HEIGHT + 4, currentScroll);
    }

    prevKingSX[vBuffer] = kingSX[vBuffer];
    prevKingSY[vBuffer] = kingSY[vBuffer];
    
    // Render the player.
    kingRender(&kingSX[vBuffer], &kingSY[vBuffer], currentScroll);

    // This has to be done after rendering the player, as the king's texture
    // has to be in the color buffer for transparency to work properly.
    renderForegroundOnTop(sectionVertices);

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
