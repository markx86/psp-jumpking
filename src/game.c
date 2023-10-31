#include "king.h"
#include "level.h"
#include "state.h"
#include <math.h>

#define SCREEN_SCROLL_SPEED 0.1f

typedef struct {
  short sx[2], sy[2];
  short prev_sx[2], prev_sy[2];
} king_screen_coords_t;

typedef struct {
  short current, target;
  short min, max;
} screen_scroll_t;

static king_screen_coords_t king;
static screen_scroll_t scroll;
static uint32_t framebuffer_index, frame_counter, current_screen_index;

static void start(void) {
  current_screen_index = 0;
  king.sx[0] = 0;
  king.sx[1] = 0;
  king.sy[0] = 0;
  king.sy[1] = 0;
  framebuffer_index = 0;
  frame_counter = 0;

  // Tell the engine to only clear the depth buffer.
  // We don't want to clear the color buffer, since
  // we can't redraw the level background fast enough
  // for every frame.
  set_clear_flags(GU_DEPTH_BUFFER_BIT);
  // Load the level.
  level_load(0);
  // Initialize the player.
  king_create();

  // Initialize screen scroll.
  scroll.current = PSP_SCREEN_MAX_SCROLL;
  scroll.target = PSP_SCREEN_MAX_SCROLL;
  scroll.min = scroll.current;
  scroll.max = scroll.current;
  set_background_scroll(scroll.current);
}

static void update(float delta) {
  // Update the player.
  level_screen_t* screen = level_get_screen(current_screen_index);
  uint32_t new_screen_index = current_screen_index;
  king_update(delta, screen, &new_screen_index);

  // Check if we need to change the screen.
  if (new_screen_index != current_screen_index) {
    // Check if we're moving vertically in the world.
    // The teleport_index stores the screen the game
    // has to switch to if the player goes walks
    // out of the level screen bounds from
    // the left or the right side.
    if (new_screen_index != screen->teleport_index) {
      if (new_screen_index > current_screen_index) {
        // If the player has moved up a screen,
        // we want to render the new screen from
        // the bottom.
        scroll.target = PSP_SCREEN_MAX_SCROLL;
      } else {
        // If the player has moved down a screen,
        // we want to render the new screen from
        // the top.
        scroll.target = 0;
      }
      scroll.current = scroll.target;
      scroll.min = scroll.current;
      scroll.max = scroll.current;
      set_background_scroll(scroll.current);
    }
    current_screen_index = new_screen_index;
    frame_counter = 0;
    // Trigger the level texture loader.
    level_get_screen(current_screen_index);
  }
}

static void render(void) {
  vertex_t* foreground_vertices = NULL;

  if (frame_counter < 2) {
    // Render the entire screen for the two frames.
    // This has to be done twice, one time for each
    // buffer. See explanation at the end of the function.
    foreground_vertices = level_render_screen(scroll.current);
    ++frame_counter;
  } else {
    // Only decrement by half here since we need the
    // sprite's center Y coordinate for calculating the scroll.
    king.sy[framebuffer_index] -= KING_SPRITE_HALFH;

    // Compute and output the new screen scroll value.
    short new_scroll = king.sy[framebuffer_index] - PSP_SCREEN_HEIGHT / 2;
    if (new_scroll > PSP_SCREEN_MAX_SCROLL) {
      new_scroll = PSP_SCREEN_MAX_SCROLL;
    } else if (new_scroll < 0) {
      new_scroll = 0;
    }
    scroll.target = new_scroll;

    // Decrement screen coordinate of the player
    // by half the sprite size to get the player sprite's top-left
    // coordinates. The Y coordinate has to be decremented only
    // by half since we already subtracted half at the top of the
    // else block.
    king.sx[framebuffer_index] -= KING_SPRITE_HALFW;
    king.sy[framebuffer_index] -= KING_SPRITE_HALFH;

    if (scroll.target != scroll.current) {
      // Linear interpolate between the current screen scroll value
      // and the target screen scroll value.
      scroll.current =
          scroll.current +
          (short)ceilf(
              ((float)(scroll.target - scroll.current)) * SCREEN_SCROLL_SPEED);
      // Scroll the screen.
      set_background_scroll(scroll.current);
      // Workaround to clear scrolling artifacts.
      level_force_clean_artifact_at(
          king.prev_sx[framebuffer_index], king.prev_sy[framebuffer_index],
          KING_SPRITE_WIDTH, KING_SPRITE_HEIGHT);
    }

    // Render new screen lines.
    if (scroll.current < scroll.min) {
      // If we're scrolling upwards, render the lines at the top of the screen.
      level_render_screen_lines_top(
          scroll.current, scroll.min - scroll.current);
      // This value keeps track of the lowest scroll value ever reached
      // during the time the current screen has been shown. Remeber that
      // the lower the scroll value, the higher we are in the level screen.
      scroll.min = scroll.current;
    } else if (scroll.current > scroll.max) {
      // If we're scrolling downwards, render the lines at the bottom of the
      // screen.
      level_render_screen_lines_bottom(
          scroll.current, scroll.current - scroll.max);
      // This value keeps track of the highest scroll value ever reached
      // during the time the current screen has been shown. Remeber that
      // the higher the scroll value, the lower we are in the level screen.
      scroll.max = scroll.current;
    }
    // The scroll.min and scroll.max values are used to remember how much of the
    // current screen we have already rendered. Once a line has been rendered,
    // it stays in the draw buffer until we switch screens.

    // Paint over where the king was in the previous frame.
    // Add a 4 pixel padding to account for the error introduced by the fixed
    // update loop.
    foreground_vertices = level_render_screen_section(
        king.sx[framebuffer_index] - 2, king.sy[framebuffer_index] - 2,
        KING_SPRITE_WIDTH + 4, KING_SPRITE_HEIGHT + 4, scroll.current);
  }

  king.prev_sx[framebuffer_index] = king.sx[framebuffer_index];
  king.prev_sy[framebuffer_index] = king.sy[framebuffer_index];

  // Render the player.
  king_render(
      &king.sx[framebuffer_index], &king.sy[framebuffer_index], scroll.current);

  // This has to be done after rendering the player, as the king's texture
  // has to be in the color buffer for transparency to work properly.
  level_render_foreground_on_top(foreground_vertices);

  // The last line flips the buffer selector.
  // This is done because the PSP is double buffered, meaning
  // that while one frame is being shown on the screen,
  // the other is being drawn by the Graphics Engine to a
  // separate buffer. This is done to avoid tearing.
  // To compensate for this, we need to remember at which
  // screen coordinates the player's sprite was drawn at,
  // for each buffer, and paint over them in the right order.
  // For more information on double buffering check out:
  // https://en.wikipedia.org/wiki/Multiple_buffering#Double_buffering_in_computer_graphics
  framebuffer_index ^= framebuffer_index;
}

static void end(void) {
  king_destroy();
  level_unload();
}

const game_state_t game_state =
    {.start = &start, .update = &update, .render = &render, .end = &end};
