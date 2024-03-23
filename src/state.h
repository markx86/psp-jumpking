#ifndef __STATE_H__
#define __STATE_H__

#include "engine.h"
#include "loader.h"
#include "panic.h"
#include <pspgu.h>

static inline void
state_start(const game_state_t* new) {
  if (_current_state != NULL) {
    _current_state->end();
  }
  _current_state = new;
  _current_state->start();
}

#define state_render() _current_state->render()
#define state_update(delta) _current_state->update(delta);
#define state_end() _current_state->end();

// Stuff needed for rendering.
#define STATE_SCREEN_WIDTH 480
#define STATE_SCREEN_HEIGHT 360

extern const game_state_t game_state;

#endif
