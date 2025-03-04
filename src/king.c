#include "king.h"
#include "compiler.h"
#include "loader.h"
#include "panic.h"
#include <pspgu.h>
#include <string.h>

// Hitbox sizes
#define KING_HITBOX_WIDTH  18
#define KING_HITBOX_HEIGHT 26
#define KING_HITBOX_HALFW  (KING_HITBOX_WIDTH / 2)
#define KING_HITBOX_HALFH  (KING_HITBOX_HEIGHT / 2)

// Physics constants
#define KING_JUMP_VSPEED     9.45f
#define KING_JUMP_HSPEED     3.5f
#define KING_WALK_SPEED      1.5f
#define KING_HIT_DAMPENING   0.45f
#define KING_GRAVITY         0.275f
#define KING_MAX_FALL_SPEED  10.0f

// Status constants
#define KING_CHARGE_TIME     0.6f
#define KING_STUN_TIME       0.5f
#define KING_MAX_FALL_TIME   1.0f

// Input constants
#define KING_JUMP_LENIENCY_FRAMES 4

// Player sprites
typedef enum {
  SPRITE_STANDING = 0,
  SPRITE_WALKING0,
  SPRITE_WALKING1,
  SPRITE_WALKING2,
  SPRITE_CHARGING,
  SPRITE_JUMPING,
  SPRITE_FALLING,
  SPRITE_STUNNED,
  SPRITE_HITWALLMIDAIR,
  SPRITE_MAX
} sprite_t;

typedef enum {
  STATE_IDLE,
  STATE_WALKING,
  STATE_CHARGING,
  STATE_JUMPING,
  STATE_FALLING,
  STATE_STUNNED
} state_t;

// Collision modifiers
typedef enum {
  COLLMOD_NONE   = 0,
  COLLMOD_NOWIND = 1 << 0,
  COLLMOD_WATER  = 1 << 1,
  COLLMOD_SAND   = 1 << 2,
  COLLMOD_QUARK  = 1 << 3,
  COLLMOD_ICE    = 1 << 4,
  COLLMOD_SNOW   = 1 << 5,
  COLLMOD_SOLID  = 1 << 6,
  COLLMOD_SLOPE  = 1 << 7,
} collision_modifier_t;

typedef enum {
  COLLZONE_LEFT      = 1 << 0,
  COLLZONE_RIGHT     = 1 << 1,
  COLLZONE_TOP       = 1 << 2,
  COLLZONE_BOTTOM    = 1 << 3,
  COLLZONE_CORNER_TL = 1 << 4,
  COLLZONE_CORNER_TR = 1 << 5,
  COLLZONE_CORNER_BL = 1 << 6,
  COLLZONE_CORNER_BR = 1 << 7,
} collision_zone_t;

typedef enum {
  COLLTYPE_VERTICAL,
  COLLTYPE_HORIZONTAL
} collision_type_t;

typedef struct {
  // the collision modifiers
  uint32_t modifiers;
  // the block the player has collided with
  block_t block;
  // width and height of the intersection rectangle
  vec2i16 dim;
  // extrapolated flags
  collision_type_t type;
} collision_info_t;

static const int block_collision_data[] = {
  [BLOCK_SLOPE_TL] = COLLMOD_SLOPE,
  [BLOCK_SLOPE_TR] = COLLMOD_SLOPE,
  [BLOCK_SLOPE_BL] = COLLMOD_SLOPE,
  [BLOCK_SLOPE_BR] = COLLMOD_SLOPE,
  [BLOCK_EMPTY]    = COLLMOD_NONE,
  [BLOCK_SOLID]    = COLLMOD_SOLID,
  [BLOCK_FAKE]     = COLLMOD_NONE,
  [BLOCK_ICE]      = COLLMOD_SOLID | COLLMOD_ICE,
  [BLOCK_SNOW]     = COLLMOD_SOLID | COLLMOD_SNOW,
  [BLOCK_SAND]     = COLLMOD_SAND,
  [BLOCK_NOWIND]   = COLLMOD_NOWIND,
  [BLOCK_WATER]    = COLLMOD_WATER,
  [BLOCK_QUARK]    = COLLMOD_QUARK
};

static const vec2f slope_normals[] = {
  [BLOCK_SLOPE_TL] = { .x = -COS_45, .y = +SIN_45 },
  [BLOCK_SLOPE_TR] = { .x = +COS_45, .y = +SIN_45 },
  [BLOCK_SLOPE_BL] = { .x = -COS_45, .y = -SIN_45 },
  [BLOCK_SLOPE_BR] = { .x = +COS_45, .y = -SIN_45 }
};

typedef state_t (*update_callback_t)(float, uint32_t*);
typedef state_t (*collision_callback_t)(collision_info_t*);

// Physics
static vec2f world_coords, velocity, slope_dir;
// Input
static int input_direction, jump_pressed;
static float jump_power;
// Status
static state_t current_state;
static int hit_wall, walk_animation_cycle, on_ground;
static int is_sliding_slope; // The sliding status is fucking annoying because it can affect multiple states, so we have to do this
static int last_input_direction;
static int leniency_direction, leniency_frames;
static float stunned_timer, fall_time;
// Graphics
static short sprite_ucoord_offset;
static char *sprite_sheet, *current_sprite;

#ifdef DEBUG
static vec2i16 coll_tl, coll_br;
#endif

static ALWAYS_INLINE void
update_sprite_ucoord_offset(void) {
  sprite_ucoord_offset = last_input_direction < 0 ? KING_SPRITE_WIDTH : 0;
}

static ALWAYS_INLINE vec2i16
get_screen_coords(void) {
  return (vec2i16) {
    .x = ((short)world_coords.x) + (LEVEL_SCREEN_WIDTH / 2),
    .y = LEVEL_SCREEN_HEIGHT - ((short)world_coords.y)
  };
}

static ALWAYS_INLINE void
set_sprite(sprite_t sprite) {
  if (sprite < 0 || sprite >= SPRITE_MAX)
    panic("Invalid sprite: %d", sprite);
  current_sprite = sprite_sheet + sprite * KING_SPRITE_HEIGHT * KING_SPRITE_WIDTH * 4;
}

static void
switch_to_state(state_t new_state) {
  switch (new_state) {
    case STATE_IDLE:
      if (!is_sliding_slope)
        velocity.x = 0.0f;
      velocity.y = 0.0f;
      hit_wall = 0;
      set_sprite(SPRITE_STANDING);
      break;

    case STATE_CHARGING:
      if (!is_sliding_slope)
        velocity.x = 0.0f;
      velocity.y = 0.0f;
      jump_power = 0.0f;
      set_sprite(SPRITE_CHARGING);
      leniency_direction = input_direction;
      break;

    case STATE_WALKING:
      walk_animation_cycle = -1;
      break;

    case STATE_JUMPING:
      velocity.y = jump_power;
      velocity.x = leniency_direction * KING_JUMP_HSPEED;
      update_sprite_ucoord_offset();
      set_sprite(SPRITE_JUMPING);
      is_sliding_slope = 0;
      break;

    case STATE_FALLING:
      fall_time = 0.0f;
      if (!hit_wall)
        set_sprite(SPRITE_FALLING);
      break;

    case STATE_STUNNED:
      if (!is_sliding_slope)
        velocity.x = 0.0f;
      stunned_timer = KING_STUN_TIME;
      set_sprite(SPRITE_STUNNED);
      hit_wall = 0;
      break;
  }
  current_state = new_state;
}

void
king_create(void) {
  sprite_sheet = loader_load_texture_vram("assets/king/base/regular.qoi", NULL, NULL);
  if ((intptr_t)sprite_sheet & 0xf)
    panic("Sprite sheet is not aligned to 16 bytes: %p", sprite_sheet);
  // Set the player starting position.
  // world_coords.x = 0.0f;
  // world_coords.y = 32.0f;
  // world_coords.x = -48.0f;
  // world_coords.y = 120.0f;
  world_coords.x = 180.0f;
  world_coords.y = 248.0f;
  // Set the player initial velocity.
  velocity.x = 0.0f;
  velocity.y = 0.0f;
  // Set initial state.
  sprite_ucoord_offset = 0;
  is_sliding_slope = 0;
  last_input_direction = 0;
  leniency_direction = 0;
  switch_to_state(STATE_IDLE);
}

static ALWAYS_INLINE void
adjust_coords(collision_info_t* info) {
  if (info->type == COLLTYPE_VERTICAL)
    world_coords.y -= signf(velocity.y) * info->dim.y;
  else
    world_coords.x -= signf(velocity.x) * info->dim.x;
}

static ALWAYS_INLINE void
adjust_velocity_on_slope(void) {
  float vel = vec2f_dot(&slope_dir, &velocity);
  velocity.x = slope_dir.x * vel;
  velocity.y = slope_dir.y * vel;
}

static state_t update_idle(float delta, uint32_t* out_screen_index) {
  UNUSED(delta);
  UNUSED(out_screen_index);

  if (!on_ground)
    return STATE_FALLING;
  if (input_direction)
    return STATE_WALKING;
  if (jump_pressed)
    return STATE_CHARGING;

  return STATE_IDLE;
}

static state_t update_walking(float delta, uint32_t* out_screen_index) {
  sprite_t next_sprite;

  UNUSED(delta);
  UNUSED(out_screen_index);

  if (!on_ground)
    return STATE_FALLING;
  if (!input_direction)
    return STATE_IDLE;
  else if (jump_pressed)
    return STATE_CHARGING;

  velocity.x = input_direction * KING_WALK_SPEED;
  update_sprite_ucoord_offset();

  switch (walk_animation_cycle++ >> 2) {
    case 0:
    case 1:
    case 2:
      next_sprite = SPRITE_WALKING0;
      break;
    case 3:
      next_sprite = SPRITE_WALKING1;
      break;
    case 4:
    case 5:
    case 6:
      next_sprite = SPRITE_WALKING2;
      break;
    default:
      next_sprite = SPRITE_WALKING1;
      walk_animation_cycle = 0;
      break;
  }
  set_sprite(next_sprite);

  return STATE_WALKING;
}

static state_t update_charging(float delta, uint32_t* out_screen_index) {
  UNUSED(delta);
  UNUSED(out_screen_index);

  if (input_direction) {
    leniency_frames = KING_JUMP_LENIENCY_FRAMES;
    leniency_direction = input_direction;
  }
  else if (--leniency_frames <= 0)
    leniency_direction = 0;

  if (jump_pressed && jump_power < KING_JUMP_VSPEED)
    jump_power += (KING_JUMP_VSPEED / KING_CHARGE_TIME) * delta;
  else if (jump_power > 0.0f)
    return STATE_JUMPING;

  return STATE_CHARGING;
}

static state_t update_jumping(float delta, uint32_t* out_screen_index) {
  float new_velocity_y;

  UNUSED(delta);
  UNUSED(out_screen_index);

  new_velocity_y = velocity.y - KING_GRAVITY;
  if (new_velocity_y < 0.0f)
    return STATE_FALLING;

  velocity.y = new_velocity_y;
  if (UNLIKELY(is_sliding_slope))
    adjust_velocity_on_slope();

  return STATE_JUMPING;
}

static state_t update_falling(float delta, uint32_t* out_screen_index) {
  UNUSED(delta);
  UNUSED(out_screen_index);

  if (velocity.y > -KING_MAX_FALL_SPEED) {
    velocity.y -= KING_GRAVITY;
    if (UNLIKELY(is_sliding_slope))
      adjust_velocity_on_slope();
  }

  fall_time += delta;

  return STATE_FALLING;
}

static state_t update_stunned(float delta, uint32_t* out_screen_index) {
  UNUSED(delta);
  UNUSED(out_screen_index);

  if (stunned_timer > 0.0f)
    stunned_timer -= delta;
  else if (jump_pressed)
    return STATE_IDLE;
  else if (input_direction)
    return STATE_WALKING;

  return STATE_STUNNED;
}

static const update_callback_t update_callbacks[] = {
  [STATE_IDLE] = &update_idle,
  [STATE_WALKING] = &update_walking,
  [STATE_CHARGING] = &update_charging,
  [STATE_JUMPING] = &update_jumping,
  [STATE_FALLING] = &update_falling,
  [STATE_STUNNED] = &update_stunned,
};

static ALWAYS_INLINE int
check_for_collision(collision_info_t* i, level_screen_t* screen) {
  int collisions, mods, corner;
  int on_left_side, on_top_side, on_x_axis, on_y_axis;
  uint16_t min_dist2, dist2;
  short dx, dy;
  block_t block;
  collision_zone_t zone;
  collision_type_t type;
  vec2i16 cur, map, coll;
  vec2i16 start, end, center;
  vec2i16 tl, br;
  vec2i16 new_tl, new_br;
  vec2f abs_velocity = {
    .x = absf(velocity.x),
    .y = absf(velocity.y)
  };

  min_dist2 = 0xffff;
  collisions = 0;

  i->block = BLOCK_EMPTY;
  i->modifiers = 0;

recheck_collisions:
  corner = 0;
  center = get_screen_coords();

  // Compute the player center's screen coordinates.
  center.y -= KING_HITBOX_HALFH;

  start.x = MAX(center.x - KING_HITBOX_HALFW, 0);
  start.y = MAX(center.y - KING_HITBOX_HALFH, 0);

  end.x = MIN(center.x + KING_HITBOX_HALFW, LEVEL_SCREEN_WIDTH);
  end.y = MIN(center.y + KING_HITBOX_HALFH, LEVEL_SCREEN_HEIGHT);

  br = start;
  tl = end;

  for (cur.y = start.y; cur.y < end.y; cur.y += LEVEL_BLOCK_SIZE) {
    for (cur.x = start.x; cur.x < end.x; cur.x += LEVEL_BLOCK_SIZE) {
      map = LEVEL_COORDS_SCREEN2BLOCK(cur);
      block = screen->blocks[map.y][map.x];

      // Skip empty blocks.
      if (LIKELY(block == BLOCK_EMPTY))
        continue;

      // Add new modifiers.
      mods = block_collision_data[block];
      if (LIKELY(mods & (COLLMOD_SOLID | COLLMOD_SLOPE))) {
        coll = LEVEL_COORDS_BLOCK2SCREEN(map);

        // FIXME: Is this even needed?
        // Check if this is the closest block.
        dx = cur.x - center.x;
        dy = cur.y - center.y;
        dist2 = dx * dx + dy * dy;
        if (dist2 < min_dist2) {
          i->block = block;
          min_dist2 = dist2;
        }

        // Recompute collision rectangle.
        if (mods & COLLMOD_SOLID)
        {
          // Update top-left corner coords.
          new_tl.x = MIN(tl.x, coll.x);
          new_tl.y = MIN(tl.y, coll.y);

          // Discard collision if player has hit a corner.
          if (new_tl.x < center.x && br.x > center.x &&
              new_tl.y < center.y && br.y > center.y) {
            corner = 1;
            continue;
          }

          // Update bottom-right corner coords.
          new_br.x = MAX(br.x, coll.x + LEVEL_BLOCK_SIZE);
          new_br.y = MAX(br.y, coll.y + LEVEL_BLOCK_SIZE);

          // Discard collision if player has hit a corner.
          if (new_tl.x < center.x && new_br.x > center.x &&
              new_tl.y < center.y && new_br.y > center.y) {
            corner = 1;
            continue;
          }

          tl = new_tl;
          br = new_br;
        }
      }

      // Increment collision count and register new modifiers.
      i->modifiers |= mods;
      ++collisions;
    }
  }

  if (i->modifiers & COLLMOD_SOLID) {
    tl.x = MAX(tl.x, start.x);
    tl.y = MAX(tl.y, start.y);

    br.x = MIN(br.x, end.x);
    br.y = MIN(br.y, end.y);

    i->dim.x = br.x - tl.x;
    i->dim.y = br.y - tl.y;

    on_left_side = tl.x < center.x && br.x < center.x;
    on_top_side = tl.y < center.y && br.y < center.y;
    on_x_axis = tl.x < center.x && br.x > center.x;
    on_y_axis = tl.y < center.y && br.y > center.y;

    if (on_x_axis)
      zone = on_top_side ? COLLZONE_TOP : COLLZONE_BOTTOM;
    else if (on_y_axis)
      zone = on_left_side ? COLLZONE_LEFT : COLLZONE_RIGHT;
    else {
      if (on_top_side)
        zone = on_left_side ? COLLZONE_CORNER_TL : COLLZONE_CORNER_TR;
      else
        zone = on_left_side ? COLLZONE_CORNER_BL : COLLZONE_CORNER_BR;
    }

    if (i->dim.x > i->dim.y)
      type = COLLTYPE_VERTICAL;
    else if (i->dim.y > i->dim.x)
      type = COLLTYPE_HORIZONTAL;
    else
      type = abs_velocity.x > abs_velocity.y ? COLLTYPE_HORIZONTAL : COLLTYPE_VERTICAL;

    // Handle edge cases.
    if (current_state == STATE_JUMPING) {
      if (type == COLLTYPE_HORIZONTAL &&
          zone & (COLLZONE_TOP | COLLZONE_CORNER_TL | COLLZONE_CORNER_TR))
        type = COLLTYPE_VERTICAL;
      else if (type == COLLTYPE_VERTICAL &&
               zone & ~(COLLZONE_TOP | COLLZONE_CORNER_TL | COLLZONE_CORNER_TR))
        type = COLLTYPE_HORIZONTAL;
    }
    else if (current_state == STATE_FALLING) {
      if (type == COLLTYPE_VERTICAL &&
          zone & ~(COLLZONE_BOTTOM | COLLZONE_CORNER_BL | COLLZONE_CORNER_BR))
        type = COLLTYPE_HORIZONTAL;
      else if (type == COLLTYPE_HORIZONTAL &&
               zone & (COLLZONE_BOTTOM | COLLZONE_CORNER_BL | COLLZONE_CORNER_BR) &&
               abs_velocity.y > abs_velocity.x)
        type = COLLTYPE_VERTICAL;
    }

    // Set collision type.
    i->type = type;

    // If the player collided with a corner, adjust the player coordinates,
    // and recompute collisions.
    if (corner) {
      adjust_coords(i);
      goto recheck_collisions;
    }
  }

#ifdef DEBUG
  if (i->modifiers & COLLMOD_SOLID) {
    coll_tl = tl;
    coll_br = br;
  }
  else
    coll_tl = coll_br = (vec2i16) { .x = -1, .y = -1 };
#endif

  return collisions;
}

static state_t
collision_jumping(collision_info_t* info) {
  if (!(info->modifiers & COLLMOD_SOLID))
    return STATE_JUMPING;

  if (info->type == COLLTYPE_VERTICAL) {
    velocity.y = 0.0f;
    velocity.x *= KING_HIT_DAMPENING;
    return STATE_FALLING;
  }
  else /* info->type == COLLTYPE_HORIZONTAL */ {
    velocity.x = -velocity.x * KING_HIT_DAMPENING;
    hit_wall = 1;
    set_sprite(SPRITE_HITWALLMIDAIR);
  }

  return STATE_JUMPING;
}

static state_t
collision_falling(collision_info_t* info) {
  if (!(info->modifiers & COLLMOD_SOLID))
    return STATE_FALLING;

  if (info->type == COLLTYPE_VERTICAL)
    return fall_time > KING_MAX_FALL_TIME ? STATE_STUNNED : STATE_IDLE;
  else /* info->type == COLLTYPE == HORIZONTAL */ {
    velocity.x = -velocity.x * KING_HIT_DAMPENING;
    hit_wall = 1;
    set_sprite(SPRITE_HITWALLMIDAIR);
  }

  return STATE_FALLING;
}

static const collision_callback_t collision_callbacks[] = {
  [STATE_IDLE]     = NULL,
  [STATE_WALKING]  = NULL,
  [STATE_CHARGING] = NULL,
  [STATE_JUMPING]  = &collision_jumping,
  [STATE_FALLING]  = &collision_falling,
  [STATE_STUNNED]  = NULL
};

static ALWAYS_INLINE int
should_process_slope_collision(block_t block) {
  if (current_state == STATE_FALLING)
    return block <= BLOCK_SLOPE_TR;
  return LEVEL_BLOCK_ISSLOPE(block);
}

static ALWAYS_INLINE void
handle_collisions(level_screen_t* screen) {
  vec2f norm;
  float dot;
  state_t new_state;
  collision_callback_t collision_callback;
  collision_info_t info;
  int collisions, on_slope;

  collisions = check_for_collision(&info, screen);
  if (!collisions) {
    if (UNLIKELY(is_sliding_slope))
      is_sliding_slope = 0;
    return;
  }

  on_slope = should_process_slope_collision(info.block);
  // is_sliding = on_ground && !!(info.modifiers & COLLMOD_ICE);

  if (info.modifiers & COLLMOD_SOLID)
    adjust_coords(&info);

  if (UNLIKELY(on_slope)) {
    norm = slope_normals[info.block];
    dot = vec2f_dot(&velocity, &norm);
    if (!is_float_pos(dot)) {
      is_sliding_slope = 1;
      set_sprite(SPRITE_HITWALLMIDAIR);
      slope_dir = vec2f_ortho(&norm);
      adjust_velocity_on_slope();
      return;
    }
  } else if (UNLIKELY(is_sliding_slope))
    is_sliding_slope = 0;

  collision_callback = collision_callbacks[current_state];
  if (LIKELY(collision_callback != NULL)) {
    new_state = collision_callbacks[current_state](&info);
    if (LIKELY(new_state != current_state))
      switch_to_state(new_state);
  }
}

static ALWAYS_INLINE void
pre_update(level_screen_t* screen) {
  block_t block;
  vec2i16 map, end, cur = get_screen_coords();
  int on_blocks, on_slopes;

  cur.y += LEVEL_BLOCK_HALF;
  end = cur;
  cur.x -= KING_HITBOX_HALFW;
  end.x += KING_HITBOX_HALFW;

  on_blocks = on_slopes = 0;
  if (current_state != STATE_JUMPING && current_state != STATE_FALLING) {
    for (; cur.x < end.x; cur.x += LEVEL_BLOCK_SIZE) {
      map = LEVEL_COORDS_SCREEN2BLOCK(cur);
      block = screen->blocks[map.y][map.x];
      on_blocks += LEVEL_BLOCK_ISSOLID(block);
      on_slopes += LEVEL_BLOCK_ISSLOPE(block);
    }

    is_sliding_slope = on_slopes >= on_blocks;

    // Make the player slide if they step on a slope.
    if (UNLIKELY(is_sliding_slope))
      switch_to_state(STATE_FALLING);

    // TODO: Stop sliding if on ice and absf(velocity.x) < 0.001f
  }

  on_ground = on_blocks > 0;
  input_direction = !!(input.Buttons & PSP_CTRL_RIGHT) - !!(input.Buttons & PSP_CTRL_LEFT);
  jump_pressed = !!(input.Buttons & PSP_CTRL_CROSS);

  if (LIKELY(input_direction))
    last_input_direction = input_direction;
}

void
king_update(float delta, level_screen_t* screen, uint32_t* out_screen_index) {
  state_t new_state;

  pre_update(screen);

  while ((new_state = update_callbacks[current_state](delta, out_screen_index)) != current_state)
    switch_to_state(new_state);

  world_coords.x += velocity.x;
  world_coords.y += velocity.y;

  handle_collisions(screen);

  // Clamp the x-axis position of the player within the bounds of the screen.
  world_coords.x = clampf(world_coords.x, -((LEVEL_SCREEN_WIDTH - KING_HITBOX_WIDTH) >> 1), (LEVEL_SCREEN_WIDTH - KING_HITBOX_WIDTH) >> 1);

  // Check if the player has exited the current screen.
  if ((short)world_coords.y + KING_SPRITE_HALFH < 0) {
    --(*out_screen_index);
    world_coords.y += LEVEL_SCREEN_HEIGHT;
  }
  else if ((short)world_coords.y + KING_SPRITE_HALFH >= LEVEL_SCREEN_HEIGHT) {
    ++(*out_screen_index);
    world_coords.y -= LEVEL_SCREEN_HEIGHT;
  }
}

void
king_render(vec2i16* out_screen_coords, uint32_t current_scroll) {
  vertex_t* vertices;
  vec2i16 screen_coords = get_screen_coords();

  vertices = (vertex_t*)sceGuGetMemory(2 * sizeof(vertex_t));
  // Translate the player's level screen coordinates
  // to the PSP's screen coordinates.
  vertices[0].x = screen_coords.x - KING_SPRITE_HALFW;
  vertices[0].y = (screen_coords.y - current_scroll) - KING_SPRITE_HEIGHT;
  vertices[1].x = screen_coords.x + KING_SPRITE_HALFW;
  vertices[1].y = (screen_coords.y - current_scroll);
  // Set both vertices to have a depth of 1 so that the player's
  // sprite sits on top of the background.
  vertices[0].z = 1;
  vertices[1].z = 1;
  // Set the sprite's texture coordinates according to the direction
  // the player is currently facing.
  vertices[0].u = sprite_ucoord_offset;
  vertices[0].v = 0;
  vertices[1].u = KING_SPRITE_WIDTH - sprite_ucoord_offset;
  vertices[1].v = KING_SPRITE_HEIGHT;

  // Enable blending to account for transparency.
  sceGuEnable(GU_BLEND);
  sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  // Set the texture as the current selected sprite for the player.
  sceGuTexMode(GU_PSM_8888, 0, 0, GU_TRUE);
  sceGuTexImage(
      0,
      KING_SPRITE_WIDTH,
      KING_SPRITE_HEIGHT,
      KING_SPRITE_WIDTH,
      current_sprite);
  sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
  sceGuTexFilter(GU_NEAREST, GU_NEAREST);
  // Draw it!
  sceGuDrawArray(
      GU_SPRITES,
      GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
      2,
      NULL,
      vertices);
  // Disable blending since it's not needed anymore.
  sceGuDisable(GU_BLEND);

#ifdef DEBUG
  vertex_t *coll_vertices = sceGuGetMemory(2 * sizeof(vertex_t));

  coll_vertices[0].x = coll_tl.x;
  coll_vertices[0].y = coll_tl.y - current_scroll;
  coll_vertices[0].z = 2;
  coll_vertices[1].x = coll_br.x;
  coll_vertices[1].y = coll_br.y - current_scroll;
  coll_vertices[1].z = 2;

  sceGuDisable(GU_TEXTURE_2D);
  sceGuColor(0xFF0000FF);
  sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, coll_vertices);
  sceGuEnable(GU_TEXTURE_2D);
#endif

  // Output the previous' frame level screen coordinates.
  // This is needed by in the game's render function to paint
  // over the player sprite in the previous frame, which is
  // needed since we're not clearing the framebuffer every frame
  // to avoid having to re-render the entire midground texture
  // which is stored in RAM.
  *out_screen_coords = screen_coords;
}

void
king_destroy(void) {
  loader_unload_texture_vram(sprite_sheet);
}
