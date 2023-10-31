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

#define LEVEL_BLOCK_ISSOLID(b)                          \
  (!(b == SCREENBLOCK_EMPTY || b == SCREENBLOCK_FAKE || \
     b == SCREENBLOCK_NOWIND))
#define LEVEL_BLOCK_ISSLOPE(b)                               \
  (b == SCREENBLOCK_SLOPE_TL || b == SCREENBLOCK_SLOPE_TR || \
   b == SCREENBLOCK_SLOPE_BL || b == SCREENBLOCK_SLOPE_BR)

#define LEVEL_SLOPE_DIRECTION(s) ((s % 2) ? 1 : -1);

typedef enum {
  SCREENBLOCK_SLOPE_TL,
  SCREENBLOCK_SLOPE_TR,
  SCREENBLOCK_SLOPE_BL,
  SCREENBLOCK_SLOPE_BR,
  SCREENBLOCK_EMPTY,
  SCREENBLOCK_SOLID,
  SCREENBLOCK_FAKE,
  SCREENBLOCK_ICE,
  SCREENBLOCK_SNOW,
  SCREENBLOCK_SAND,
  SCREENBLOCK_NOWIND,
  SCREENBLOCK_WATER,
  SCREENBLOCK_QUARK
} level_screen_block_t;

typedef struct {
  uint16_t magic;
  uint8_t wind;
  uint8_t teleport_index;
  uint8_t blocks[LEVEL_SCREEN_BLOCK_HEIGHT][LEVEL_SCREEN_BLOCK_WIDTH];
} __attribute__((packed)) level_screen_t;

void level_load(uint32_t start_screen);
level_screen_t* level_get_screen(uint32_t index);
vertex_t* level_render_screen(short scroll);
vertex_t* level_render_screen_section(
    short x,
    short y,
    short width,
    short height,
    uint32_t current_scroll);
void level_render_screen_lines_top(short scroll, short lines);
void level_render_screen_lines_bottom(short scroll, short lines);
void level_render_foreground_on_top(vertex_t* section_vertices);
void level_force_clean_artifact_at(short x, short y, short width, short height);
void level_unload(void);

#endif
