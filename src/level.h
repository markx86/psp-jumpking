#ifndef __LEVEL_H__
#define __LEVEL_H__

#include "alloc.h"

#define LEVEL_BLOCK_WIDTH 8
#define LEVEL_BLOCK_HEIGHT 8

#define LEVEL_SCREEN_WIDTH 60
#define LEVEL_SCREEN_HEIGHT 45

#define LEVEL_SCREEN_PXWIDTH (LEVEL_SCREEN_WIDTH * LEVEL_BLOCK_WIDTH)
#define LEVEL_SCREEN_PXHEIGHT (LEVEL_SCREEN_HEIGHT * LEVEL_BLOCK_HEIGHT)

typedef enum {
    BLOCK_EMPTY,
    BLOCK_SOLID,
    BLOCK_SLOPE_TL,
    BLOCK_SLOPE_TR,
    BLOCK_SLOPE_BL,
    BLOCK_SLOPE_BR,
    BLOCK_FAKE,
    BLOCK_ICE,
    BLOCK_SNOW,
    BLOCK_SAND,
    BLOCK_NOWIND,
    BLOCK_WATER,
    BLOCK_QUARK
} LevelScreenBlock;

typedef struct {
    unsigned short magic;
    unsigned char wind;
    char teleportIndex;
    unsigned char blocks[LEVEL_SCREEN_HEIGHT][LEVEL_SCREEN_WIDTH];
} LevelScreen;

void loadLevel(void);
LevelScreen *getLevelScreen(unsigned int index);
void renderLevelScreen(void);
void renderLevelScreenSection(short x, short y, short width, short height);
void unloadLevel(void);

#endif
