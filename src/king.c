#include "king.h"
#include "state.h"
#include <string.h>

#define PLAYER_MAX_FALL_TIME 3.0f

#define PLAYER_MAX_VSPEED 450.0f
#define PLAYER_MAX_HSPEED 100.0f
#define PLAYER_GRAVITY 13.5f

#define PLAYER_SPRITE_WIDTH 32
#define PLAYER_SPRITE_HEIGHT 32

#define PLAYER_GET_SPRITE(idx) (((char*) playerTilemap) + PLAYER_SPRITE_WIDTH * PLAYER_SPRITE_HEIGHT * 4 * (idx))

#define BORDER_OFFSET ((SCREEN_WIDTH - PLAYER_SPRITE_WIDTH) / 2.0f)

typedef enum {
    SPRITE_STANDING,
    SPRITE_WALKING0,
    SPRITE_WALKING1,
    SPRITE_WALKING2,
    SPRITE_CHARGING,
    SPRITE_JUMPING,
    SPRITE_FALLING,
    SPRITE_HITFLOOR,
    SPRITE_HITWALL,
} SpriteType;

typedef struct {
    float x, y;
    float vx, vy;
    float jumpPower;
    short direction;
    short inAir;
    short stunned;
} Player;

typedef struct {
    short u, v;
    short x, y, z;
} Vertex;

static Player player;
static void *playerTilemap;
static short spriteOffsetU, walkCycle;
static float fallTime;
static SpriteType spriteIndex;

static void createKing(void) {
    memset(&player, 0, sizeof(Player));
    playerTilemap = loadTextureVram("host0://assets/king/base/regular.qoi", NULL, NULL);
    spriteIndex = SPRITE_STANDING;
    spriteOffsetU = 0;
    walkCycle = 0;
    fallTime = 0.0f;
}

static void updateKing(float delta, int *currentScreen) {
    // If the player is on the ground
    if (!player.inAir) {
        short rightBias = ((Input.Buttons & PSP_CTRL_RIGHT) != 0) * 2 + ((Latch.uiBreak & PSP_CTRL_RIGHT) != 0);
        short leftBias = ((Input.Buttons & PSP_CTRL_LEFT) != 0) * 2 + ((Latch.uiBreak & PSP_CTRL_LEFT) != 0);
        // Check which direction the player wants to go
        if (rightBias > leftBias) {
            player.stunned = 0;
            player.direction = +1;
            // Don't flip sprite
            spriteOffsetU = 0;
        } else if (leftBias > rightBias) {
            player.stunned = 0;
            player.direction = -1;
            // Flip sprite
            spriteOffsetU = PLAYER_SPRITE_WIDTH;
        } else {
            player.direction = 0;
            walkCycle = 0;
        }
        
        if ((Input.Buttons & PSP_CTRL_CROSS) && player.jumpPower < PLAYER_MAX_VSPEED) {
            player.stunned = 0;
            player.vx = 0;
            // If cross is pressed, build up jump power
            player.jumpPower += PLAYER_MAX_VSPEED * 2.0f * delta;
        } else if (player.jumpPower) {
            // If some power was built up, jump
            player.vx = player.direction * PLAYER_MAX_HSPEED * 2.0f;
            player.vy = player.jumpPower;
            player.jumpPower = 0.0f;
            player.inAir = 1;
        } else if (!player.stunned) {
            // Otherwise move the player
            player.vx = player.direction * PLAYER_MAX_HSPEED;
        }
    }

    // Move the player
    player.x += player.vx * delta;
    player.y += player.vy * delta;

    // Apply gravity if in air
    if (player.inAir && player.vy > -PLAYER_MAX_VSPEED) {
        fallTime += delta;
        player.vy -= PLAYER_GRAVITY;
    }

    // Check for y-collisions and scroll the screen
    if (player.y < 0.0f) {
        if (*currentScreen > 0) {
            --(*currentScreen);
            player.y = SCREEN_HEIGHT;
        } else {
            if (fallTime > PLAYER_MAX_FALL_TIME) {
                player.stunned = 1;
            } else {
                player.stunned = 0;
            }
            player.vx = 0.0f;
            player.vy = 0.0f;
            player.y = 0.0f;
            fallTime = 0.0f;
            player.inAir = 0;
        }
    } else if (player.y > SCREEN_HEIGHT) {
        ++(*currentScreen);
        player.y = 0.0f;
    }

    // Check for x-collisions
    if (player.x < -BORDER_OFFSET) {
        player.x = -BORDER_OFFSET;
        if (player.inAir) {
            player.vx = -(player.vx / 2.0f);
            player.stunned = 1;
        }
    } else if (player.x > BORDER_OFFSET) {
        player.x = BORDER_OFFSET;
        if (player.inAir) {
            player.vx = -(player.vx / 2.0f);
            player.stunned = 1;
        }
    }

    // Update animation state
    {
        if (player.jumpPower) {
            spriteIndex = SPRITE_CHARGING;
        } else if (player.stunned) {
            spriteIndex = (player.vy) ? SPRITE_HITWALL : SPRITE_HITFLOOR;
        } else if (player.vy) {
            spriteIndex = (player.vy > 0.0f) ? SPRITE_JUMPING : SPRITE_FALLING;
        } else if (player.vx && player.direction) {
            walkCycle += 1;
            switch (walkCycle / 4) {
                case 0:
                case 1:
                case 2:
                    spriteIndex = SPRITE_WALKING0;
                    break;
                case 3:
                    spriteIndex = SPRITE_WALKING1;
                    break;
                case 4:
                case 5:
                case 6:
                    spriteIndex = SPRITE_WALKING2;
                    break;
                case 7:
                    spriteIndex = SPRITE_WALKING1;
                    break;
                default:
                    walkCycle = 0;
                    break;
            }
        } else {
            spriteIndex = SPRITE_STANDING;
        }
    }
}

static void renderKing(void) {
    short playerScreenX = ((short) player.x) + (SCREEN_WIDTH / 2);
    short playerScreenY = SCREEN_HEIGHT - ((short) player.y);

    Vertex *vertices = (Vertex*) sceGuGetMemory(2 * sizeof(Vertex));
    vertices[0].x = playerScreenX - (PLAYER_SPRITE_WIDTH / 2);
    vertices[0].y = playerScreenY - PLAYER_SPRITE_HEIGHT;
    vertices[0].u = spriteOffsetU;
    vertices[0].v = 0;
    vertices[1].x = playerScreenX + (PLAYER_SPRITE_WIDTH / 2);
    vertices[1].y = playerScreenY;
    vertices[1].u = PLAYER_SPRITE_WIDTH - spriteOffsetU;
    vertices[1].v = PLAYER_SPRITE_HEIGHT;

    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexImage(0, PLAYER_SPRITE_WIDTH, PLAYER_SPRITE_HEIGHT, PLAYER_SPRITE_WIDTH, PLAYER_GET_SPRITE(spriteIndex));
    sceGuTexFunc(GU_TFX_ADD, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, vertices);
}

static void destroyKing(void) {
    unloadTextureVram(playerTilemap);
};

const King king = {
    .create = &createKing,
    .update = &updateKing,
    .render = &renderKing,
    .destroy = &destroyKing
};
