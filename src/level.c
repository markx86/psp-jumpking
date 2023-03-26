#include "level.h"
#include "loader.h"
#include "state.h"
#include "panic.h"
#include <pspgu.h>
#include <stdio.h>
#include <string.h>

#define LEVEL_SCREEN_MAGIC 0xBABE

#define LEVEL_SCREEN_IMAGEW 512
#define LEVEL_SCREEN_IMAGEH 512
#define LEVEL_SCREEN_BYTES (LEVEL_SCREEN_IMAGEW * LEVEL_SCREEN_IMAGEH * 4)

typedef struct {
    unsigned short totalScreens;
    LevelScreen *screens;
} Level;

typedef struct {
    unsigned int index;
    char *texture;
} LevelScreenHandle;

typedef enum {
    LOAD_LAZY,
    LOAD_NOW,
} LevelScreenLoadingType;

#define SCREEN_PREV 0
#define SCREEN_THIS 1
#define SCREEN_NEXT 2

static Level level;
static LevelScreen *lastScreenReturned;
static LevelScreenHandle screenHandlePrevious;
static LevelScreenHandle screenHandleCurrent;
static LevelScreenHandle screenHandleNext;
static __attribute__((section(".bss"), aligned(16))) char texturesPool[LEVEL_SCREEN_BYTES * 3];

static void loadScreenImage(LevelScreenHandle *handle, LevelScreenLoadingType loadType) {
    if (handle->index >= level.totalScreens) {
        return;
    }
    char file[64];
    sprintf(file, "assets/screens/midground/%u.qoi", handle->index + 1);
    switch (loadType) {
        case LOAD_LAZY:
            lazySwapTextureRam(file, handle->texture);
            break;
        case LOAD_NOW:
            swapTextureRam(file, handle->texture);
            break;
    }
}

void loadLevel(unsigned int startScreen) {
    // Load the level data.
    unsigned int size;
    void *buffer = readFile("assets/level.bin", &size);
    level.totalScreens = size / sizeof(LevelScreen);
    level.screens = malloc(size);
    memcpy(level.screens, buffer, size);
    unloadFile(buffer);
    // Initialize screen texture handles.
    screenHandlePrevious.index = startScreen - 1;
    screenHandlePrevious.texture = texturesPool;
    screenHandleCurrent.index = startScreen;
    screenHandleCurrent.texture = texturesPool + LEVEL_SCREEN_BYTES;
    screenHandleNext.index = startScreen + 1;
    screenHandleNext.texture = texturesPool + LEVEL_SCREEN_BYTES * 2;
    // Load the appropriate screen textures.
    lastScreenReturned = NULL;
    getLevelScreen(startScreen);
}

LevelScreen *getLevelScreen(unsigned int index) {
    // Check if the index is valid (maybe we computed the wrong index?).
    if (index < level.totalScreens) {
        LevelScreen *screen = &level.screens[index];
        // Check if the screen data is intact
        // (it's a simple check but it's better than nothing).
        if (screen->magic != LEVEL_SCREEN_MAGIC) {
            panic("Invalid screen %u: expected magic %04X but found %04X", index, LEVEL_SCREEN_MAGIC, screen->magic);
        }
        // Check if the screen is the same as the last we returned.
        // If it is we don't need to load any new textures.
        if (screen != lastScreenReturned) {
            // If we need to load new textures we can
            // check which one we need to load.
            void *tmp;
            lastScreenReturned = screen;
            if (index == screenHandleNext.index) {
                // If the requested screen is the next one relative to the current one...
                // - shift each handle's indices
                screenHandlePrevious.index = screenHandleCurrent.index;
                screenHandleCurrent.index = index;
                screenHandleNext.index = index + 1;
                // - swap the texture pointers
                tmp = screenHandlePrevious.texture;
                screenHandlePrevious.texture = screenHandleCurrent.texture;
                screenHandleCurrent.texture = screenHandleNext.texture;
                screenHandleNext.texture = tmp;
                // - and finally load (lazily) the next screen.
                loadScreenImage(&screenHandleNext, LOAD_LAZY);
            } else if (index == screenHandlePrevious.index) {
                // If the requested screen is the previous one relative to the current one...
                // - shift each handle's indices
                screenHandleNext.index = screenHandleCurrent.index;
                screenHandleCurrent.index = index;
                screenHandlePrevious.index = index - 1;
                // - swap the texture pointers
                tmp = screenHandleNext.texture;
                screenHandleNext.texture = screenHandleCurrent.texture;
                screenHandleCurrent.texture = screenHandlePrevious.texture;
                screenHandlePrevious.texture = tmp;
                // - and finally load (lazily) the next screen.
                loadScreenImage(&screenHandlePrevious, LOAD_LAZY);
            } else {
                // If the requested screen is completely new
                // - change each handle's indices
                screenHandlePrevious.index = index - 1;
                screenHandleCurrent.index = index;
                screenHandleNext.index = index + 1;
                // Load the current screen immediately
                // This will probably lag the game but
                // that shouldn't be a problem as this is needed
                // only when there is a horizontal transition.
                loadScreenImage(&screenHandleCurrent, LOAD_NOW);
                // Load the next and previous screen lazily as
                // we shouldn't need the immediately.
                loadScreenImage(&screenHandleNext, LOAD_LAZY);
                loadScreenImage(&screenHandlePrevious, LOAD_LAZY);
            }
        }
    }
    return lastScreenReturned;
}

void renderLevelScreen(short scroll) {
    Vertex *vertices = sceGuGetMemory(2 * sizeof(Vertex));
    
    vertices[0].x = 0;
    vertices[0].y = 0;
    vertices[0].z = 0;
    vertices[0].u = 0;
    vertices[0].v = scroll;

    vertices[1].x = PSP_SCREEN_WIDTH;
    vertices[1].y = PSP_SCREEN_HEIGHT;
    vertices[1].z = 0;
    vertices[1].u = LEVEL_SCREEN_PXWIDTH;
    vertices[1].v = PSP_SCREEN_HEIGHT + scroll;
    
    sceGuTexMode(GU_PSM_8888, 0, 0, GU_FALSE);
    sceGuTexImage(0, LEVEL_SCREEN_IMAGEW, LEVEL_SCREEN_IMAGEH, LEVEL_SCREEN_IMAGEW, screenHandleCurrent.texture);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, vertices);
}

void renderLevelScreenLinesTop(short scroll, short lines) {
    Vertex *vertices = sceGuGetMemory(2 * sizeof(Vertex));

    vertices[0].x = 0;
    vertices[0].y = 0;
    vertices[0].z = 1;
    vertices[0].u = 0;
    vertices[0].v = scroll;

    vertices[1].x = PSP_SCREEN_WIDTH;
    vertices[1].y = lines;
    vertices[1].z = 1;
    vertices[1].u = LEVEL_SCREEN_PXWIDTH;
    vertices[1].v = scroll + lines;

    sceGuTexMode(GU_PSM_8888, 0, 0, GU_FALSE);
    sceGuTexImage(0, LEVEL_SCREEN_IMAGEW, LEVEL_SCREEN_IMAGEH, LEVEL_SCREEN_IMAGEW, screenHandleCurrent.texture);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, vertices);
    
    // Update the display buffer to avoid graphical glitches.
    queueDisplayBufferUpdate(0, scroll, PSP_SCREEN_WIDTH, lines);
}

void renderLevelScreenLinesBottom(short scroll, short lines) {
    Vertex *vertices = sceGuGetMemory(2 * sizeof(Vertex));

    short offset = PSP_SCREEN_HEIGHT - lines;

    vertices[0].x = 0;
    vertices[0].y = offset;
    vertices[0].z = 0;
    vertices[0].u = 0;
    vertices[0].v = scroll + offset;

    vertices[1].x = PSP_SCREEN_WIDTH;
    vertices[1].y = offset + lines;
    vertices[1].z = 0;
    vertices[1].u = LEVEL_SCREEN_PXWIDTH;
    vertices[1].v = offset + scroll + lines;

    sceGuTexMode(GU_PSM_8888, 0, 0, GU_FALSE);
    sceGuTexImage(0, LEVEL_SCREEN_IMAGEW, LEVEL_SCREEN_IMAGEH, LEVEL_SCREEN_IMAGEW, screenHandleCurrent.texture);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, vertices);

    // Update the display buffer to avoid graphical glitches.
    queueDisplayBufferUpdate(0, offset + scroll, PSP_SCREEN_WIDTH, lines);
}

void forceCleanLevelArtifactAt(short x, short y, short width, short height) {
    // Clamp x coordinate within the screen bounds.
    if (x < 0) {
        x = 0;
    } else if (x + width >= LEVEL_SCREEN_PXWIDTH) {
        x = LEVEL_SCREEN_PXWIDTH - width;
    }

    // Clamp y coordinate within the screen bounds.
    if (y < 0) {
        y = 0;
    } else if (y + height >= LEVEL_SCREEN_PXHEIGHT) {
        y = LEVEL_SCREEN_PXHEIGHT - height;
    }
    
    queueDisplayBufferUpdate(x, y, width, height);
}

void renderLevelScreenSection(short x, short y, short width, short height, unsigned int currentScroll) {
    // Clamp x coordinate within the screen bounds.
    if (x < 0) {
        x = 0;
    } else if (x + width >= LEVEL_SCREEN_PXWIDTH) {
        x = LEVEL_SCREEN_PXWIDTH - width;
    }

    // Clamp y coordinate within the screen bounds.
    if (y < 0) {
        y = 0;
    } else if (y + height >= LEVEL_SCREEN_PXHEIGHT) {
        y = LEVEL_SCREEN_PXHEIGHT - height;
    }
    
    Vertex *vertices = sceGuGetMemory(2 * sizeof(Vertex));
    
    vertices[0].u = x;
    vertices[0].v = y;
    vertices[0].x = x;
    vertices[0].y = y - currentScroll;
    vertices[0].z = 0;

    vertices[1].u = x + width;
    vertices[1].v = y + height;
    vertices[1].x = x + width;
    vertices[1].y = (y - currentScroll) + height;
    vertices[1].z = 0;
    
    sceGuTexMode(GU_PSM_8888, 0, 0, GU_FALSE);
    sceGuTexImage(0, LEVEL_SCREEN_IMAGEW, LEVEL_SCREEN_IMAGEH, LEVEL_SCREEN_IMAGEW, screenHandleCurrent.texture);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, vertices);
}

void unloadLevel(void) {
    free(level.screens);
    level.screens = NULL;
}
