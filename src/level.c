#include "level.h"
#include "loader.h"
#include "state.h"
#include "panic.h"
#include <pspgu.h>
#include <stdio.h>
#include <string.h>

#define LEVEL_MAX_TEXTURE_HANDLES 3
#define LEVEL_SCREEN_MAGIC 0xBABE

#define LEVEL_SCREEN_IMAGEW 512
#define LEVEL_SCREEN_IMAGEH 512
#define LEVEL_SCREEN_BYTES (LEVEL_SCREEN_IMAGEW * LEVEL_SCREEN_IMAGEH * 4)

typedef struct {
    unsigned short totalScreens;
    SceUID pool;
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
static LevelScreenHandle loadedScreensHandles[LEVEL_MAX_TEXTURE_HANDLES];
static __attribute__((section(".bss"), aligned(16))) char texturesPool[LEVEL_SCREEN_BYTES * LEVEL_MAX_TEXTURE_HANDLES];

static void loadScreenImage(unsigned int handleIndex, LevelScreenLoadingType loadType) {
    LevelScreenHandle *handle = &loadedScreensHandles[handleIndex];
    if (handle->index >= level.totalScreens) {
        return;
    }
    char file[64];
    sprintf(file, "host0://assets/screens/midground/%u.qoi", handle->index + 1);
    switch (loadType) {
        case LOAD_LAZY:
            lazySwapTextureRam(file, handle->texture);
            break;
        case LOAD_NOW:
            swapTextureRam(file, handle->texture);
            break;
    }
}

void loadLevel(void) {
    // Load the level data.
    unsigned int size;
    void *buffer = readFile("host0://assets/level.bin", &size);
    level.totalScreens = size / sizeof(LevelScreen);
    level.pool = createPool("LevelPool", size);
    level.screens = allocatePool(level.pool);
    memcpy(level.screens, buffer, size);
    unloadFile(buffer);
    // Initialize screen texture handles.
    for (int i = 0; i < LEVEL_MAX_TEXTURE_HANDLES; i++) {
        loadedScreensHandles[i].index = -1 + i;
        loadedScreensHandles[i].texture = texturesPool + (i * LEVEL_SCREEN_BYTES);
    }
    // Load the appropriate screen textures.
    lastScreenReturned = NULL;
    getLevelScreen(0);
}

LevelScreen *getLevelScreen(unsigned int index) {
    // Check if the index is valid (maybe we computed the wrong index?).
    if (index < level.totalScreens) {
        LevelScreen *screen = &level.screens[index];
        // Check if the screen data is intact
        // (it's a simple check but it's better than nothing).
        if (screen->magic != LEVEL_SCREEN_MAGIC) {
            panic("Invalid screen %u: expected magic %X but found %X ", index, LEVEL_SCREEN_MAGIC, lastScreenReturned->magic);
        }
        // Check if the screen is the same as the last we returned.
        // If it is we don't need to load any new textures.
        if (screen != lastScreenReturned) {
            // If we need to load new textures we can
            // check which one we need to load.
            void *tmp;
            lastScreenReturned = screen;
            if (index == loadedScreensHandles[SCREEN_NEXT].index) {
                // If the requested screen is the next one relative to the current one...
                // - shift each handle's indices
                loadedScreensHandles[SCREEN_PREV].index = loadedScreensHandles[SCREEN_THIS].index;
                loadedScreensHandles[SCREEN_THIS].index = index;
                loadedScreensHandles[SCREEN_NEXT].index = index + 1;
                // - swap the texture pointers
                tmp = loadedScreensHandles[SCREEN_PREV].texture;
                loadedScreensHandles[SCREEN_PREV].texture = loadedScreensHandles[SCREEN_THIS].texture;
                loadedScreensHandles[SCREEN_THIS].texture = loadedScreensHandles[SCREEN_NEXT].texture;
                loadedScreensHandles[SCREEN_NEXT].texture = tmp;
                // - and finally load (lazily) the next screen.
                loadScreenImage(SCREEN_NEXT, LOAD_LAZY);
            } else if (index == loadedScreensHandles[SCREEN_PREV].index) {
                // If the requested screen is the previous one relative to the current one...
                // - shift each handle's indices
                loadedScreensHandles[SCREEN_NEXT].index = loadedScreensHandles[SCREEN_THIS].index;
                loadedScreensHandles[SCREEN_THIS].index = index;
                loadedScreensHandles[SCREEN_PREV].index = index - 1;
                // - swap the texture pointers
                tmp = loadedScreensHandles[SCREEN_NEXT].texture;
                loadedScreensHandles[SCREEN_NEXT].texture = loadedScreensHandles[SCREEN_THIS].texture;
                loadedScreensHandles[SCREEN_THIS].texture = loadedScreensHandles[SCREEN_PREV].texture;
                loadedScreensHandles[SCREEN_PREV].texture = tmp;
                // - and finally load (lazily) the next screen.
                loadScreenImage(SCREEN_PREV, LOAD_LAZY);
            } else {
                // If the requested screen is completely new
                // - change each handle's indices
                loadedScreensHandles[SCREEN_PREV].index = index - 1;
                loadedScreensHandles[SCREEN_THIS].index = index;
                loadedScreensHandles[SCREEN_NEXT].index = index + 1;
                // Load the current screen immediately
                // This will probably lag the game but
                // that shouldn't be a problem as this is needed
                // only when there is a horizontal transition.
                loadScreenImage(SCREEN_THIS, LOAD_NOW);
                // Load the next and previous screen lazily as
                // we shouldn't need the immediately.
                loadScreenImage(SCREEN_PREV, LOAD_LAZY);
                loadScreenImage(SCREEN_NEXT, LOAD_LAZY);
            }
        }
    }
    return lastScreenReturned;
}

void renderLevelScreen(void) {
    setBackgroundData(loadedScreensHandles[SCREEN_THIS].texture, LEVEL_SCREEN_IMAGEW, LEVEL_SCREEN_PXHEIGHT, GU_PSM_8888);
}

void forceCleanLevelArtifactAt(short x, short y, short width, short height) {
    cleanBackgroundAt(loadedScreensHandles[SCREEN_THIS].texture, x, y, width, height, LEVEL_SCREEN_IMAGEW);
}

void renderLevelScreenSection(short x, short y, short width, short height, unsigned int currentScroll) {
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
    
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexImage(0, LEVEL_SCREEN_IMAGEW, LEVEL_SCREEN_IMAGEH, LEVEL_SCREEN_IMAGEW, loadedScreensHandles[SCREEN_THIS].texture);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, vertices);
}

void unloadLevel(void) {
    freePool(level.pool, level.screens);
    destroyPool(level.pool);
    level.screens = NULL;
}
