#ifndef __KING_H__
#define __KING_H__

#include "level.h"

// Graphics constants
#define KING_SPRITE_WIDTH 32
#define KING_SPRITE_HEIGHT 32
#define KING_SPRITE_HALFW (KING_SPRITE_WIDTH / 2)
#define KING_SPRITE_HALFH (KING_SPRITE_HEIGHT / 2)

void king_create(void);
void king_update(float delta, level_screen_t *screen, uint32_t *out_screen_index);
void king_render(short *out_sx, short *out_sy, uint32_t current_scroll);
void king_destroy(void);

#endif
