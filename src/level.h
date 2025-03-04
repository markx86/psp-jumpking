#ifndef __LEVEL_H__
#define __LEVEL_H__

#include "compiler.h"
#include "engine.h"
#include <stdint.h>

#define LEVEL_BLOCK_SIZE 8
#define LEVEL_BLOCK_HALF (LEVEL_BLOCK_SIZE / 2)

#define LEVEL_SCREEN_BLOCK_WIDTH 60
#define LEVEL_SCREEN_BLOCK_HEIGHT 45

#define LEVEL_SCREEN_WIDTH (LEVEL_SCREEN_BLOCK_WIDTH * LEVEL_BLOCK_SIZE)
#define LEVEL_SCREEN_HEIGHT (LEVEL_SCREEN_BLOCK_HEIGHT * LEVEL_BLOCK_SIZE)

#define LEVEL_BLOCK_ISSOLID(b) (b < BLOCK_EMPTY)
#define LEVEL_BLOCK_ISSLOPE(b) (b <= BLOCK_SLOPE_BR)

#define LEVEL_COORDS_SCREEN2BLOCK(v) ((vec2i16) { .x = v.x >> 3, .y = v.y >> 3 })
#define LEVEL_COORDS_BLOCK2SCREEN(v) ((vec2i16) { .x = v.x << 3, .y = v.y << 3 })
#define LEVEL_COORDS_SCREENDELTA(v)  ((vec2i16) { .x = v.x - ((v.x >> 3) << 3), .y = v.y - ((v.y >> 3) << 3) })

typedef enum {
  BLOCK_SLOPE_TL,
  BLOCK_SLOPE_TR,
  BLOCK_SLOPE_BL,
  BLOCK_SLOPE_BR,
  BLOCK_SOLID,
  BLOCK_ICE,
  BLOCK_SNOW,
  BLOCK_SAND,
  BLOCK_WATER,
  BLOCK_QUARK,
  BLOCK_EMPTY,
  BLOCK_FAKE,
  BLOCK_NOWIND,
} block_t;

typedef struct {
  uint16_t magic;
  uint8_t wind;
  uint8_t teleport_index;
  uint8_t blocks[LEVEL_SCREEN_BLOCK_HEIGHT][LEVEL_SCREEN_BLOCK_WIDTH];
} PACKED level_screen_t;

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
