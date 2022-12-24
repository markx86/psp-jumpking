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
    char *data;
} LevelScreenTextureHandle;

typedef enum {
    LOAD_LAZY,
    LOAD_NOW,
} LevelScreenLoadingType;

static Level level;
static LevelScreen *lastScreenReturned;
static LevelScreenTextureHandle loadedScreenTextures[LEVEL_MAX_TEXTURE_HANDLES];
static __attribute__((section(".bss"), aligned(16))) char texturesPool[LEVEL_SCREEN_BYTES * LEVEL_MAX_TEXTURE_HANDLES];

static void loadScreenImage(unsigned int handleIndex, LevelScreenLoadingType loadType) {
    LevelScreenTextureHandle *handle = &loadedScreenTextures[handleIndex];
    if (handle->index >= level.totalScreens) {
        return;
    }
    char file[64];
    sprintf(file, "host0://assets/screens/midground/%u.qoi", handle->index + 1);
    switch (loadType) {
        case LOAD_LAZY:
            lazySwapTextureRam(file, handle->data);
            break;
        case LOAD_NOW:
            swapTextureRam(file, handle->data);
            break;
    }
}

void loadLevel(void) {
    unsigned int size;
    void *buffer = readFile("host0://assets/level.bin", &size);
    level.totalScreens = size / sizeof(LevelScreen);
    level.pool = createPool("LevelPool", size);
    level.screens = allocatePool(level.pool);
    memcpy(level.screens, buffer, size);
    unloadFile(buffer);
    lastScreenReturned = NULL;
    for (int i = 0; i < LEVEL_MAX_TEXTURE_HANDLES; i++) {
        loadedScreenTextures[i].index = -1 + i;
        loadedScreenTextures[i].data = texturesPool + (i * LEVEL_SCREEN_BYTES);
    }
    getLevelScreen(0);
}

LevelScreen *getLevelScreen(unsigned int index) {
    if (index < level.totalScreens) {
        LevelScreen *screen = &level.screens[index];
        if (screen->magic != LEVEL_SCREEN_MAGIC) {
            panic("Invalid screen %u: expected magic %X but found %X ", index, LEVEL_SCREEN_MAGIC, lastScreenReturned->magic);
        }
        if (screen != lastScreenReturned) {
            void *tmp;
            lastScreenReturned = screen;
            if (index == loadedScreenTextures[2].index) {
                loadedScreenTextures[0].index = loadedScreenTextures[1].index;
                loadedScreenTextures[1].index = loadedScreenTextures[2].index;
                loadedScreenTextures[2].index = index + 1;
                tmp = loadedScreenTextures[0].data;
                loadedScreenTextures[0].data = loadedScreenTextures[1].data;
                loadedScreenTextures[1].data = loadedScreenTextures[2].data;
                loadedScreenTextures[2].data = tmp;
                loadScreenImage(2, LOAD_LAZY);
            } else if (index == loadedScreenTextures[0].index) {
                loadedScreenTextures[2].index = loadedScreenTextures[1].index;
                loadedScreenTextures[1].index = loadedScreenTextures[0].index;
                loadedScreenTextures[0].index = index - 1;
                tmp = loadedScreenTextures[2].data;
                loadedScreenTextures[2].data = loadedScreenTextures[1].data;
                loadedScreenTextures[1].data = loadedScreenTextures[0].data;
                loadedScreenTextures[0].data = tmp;
                loadScreenImage(0, LOAD_LAZY);
            } else {
                loadedScreenTextures[0].index = index - 1;
                loadedScreenTextures[1].index = index;
                loadedScreenTextures[2].index = index + 1;
                loadScreenImage(1, LOAD_NOW);
                loadScreenImage(0, LOAD_LAZY);
                loadScreenImage(2, LOAD_LAZY);
            }
        }
    }
    return lastScreenReturned;
}

void renderLevelScreen(void) {
    Vertex *vertices = sceGuGetMemory(2 * sizeof(Vertex));
    vertices[0].x = 0;
    vertices[0].y = 0;
    vertices[0].u = 0;
    vertices[0].v = LEVEL_SCREEN_PXHEIGHT - SCREEN_HEIGHT;
    vertices[0].z = 0;
    vertices[1].x = SCREEN_WIDTH;
    vertices[1].y = SCREEN_HEIGHT;
    vertices[1].u = LEVEL_SCREEN_PXWIDTH;
    vertices[1].v = vertices[0].v + SCREEN_HEIGHT;
    vertices[1].z = 0;
    
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexImage(0, LEVEL_SCREEN_IMAGEW, LEVEL_SCREEN_IMAGEH, LEVEL_SCREEN_IMAGEW, loadedScreenTextures[1].data);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, vertices);
}

void renderLevelScreenSection(short x, short y, short width, short height) {
    Vertex *vertices = sceGuGetMemory(2 * sizeof(Vertex));
    vertices[0].u = x;
    vertices[0].v = y;
    vertices[0].x = vertices[0].u;
    vertices[0].y = vertices[0].v - (LEVEL_SCREEN_PXHEIGHT - SCREEN_HEIGHT);
    vertices[0].z = 0;
    vertices[1].u = x + width;
    vertices[1].v = y + height;
    vertices[1].x = vertices[1].u;
    vertices[1].y = vertices[1].v - (LEVEL_SCREEN_PXHEIGHT - SCREEN_HEIGHT);
    vertices[1].z = 0;
    
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexImage(0, LEVEL_SCREEN_IMAGEW, LEVEL_SCREEN_IMAGEH, LEVEL_SCREEN_IMAGEW, loadedScreenTextures[1].data);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, vertices);
}

void unloadLevel(void) {
    freePool(level.pool, level.screens);
    destroyPool(level.pool);
    level.screens = NULL;
}
