#include "level.h"
#include "loader.h"
#include "panic.h"
#include <pspgu.h>
#include <stdio.h>
#include <string.h>

#define LEVEL_SCREEN_MAGIC 0xBABE

#define LEVEL_SCREEN_IMAGEW 512
#define LEVEL_SCREEN_IMAGEH 512
#define LEVEL_SCREEN_BYTES \
  (LEVEL_SCREEN_IMAGEW * (LEVEL_SCREEN_HEIGHT + LEVEL_SCREEN_IMAGEH) * 4)

typedef struct {
  uint16_t total_screens;
  level_screen_t* screens;
} level_t;

typedef struct {
  uint16_t index;
  uint16_t has_foreground;
  char* image;
} screen_handle_t;

typedef enum {
  LOAD_LAZY,
  LOAD_NOW,
} screen_image_loading_type_t;

#define SCREEN_PREV 0
#define SCREEN_THIS 1
#define SCREEN_NEXT 2

static level_t level;
static level_screen_t* last_screen_returned;
static screen_handle_t screen_handle_previous;
static screen_handle_t screen_handle_current;
static screen_handle_t screen_handle_next;
static __attribute__((
    section(".bss"),
    aligned(16))) char images_pool[3][LEVEL_SCREEN_BYTES];

static void
screen_image_loaded_callback(void* data, uint32_t width, uint32_t height) {
  screen_handle_t* handle = (screen_handle_t*)data;
  handle->has_foreground = height > LEVEL_SCREEN_HEIGHT;
}

static void load_screen_image(
    screen_handle_t* handle,
    screen_image_loading_type_t loadType) {
  if (handle->index >= level.total_screens) {
    return;
  }
  char file[64];
  sprintf(file, "assets/screens/%u.qoi", handle->index + 1);
  uint32_t width, height;
  switch (loadType) {
    case LOAD_LAZY:
      loader_lazy_swap_texture_ram(
          file, handle->image, &screen_image_loaded_callback, handle);
      break;
    case LOAD_NOW:
      loader_swap_texture_ram(file, handle->image, &width, &height);
      screen_image_loaded_callback(handle, width, height);
      break;
  }
}

void level_load(uint32_t start_screen) {
  // Load the level data.
  uint32_t size;
  level.screens = loader_read_file("assets/level.bin", &size);
  level.total_screens = size / sizeof(level_screen_t);
  // Initialize screen texture handles.
  screen_handle_previous.index = start_screen - 1;
  screen_handle_previous.image = images_pool[0];
  screen_handle_current.index = start_screen;
  screen_handle_current.image = images_pool[1];
  screen_handle_next.index = start_screen + 1;
  screen_handle_next.image = images_pool[2];
  // Load the appropriate screen textures.
  last_screen_returned = NULL;
  level_get_screen(start_screen);
}

level_screen_t* level_get_screen(uint32_t index) {
  // Check if the index is valid (maybe we computed the wrong index?).
  if (index < level.total_screens) {
    level_screen_t* screen = &level.screens[index];
    // Check if the screen data is intact
    // (it's a simple check but it's better than nothing).
    if (screen->magic != LEVEL_SCREEN_MAGIC) {
      panic(
          "Invalid screen %u: expected magic %04X but found %04X", index,
          LEVEL_SCREEN_MAGIC, screen->magic);
    }
    // Check if the screen is the same as the last we returned.
    // If it is we don't need to load any new textures.
    if (screen != last_screen_returned) {
      // If we need to load new textures we can
      // check which one we need to load.
      void* tmp;
      last_screen_returned = screen;
      if (index == screen_handle_next.index) {
        // If the requested screen is the next one relative to the current
        // one...
        // - shift each handle's indices
        screen_handle_previous.index = screen_handle_current.index;
        screen_handle_current.index = index;
        screen_handle_next.index = index + 1;
        // - shift each handle's has_foreground flag
        screen_handle_previous.has_foreground =
            screen_handle_current.has_foreground;
        screen_handle_current.has_foreground =
            screen_handle_next.has_foreground;
        screen_handle_next.has_foreground = 0;
        // - swap the texture pointers
        tmp = screen_handle_previous.image;
        screen_handle_previous.image = screen_handle_current.image;
        screen_handle_current.image = screen_handle_next.image;
        screen_handle_next.image = tmp;
        // - and finally load (lazily) the next screen.
        load_screen_image(&screen_handle_next, LOAD_LAZY);
      } else if (index == screen_handle_previous.index) {
        // If the requested screen is the previous one relative to the current
        // one...
        // - shift each handle's indices
        screen_handle_next.index = screen_handle_current.index;
        screen_handle_current.index = index;
        screen_handle_previous.index = index - 1;
        // - shift each handle's has_foreground flag
        screen_handle_next.has_foreground =
            screen_handle_current.has_foreground;
        screen_handle_current.has_foreground =
            screen_handle_previous.has_foreground;
        screen_handle_previous.has_foreground = 0;
        // - swap the texture pointers
        tmp = screen_handle_next.image;
        screen_handle_next.image = screen_handle_current.image;
        screen_handle_current.image = screen_handle_previous.image;
        screen_handle_previous.image = tmp;
        // - and finally load (lazily) the next screen.
        load_screen_image(&screen_handle_previous, LOAD_LAZY);
      } else {
        // If the requested screen is completely new
        // - change each handle's indices
        screen_handle_previous.index = index - 1;
        screen_handle_current.index = index;
        screen_handle_next.index = index + 1;
        // Load the current screen immediately
        // This will probably lag the game but
        // that shouldn't be a problem as this is needed
        // only when there is a horizontal transition.
        load_screen_image(&screen_handle_current, LOAD_NOW);
        // Load the next and previous screen lazily as
        // we shouldn't need the immediately.
        load_screen_image(&screen_handle_next, LOAD_LAZY);
        load_screen_image(&screen_handle_previous, LOAD_LAZY);
      }
    }
  }
  return last_screen_returned;
}

static inline __attribute__((always_inline)) void
render_screen(vertex_t* vertices, uint32_t image_offset, int tcc) {
  // Bind the foreground texture.
  sceGuTexMode(GU_PSM_8888, 0, 0, GU_TRUE);
  sceGuTexImage(
      0, LEVEL_SCREEN_IMAGEW, LEVEL_SCREEN_IMAGEH, LEVEL_SCREEN_IMAGEW,
      screen_handle_current.image + image_offset);
  sceGuTexFunc(GU_TFX_REPLACE, tcc);
  sceGuTexFilter(GU_NEAREST, GU_NEAREST);
  // Draw the foreground.
  sceGuDrawArray(
      GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL,
      vertices);
}

void level_render_foreground_on_top(vertex_t* vertices) {
  if (!screen_handle_current.has_foreground || vertices == NULL) {
    return;
  }
  // Update the vertices to be on the layer above the background.
  vertices[0].z = 3;
  vertices[1].z = 3;
  // Enable blending to account for transparency.
  sceGuEnable(GU_BLEND);
  sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  // Call render stub
  render_screen(
      vertices, LEVEL_SCREEN_IMAGEW * LEVEL_SCREEN_HEIGHT * 4, GU_TCC_RGBA);
  // Disable blending.
  sceGuDisable(GU_BLEND);
}

vertex_t* level_render_screen(short scroll) {
  vertex_t* vertices = sceGuGetMemory(2 * sizeof(vertex_t));

  vertices[0].x = 0;
  vertices[0].y = 0;
  vertices[0].z = 0;
  vertices[0].u = 0;
  vertices[0].v = scroll;

  vertices[1].x = PSP_SCREEN_WIDTH;
  vertices[1].y = PSP_SCREEN_HEIGHT;
  vertices[1].z = 0;
  vertices[1].u = LEVEL_SCREEN_WIDTH;
  vertices[1].v = PSP_SCREEN_HEIGHT + scroll;

  render_screen(vertices, 0, GU_TCC_RGB);

  return vertices;
}

void level_render_screen_lines_top(short scroll, short lines) {
  vertex_t* vertices = sceGuGetMemory(2 * sizeof(vertex_t));

  vertices[0].x = 0;
  vertices[0].y = 0;
  vertices[0].z = 1;
  vertices[0].u = 0;
  vertices[0].v = scroll;

  vertices[1].x = PSP_SCREEN_WIDTH;
  vertices[1].y = lines;
  vertices[1].z = 1;
  vertices[1].u = LEVEL_SCREEN_WIDTH;
  vertices[1].v = scroll + lines;

  render_screen(vertices, 0, GU_TCC_RGB);

  // We do not need to return vertices here as the player
  // can never be in a position that would be affected by
  // this draw call.
  level_render_foreground_on_top(vertices);

  // Update the display buffer to avoid graphical glitches.
  queue_display_buffer_update(0, scroll, PSP_SCREEN_WIDTH, lines);
}

void level_render_screen_lines_bottom(short scroll, short lines) {
  vertex_t* vertices = sceGuGetMemory(2 * sizeof(vertex_t));

  short offset = PSP_SCREEN_HEIGHT - lines;

  vertices[0].x = 0;
  vertices[0].y = offset;
  vertices[0].z = 0;
  vertices[0].u = 0;
  vertices[0].v = scroll + offset;

  vertices[1].x = PSP_SCREEN_WIDTH;
  vertices[1].y = offset + lines;
  vertices[1].z = 0;
  vertices[1].u = LEVEL_SCREEN_WIDTH;
  vertices[1].v = offset + scroll + lines;

  render_screen(vertices, 0, GU_TCC_RGB);

  // We do not need to return vertices here as the player
  // can never be in a position that would be affected by
  // this draw call.
  level_render_foreground_on_top(vertices);

  // Update the display buffer to avoid graphical glitches.
  queue_display_buffer_update(0, offset + scroll, PSP_SCREEN_WIDTH, lines);
}

void level_force_clean_artifact_at(
    short x,
    short y,
    short width,
    short height) {
  // Clamp x coordinate within the screen bounds.
  if (x < 0) {
    x = 0;
  } else if (x + width >= LEVEL_SCREEN_WIDTH) {
    x = LEVEL_SCREEN_WIDTH - width;
  }

  // Clamp y coordinate within the screen bounds.
  if (y < 0) {
    y = 0;
  } else if (y + height >= LEVEL_SCREEN_HEIGHT) {
    y = LEVEL_SCREEN_HEIGHT - height;
  }

  queue_display_buffer_update(x, y, width, height);
}

vertex_t* level_render_screen_section(
    short x,
    short y,
    short width,
    short height,
    uint32_t current_scroll) {
  // Clamp x coordinate within the screen bounds.
  if (x < 0) {
    x = 0;
  } else if (x + width >= LEVEL_SCREEN_WIDTH) {
    x = LEVEL_SCREEN_WIDTH - width;
  }

  // Clamp y coordinate within the screen bounds.
  if (y < 0) {
    y = 0;
  } else if (y + height >= LEVEL_SCREEN_HEIGHT) {
    y = LEVEL_SCREEN_HEIGHT - height;
  }

  vertex_t* vertices = sceGuGetMemory(2 * sizeof(vertex_t));

  vertices[0].u = x;
  vertices[0].v = y;
  vertices[0].x = x;
  vertices[0].y = y - current_scroll;
  vertices[0].z = 0;

  vertices[1].u = x + width;
  vertices[1].v = y + height;
  vertices[1].x = x + width;
  vertices[1].y = (y - current_scroll) + height;
  vertices[1].z = 0;

  render_screen(vertices, 0, GU_TCC_RGB);

  return vertices;
}

void level_unload(void) {
  loader_unload_file(level.screens);
  level.screens = NULL;
}
