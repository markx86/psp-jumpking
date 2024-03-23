#ifndef __ENGINE_H__
#define __ENGINE_H__

#include <pspctrl.h>

#define PSP_SCREEN_WIDTH 480
#define PSP_SCREEN_HEIGHT 272
#define PSP_SCREEN_MAX_SCROLL (STATE_SCREEN_HEIGHT - PSP_SCREEN_HEIGHT)

typedef struct {
  void (*start)(void);
  void (*update)(float delta);
  void (*render)(void);
  void (*end)(void);
} game_state_t;

// Aliases.
#define input _ctrl_data
#define latch _latch_data

// Singletons.
extern SceCtrlData _ctrl_data;
extern SceCtrlLatch _latch_data;
extern const game_state_t* _current_state;

void set_clear_flags(int flags);
void queue_display_buffer_update(short x, short y, short w, short h);
void set_background_scroll(short offset);
int is_running(void);

// NOTE: It's better to only use one vertex type,
//       so I've defined one here for the whole game.
//       It uses 16-bit texture (UV) coordinates and
//       16-bit vertex coordinates.
typedef struct {
  short u, v;
  short x, y, z;
} vertex_t;

#endif
