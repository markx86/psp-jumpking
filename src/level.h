#ifndef __LEVEL_H__
#define __LEVEL_H__

#include "alloc.h"

#define LEVEL_BLOCK_SIZE 8
#define LEVEL_BLOCK_HALF (LEVEL_BLOCK_SIZE / 2)

#define LEVEL_SCREEN_WIDTH 60
#define LEVEL_SCREEN_HEIGHT 45

#define LEVEL_SCREEN_PXWIDTH (LEVEL_SCREEN_WIDTH * LEVEL_BLOCK_SIZE)
#define LEVEL_SCREEN_PXHEIGHT (LEVEL_SCREEN_HEIGHT * LEVEL_BLOCK_SIZE)

#define LEVEL_BLOCK_ISSOLID(b) (!(b == BLOCK_EMPTY || b == BLOCK_FAKE || b == BLOCK_NOWIND))
#define LEVEL_BLOCK_ISSLOPE(b) (b == BLOCK_SLOPE_TL || b == BLOCK_SLOPE_TR || b == BLOCK_SLOPE_BL || b == BLOCK_SLOPE_BR)

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
    unsigned char teleportIndex;
    unsigned char blocks[LEVEL_SCREEN_HEIGHT][LEVEL_SCREEN_WIDTH];
} __attribute__((packed)) LevelScreen;

void loadLevel(unsigned int startScreen);
LevelScreen *getLevelScreen(unsigned int index);
void renderLevelScreen(void);
void renderLevelScreenSection(short x, short y, short width, short height, unsigned int currentScroll);
void forceCleanLevelArtifactAt(short x, short y, short width, short height);
void unloadLevel(void);

#endif
