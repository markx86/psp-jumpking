#ifndef __KING_H__
#define __KING_H__

#include "level.h"

// Graphics constants
#define PLAYER_SPRITE_WIDTH 32
#define PLAYER_SPRITE_HEIGHT 32
#define PLAYER_SPRITE_HALFW (PLAYER_SPRITE_WIDTH / 2)
#define PLAYER_SPRITE_HALFH (PLAYER_SPRITE_HEIGHT / 2)

void kingCreate(void);
void kingUpdate(float delta, LevelScreen *screen, uint32_t *outScreenIndex);
void kingRender(short *outSX, short *outSY, uint32_t currentScroll);
void kingDestroy(void);

#endif
