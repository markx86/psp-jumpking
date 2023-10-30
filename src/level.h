#ifndef __LEVEL_H__
#define __LEVEL_H__

#include "state.h"

#define LEVEL_COORDS_SCREEN2MAP(c) (c >> 3)
#define LEVEL_COORDS_MAP2SCREEN(c) (c << 3)

#define LEVEL_BLOCK_SIZE 8
#define LEVEL_BLOCK_HALF (LEVEL_BLOCK_SIZE / 2)

#define LEVEL_SCREEN_BLOCK_WIDTH 60
#define LEVEL_SCREEN_BLOCK_HEIGHT 45

#define LEVEL_SCREEN_WIDTH LEVEL_COORDS_MAP2SCREEN(LEVEL_SCREEN_BLOCK_WIDTH)
#define LEVEL_SCREEN_HEIGHT LEVEL_COORDS_MAP2SCREEN(LEVEL_SCREEN_BLOCK_HEIGHT)

#define LEVEL_BLOCK_ISSOLID(b) (!(b == BLOCK_EMPTY || b == BLOCK_FAKE || b == BLOCK_NOWIND))
#define LEVEL_BLOCK_ISSLOPE(b) (b == BLOCK_SLOPE_TL || b == BLOCK_SLOPE_TR || b == BLOCK_SLOPE_BL || b == BLOCK_SLOPE_BR)

#define LEVEL_SLOPE_DIRECTION(s) ((s % 2) ? 1 : -1);

typedef enum {
    BLOCK_SLOPE_TL,
    BLOCK_SLOPE_TR,
    BLOCK_SLOPE_BL,
    BLOCK_SLOPE_BR,
    BLOCK_EMPTY,
    BLOCK_SOLID,
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
    unsigned char blocks[LEVEL_SCREEN_BLOCK_HEIGHT][LEVEL_SCREEN_BLOCK_WIDTH];
} __attribute__((packed)) LevelScreen;

void loadLevel(uint32_t startScreen);
LevelScreen *getLevelScreen(uint32_t index);
Vertex *renderLevelScreen(short scroll);
Vertex *renderLevelScreenSection(short x, short y, short width, short height, uint32_t currentScroll);
void renderLevelScreenLinesTop(short scroll, short lines);
void renderLevelScreenLinesBottom(short scroll, short lines);
void renderForegroundOnTop(Vertex *sectionVertices);
void forceCleanLevelArtifactAt(short x, short y, short width, short height);
void unloadLevel(void);

#endif
