#include "king.h"
#include "state.h"
#include <string.h>

// Hitbox sizes
#define KING_HITBOX_WIDTH 18
#define KING_HITBOX_HEIGHT 26
#define KING_HITBOX_HALFW (KING_HITBOX_WIDTH / 2)
#define KING_HITBOX_HALFH (KING_HITBOX_HEIGHT / 2)
#define KING_HITBOX_BLOCK_WIDTH LEVEL_COORDS_SCREEN2MAP(KING_HITBOX_WIDTH)
#define KING_HITBOX_BLOCK_HEIGHT LEVEL_COORDS_SCREEN2MAP(KING_HITBOX_HEIGHT)
#define KING_HITBOX_BLOCK_HALFW (KING_HITBOX_BLOCK_WIDTH / 2)
#define KING_HITBOX_BLOCK_HALFH (KING_HITBOX_BLOCK_HEIGHT / 2)

// Physics constants
#define KING_JUMP_HEIGHT 153.0f
#define KING_JUMP_VSPEED 9.0f
#define KING_JUMP_HSPEED 3.5f
#define KING_WALK_SPEED 1.5f
#define KING_WALL_BOUNCE 0.45f
#define KING_GRAVITY 0.275f
#define KING_MAX_FALL_SPEED 10.0f

// Status constants
#define KING_CHARGE_TIME 0.6f
#define KING_STUN_TIME 0.5f
#define KING_MAX_FALL_TIME 1.0f

// Input constants
#define KING_JUMP_LENIENCY_FRAMES 4

// Macros
#define KING_GET_SPRITE(idx) \
  (all_sprites + KING_SPRITE_WIDTH * KING_SPRITE_HEIGHT * 4 * (idx))

// Player sprites
typedef enum {
  SPRITE_STANDING,
  SPRITE_WALKING0,
  SPRITE_WALKING1,
  SPRITE_WALKING2,
  SPRITE_CHARGING,
  SPRITE_JUMPING,
  SPRITE_FALLING,
  SPRITE_STUNNED,
  SPRITE_HITWALLMIDAIR,
} sprite_index_t;

// Collision modifiers
typedef enum {
  COLLMOD_NONE = 0,
  COLLMOD_NOWIND = 1,
  COLLMOD_WATER = 2,
  COLLMOD_SAND = 4,
  COLLMOD_QUARK = 8,
  COLLMOD_ICE = 16,
  COLLMOD_SNOW = 32,
  COLLMOD_SOLID = 64,
  COLLMOD_SLOPE = 128,
} collision_modifier_t;

typedef struct {
  // the collision modifiers
  union {
    uint8_t modifiers;
    struct {
      uint8_t no_wind : 1;
      uint8_t is_water : 1;
      uint8_t is_sand : 1;
      uint8_t is_quark : 1;
      uint8_t is_ice : 1;
      uint8_t is_snow : 1;
      uint8_t is_solid : 1;
      uint8_t is_slope : 1;
    };
  };
  // the block the player has collided with
  level_screen_block_t block;
  // screen coordinates of the intersection rectangle's top-left corner
  short tl_sx, tl_sy;
  // screen coordinates of the intersection rectangle's bottom-right corner
  short br_sx, br_sy;
  // width and height of the intersection rectangle
  short width, height;
} collision_info_t;

// Coordinates
static float world_x, world_y;
static float velocity_x, velocity_y;
static short screen_x, screen_y;
// Input
static short direction, jump_pressed, leniency_frames, leniency_direction;
// Flags
static char in_air, hit_wall_midair, max_jump_power_reached, is_stunned;
// Status
static float jump_power, stun_time, fall_time;
// Graphics
static short walk_anim_cycle, sprite_ucoord_offset;
static sprite_index_t current_sprite_index;
static char *current_sprite, *all_sprites;

static char block_collision_data[] = {
    COLLMOD_SOLID | COLLMOD_SLOPE,
    COLLMOD_SOLID | COLLMOD_SLOPE,
    COLLMOD_SOLID | COLLMOD_SLOPE,
    COLLMOD_SOLID | COLLMOD_SLOPE,
    COLLMOD_NONE,
    COLLMOD_SOLID,
    COLLMOD_NONE,
    COLLMOD_SOLID | COLLMOD_ICE,
    COLLMOD_SOLID | COLLMOD_SNOW,
    COLLMOD_SAND,
    COLLMOD_NOWIND,
    COLLMOD_WATER,
    COLLMOD_QUARK};

static float slope_normals[4][2] =
    {{-1.0f, -1.0f}, {+1.0f, -1.0f}, {-1.0f, +1.0f}, {+1.0f, +1.0f}};

static short
check_collisions(
    short sx,
    short sy,
    level_screen_t* screen,
    collision_info_t* info) {
  short collisions;
  short positive_direction_x, positive_direction_y;
  short border_x, border_y;
  short min_dist2, dist2;
  short ox, oy, mx, my, cx, cy, csx, csy;
  level_screen_block_t block;

  sy -= KING_HITBOX_HALFH;
  collisions = 0;

  positive_direction_x = velocity_x > 0.0f;
  positive_direction_y = velocity_y < 0.0f;

  border_x =
      sx + ((positive_direction_x) ? +KING_HITBOX_HALFW : -KING_HITBOX_HALFW);
  border_y =
      sy + ((positive_direction_y) ? +KING_HITBOX_HALFH : -KING_HITBOX_HALFH);

  info->tl_sx = (positive_direction_x) ? -1 : border_x;
  info->tl_sy = (positive_direction_y) ? -1 : border_y;
  info->br_sx = (positive_direction_x) ? border_x : -1;
  info->br_sy = (positive_direction_y) ? border_y : -1;

  min_dist2 = -1;
  for (oy = -KING_HITBOX_HALFH; oy < KING_HITBOX_HALFH;
       oy += LEVEL_BLOCK_SIZE) {
    for (ox = -KING_HITBOX_HALFW; ox < KING_HITBOX_HALFW;
         ox += LEVEL_BLOCK_SIZE) {
      cx = sx + ox;
      cy = sy + oy;
      mx = LEVEL_COORDS_SCREEN2MAP(cx);
      my = LEVEL_COORDS_SCREEN2MAP(cy);
      if (mx < 0 || mx >= LEVEL_SCREEN_BLOCK_WIDTH)
        continue;
      if (my < 0 || my >= LEVEL_SCREEN_BLOCK_HEIGHT)
        continue;

      block = screen->blocks[my][mx];
      info->modifiers |= block_collision_data[block];
      if (LEVEL_BLOCK_ISSOLID(block)) {
        ++collisions;
        csx = LEVEL_COORDS_MAP2SCREEN(mx);
        csy = LEVEL_COORDS_MAP2SCREEN(my);

        if (positive_direction_x)
          info->tl_sx =
              (info->tl_sx < 0 || csx < info->tl_sx) ? csx : info->tl_sx;
        else
          info->br_sx = (info->br_sx < 0 || csx > info->br_sx)
                            ? csx + LEVEL_BLOCK_SIZE
                            : info->br_sx;

        if (positive_direction_y)
          info->tl_sy =
              (info->tl_sy < 0 || csy < info->tl_sy) ? csy : info->tl_sy;
        else
          info->br_sy = (info->br_sy < 0 || csy > info->br_sy)
                            ? csy + LEVEL_BLOCK_SIZE
                            : info->br_sy;

        dist2 = ox * ox + oy * oy;
        if (min_dist2 < 0 || dist2 < min_dist2) {
          min_dist2 = dist2;
          info->block = block;
        }
      }
    }
  }

  info->width = info->br_sx - info->tl_sx;
  info->height = info->br_sy - info->tl_sy;

  return collisions;
}

static void
do_collision(float new_x, float new_y, level_screen_t* screen) {
  short new_sx, new_sy;
  short collisions;
  short was_vertical_collision, was_diagonal_collision;
  short abs_vx, abs_vy;
  short vertical_direction, horizontal_direction;
  collision_info_t info;

  // Convert new player position to screen coordinates.
  new_sx = ((short)new_x) + (LEVEL_SCREEN_WIDTH / 2);
  new_sy = LEVEL_SCREEN_HEIGHT - ((short)new_y);

  // Check if the player has collided with something.
  collisions = check_collisions(new_sx, new_sy, screen, &info);
  if (collisions) {
    // If they have, check if the collision was along the X or the Y axis.
    was_diagonal_collision = info.width == info.height;
    abs_vx = (velocity_x > 0.0f) ? velocity_x : -velocity_x; // |velocity_x|
    abs_vy = (velocity_y > 0.0f) ? velocity_y : -velocity_y; // |velocity_y|
    if (info.width > info.height ||
        (was_diagonal_collision && abs_vy >= abs_vx)) {
      // A collision is vertical if the intersection rectangle
      // is wider than it is tall. If the intersection is a square,
      // the collision is considered vertical if the Y component of the
      // velocity vector is greater (or equal) than its X component.
      was_vertical_collision = 1;
      vertical_direction = (velocity_y < 0.0f) ? +1 : -1;
      new_sy -= vertical_direction * info.height;
      new_y = (float)(LEVEL_SCREEN_HEIGHT - new_sy);
    } else if (
        info.width < info.height ||
        (was_diagonal_collision && abs_vx > abs_vy)) {
      // A collision is horizontal if the intersection rectangle
      // is taller than it is wide. If the intersection is a square,
      // the collision is considered horizontal if the X component
      // of the velocity vector is greater than its Y component.
      was_vertical_collision = 0;
      if (info.width > LEVEL_BLOCK_SIZE)
        info.width = LEVEL_BLOCK_SIZE;
      horizontal_direction = (velocity_x > 0.0f) ? +1 : -1;
      new_sx -= horizontal_direction * info.width;
      new_x = (float)(new_sx - (short)(LEVEL_SCREEN_WIDTH / 2));
    }

    // TODO: Better collision handling.
    in_air = in_air && (!was_vertical_collision || velocity_y > 0.0f);
    is_stunned = !info.is_slope && !in_air && fall_time > KING_MAX_FALL_TIME;
    stun_time = is_stunned * KING_STUN_TIME;
    hit_wall_midair = (in_air || velocity_y > 0.0f) && !was_vertical_collision;
    if (info.is_slope || (was_vertical_collision && velocity_y > 0.0f))
      fall_time = 0.0f;
    velocity_x = (!was_vertical_collision || velocity_y > 0.0f) * -velocity_x *
                 KING_WALL_BOUNCE;
    velocity_y *= !was_vertical_collision;
  }

  // Update physics
  {
    // Update player position.
    world_x = new_x;
    world_y = new_y;
  }

  // Update graphics
  {
    // Update screeen coordinates.
    screen_x = new_sx;
    screen_y = new_sy;
  }
}

void
king_create(void) {
  all_sprites =
      loader_load_texture_vram("assets/king/base/regular.qoi", NULL, NULL);
  // Set the player starting position.
  world_x = 0.0f;
  world_y = 32.0f;
  // Set the player initial speed.
  velocity_x = 0.0f;
  velocity_y = 0.0f;
  // Update the player screen coordinates.
  screen_x = ((short)world_x) + (LEVEL_SCREEN_WIDTH / 2);
  screen_y = LEVEL_SCREEN_HEIGHT - ((short)world_y);
  // Reset input.
  direction = 0;
  jump_pressed = 0;
  leniency_frames = 0;
  leniency_direction = 0;
  // Reset flags.
  in_air = 0;
  hit_wall_midair = 0;
  max_jump_power_reached = 0;
  is_stunned = 1;
  // Reset status.
  jump_power = 0.0f;
  stun_time = 0.0f;
  fall_time = 0.0f;
  // Reset animation.
  walk_anim_cycle = 0;
  sprite_ucoord_offset = 0;
  // Set the initial sprite.
  current_sprite_index = SPRITE_STUNNED;
  current_sprite = KING_GET_SPRITE(current_sprite_index);
}

void
king_update(float delta, level_screen_t* screen, uint32_t* out_screen_index) {
  int map_x, map_y;
  int ix;
  float new_x, new_y;
  level_screen_block_t block;
  sprite_index_t new_sprite_index;

  // Update status
  {
    if (velocity_y) {
      // If the vertical velocity is not zero,
      // the player is in the air.
      in_air = 1;
      if (velocity_y > 0.0f) {
        // If the player is jumping up (meaning the vertical velocity is
        // positive), reset the jump power.
        // NOTE: This is there because the
        // player can be falling,
        //       and still be on a solid block (eg. sand block).
        // TODO: This still needs to be implemented properly.
        jump_power = 0.0f;
        // Also reset the fall time.
        fall_time = 0.0f;
      } else if (fall_time < KING_MAX_FALL_TIME)
        // If the player is falling (meaning the vertical velocity is negative),
        // count up the fall time.
        fall_time += delta;
    } else {
      // Check if the player is standing on solid ground.
      map_x = LEVEL_COORDS_SCREEN2MAP(screen_x);
      map_y = LEVEL_COORDS_SCREEN2MAP(screen_y);
      for (ix = -KING_HITBOX_BLOCK_HALFW; ix <= KING_HITBOX_BLOCK_HALFW; ix++) {
        block = screen->blocks[map_y][map_x + ix];
        in_air |= LEVEL_BLOCK_ISSOLID(block) && !LEVEL_BLOCK_ISSLOPE(block);
      }
      in_air = !in_air;

      if (in_air) {
        // Update physics
        velocity_x = direction * KING_WALK_SPEED;

        // Update status
        fall_time = 0.0f;
      }
    }
  }

  if (!in_air) {
    // If the player is on the ground...

    // Resolve input
    {
      // Check which direction the player wants to go.
      direction = (input.Buttons & PSP_CTRL_LEFT)    ? -1
                  : (input.Buttons & PSP_CTRL_RIGHT) ? +1
                                                     : 0;
      // Handle the leniency direction.
      if (direction) {
        // If the direction is non 0, reset the leniency time.
        leniency_frames = KING_JUMP_LENIENCY_FRAMES;
        leniency_direction = direction;
      } else
        // Otherwise, count down the leniency time.
        --leniency_frames;
      // Check if the player is trying to jump.
      jump_pressed = (input.Buttons & PSP_CTRL_CROSS);
    }

    // Update status
    {
      // Update stunned timer
      if (stun_time > 0)
        stun_time -= delta;

      // Un-stun the player if input was recieved
      // and the cooldown time has elapsed.
      if (is_stunned && stun_time <= 0 && (jump_pressed || direction))
        is_stunned = 0;

      // Check if the player is pressing the jump button
      // and, if true, build up jump power.
      if (!is_stunned && jump_pressed) {
        jump_power += (KING_JUMP_VSPEED / KING_CHARGE_TIME) * delta;
        max_jump_power_reached = jump_power >= KING_JUMP_VSPEED;
      }
    }

    // Update physics
    {
      if (!is_stunned) {
        if (jump_pressed && !max_jump_power_reached)
          // Freeze the player if the jump button is pressed.
          velocity_x = 0.0f;
        else {
          if (jump_power) {
            // If the jump button was released...
            // - set the vertical speed to the jump power that was built up
            velocity_y = jump_power;
            // - check if the input direction is 0. If it is, check if the
            //   last non 0 direction was within the last two frames
            if (leniency_frames > 0 && direction == 0)
              // - if it was, override the player's direction
              direction = leniency_direction;
            // - set the horizontal speed to KING_JUMP_HSPEED in the direction
            //   the player is facing
            velocity_x = direction * KING_JUMP_HSPEED;
          } else
            // Otherwise, walk at normal speed.
            velocity_x = direction * KING_WALK_SPEED;
        }
      }
    }
  } else {
    // If the player is in the air...

    // Update physics
    {
      // If the falling terminal velocity has not been reached,
      // apply gravity.
      if (velocity_y > -KING_MAX_FALL_SPEED)
        velocity_y -= KING_GRAVITY;
    }
  }

  // Compute new player position.
  new_x = world_x + velocity_x;
  new_y = world_y + velocity_y;

  // If the player is within the leve screen bounds,
  // handle collisions.
  do_collision(new_x, new_y, screen);

  if (screen_y - KING_HITBOX_HALFH < 0) {
    // If the player has left the screen from the top side...
    screen_y += LEVEL_SCREEN_HEIGHT;
    world_y -= LEVEL_SCREEN_HEIGHT;
    *out_screen_index += 1;
  } else if (screen_y - KING_HITBOX_HALFH >= LEVEL_SCREEN_HEIGHT) {
    // If the player has left the screen from the bottom side...
    screen_y -= LEVEL_SCREEN_HEIGHT;
    world_y += LEVEL_SCREEN_HEIGHT;
    *out_screen_index -= 1;
  } else if (screen_x < 0) {
    // If the player has left the screen from the left side...
    screen_x += LEVEL_SCREEN_WIDTH;
    world_x += LEVEL_SCREEN_WIDTH;
    *out_screen_index = screen->teleport_index;
  } else if (screen_x > LEVEL_SCREEN_WIDTH) {
    // If the player has left the screen from the right side...
    screen_x -= LEVEL_SCREEN_WIDTH;
    world_x -= LEVEL_SCREEN_WIDTH;
    *out_screen_index = screen->teleport_index;
  }

  // Update graphics
  {
    // If the player is not stunned...
    if (!is_stunned) {
      // Flip the sprite according to the player direction.
      if (direction == +1)
        sprite_ucoord_offset = 0;
      else if (direction == -1)
        sprite_ucoord_offset = KING_SPRITE_WIDTH;
    }

    // Find the appropriate sprite index for the current frame.
    if (jump_power)
      new_sprite_index = SPRITE_CHARGING;
    else if (is_stunned)
      new_sprite_index = SPRITE_STUNNED;
    else if (hit_wall_midair)
      new_sprite_index = SPRITE_HITWALLMIDAIR;
    else if (in_air)
      new_sprite_index = (velocity_y > 0.0f) ? SPRITE_JUMPING : SPRITE_FALLING;
    else if (velocity_x && direction) {
      walk_anim_cycle += 1;
      switch (walk_anim_cycle / 4) {
        case 0:
        case 1:
        case 2:
          new_sprite_index = SPRITE_WALKING0;
          break;
        case 3:
          new_sprite_index = SPRITE_WALKING1;
          break;
        case 4:
        case 5:
        case 6:
          new_sprite_index = SPRITE_WALKING2;
          break;
        case 7:
          new_sprite_index = SPRITE_WALKING1;
          walk_anim_cycle = 0;
          break;
      }
    } else {
      new_sprite_index = SPRITE_STANDING;
      walk_anim_cycle = 0;
    }

    // Update sprite pointer if the new sprite is different
    // from the previous one.
    if (current_sprite_index != new_sprite_index) {
      current_sprite_index = new_sprite_index;
      current_sprite = KING_GET_SPRITE(current_sprite_index);
    }
  }
}

void
king_render(short* out_sx, short* out_sy, uint32_t current_scroll) {
  vertex_t* vertices;

  vertices = (vertex_t*)sceGuGetMemory(2 * sizeof(vertex_t));
  // Translate the player's level screen coordinates
  // to the PSP's screen coordinates.
  vertices[0].x = screen_x - KING_SPRITE_HALFW;
  vertices[0].y = (screen_y - current_scroll) - KING_SPRITE_HEIGHT;
  vertices[1].x = screen_x + KING_SPRITE_HALFW;
  vertices[1].y = (screen_y - current_scroll);
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
  sceGuTexMode(GU_PSM_8888, 0, 0, GU_FALSE);
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

  // Output the previous' frame level screen coordinates.
  // This is needed by in the game's render function to paint
  // over the player sprite in the previous frame, which is
  // needed since we're not clearing the framebuffer every frame
  // to avoid having to re-render the entire midground texture
  // which is stored in RAM.
  *out_sx = screen_x;
  *out_sy = screen_y;
}

void
king_destroy(void) {
  loader_unload_texture_vram(all_sprites);
}
