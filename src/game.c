#include "state.h"
#include "loader.h"
#include <string.h>

#define PLAYER_MAX_VSPEED 40
#define PLAYER_MAX_HSPEED 15

#define GAME_SCREEN_WIDTH (SCREEN_HEIGHT * 4 / 3)

typedef struct {
    short x, y;
    short vx, vy;
    short jumpPower;
    short direction;
    short inAir;
} Player;

typedef struct {
    short u, v;
    short x, y, z;
} __attribute__((packed)) Vertex;

static Player player;

static int currentScreen;
static void *playerTilemap;
static short spriteOffsetU = 0;

static void init(void) {
    currentScreen = 0;
    memset(&player, 0, sizeof(Player));
    playerTilemap = loadTexture("host0://assets/textures/king/base/regular.qoi");
}

static void update(long delta) {
    // Check which direction the player wants to go
    if (Input.Buttons & PSP_CTRL_RIGHT) {
        player.direction = +1;
        spriteOffsetU = 0;
    } else if (Input.Buttons & PSP_CTRL_LEFT) {
        player.direction = -1;
        spriteOffsetU = 32;
    } else {
        player.direction = 0;
    }

    // If the player is on the ground
    if (!player.inAir) {
        if ((Input.Buttons & PSP_CTRL_CROSS) && player.jumpPower < PLAYER_MAX_VSPEED) {
            // If cross is pressed, build up jump power
            player.jumpPower += 1;
            player.vx = 0;
        } else if (player.jumpPower) {
            // If some power was built up, jump
            player.vx = player.direction * PLAYER_MAX_HSPEED;
            player.vy = player.jumpPower;
            player.jumpPower = 0;
            player.inAir = 1;
        } else {
            // Otherwise move the player
            player.vx = player.direction * PLAYER_MAX_HSPEED;
        }
    }

    // Move the player
    player.x += timesDelta(player.vx, delta) / 10;
    player.y += timesDelta(player.vy, delta) / 6;

    // Apply gravity if in air
    if (player.inAir && player.vy > -PLAYER_MAX_VSPEED) {
        player.vy -= 1;
    }

    // Check for y-collisions and scroll the screen
    if (player.y < 0) {
        if (currentScreen > 0) {
            --currentScreen;
            player.y = SCREEN_HEIGHT;
        } else {
            player.vy = 0;
            player.y = 0;
            player.inAir = 0;
        }
    } else if (player.y > SCREEN_HEIGHT) {
        ++currentScreen;
        player.y = 0;
    }

    // Check for x-collisions
    if (player.x < -GAME_SCREEN_WIDTH / 2) {
        player.x = -GAME_SCREEN_WIDTH / 2;
        if (player.inAir) {
            player.vx = -player.vx / 2;
        }
    } else if (player.x > GAME_SCREEN_WIDTH / 2) {
        player.x = GAME_SCREEN_WIDTH / 2;
        if (player.inAir) {
            player.vx = -player.vx / 2;
        }
    }
}

static void render(void) {
    // Draw player
    {
        short playerScreenX = player.x + (SCREEN_WIDTH / 2);
        short playerScreenY = SCREEN_HEIGHT - player.y;

        Vertex *vertices = (Vertex*) sceGuGetMemory(2 * sizeof(Vertex));
        vertices[0].x = playerScreenX -16;
        vertices[0].y = playerScreenY -32;
        vertices[0].u = spriteOffsetU;
        vertices[0].v = 0;
        vertices[1].x = playerScreenX +16;
        vertices[1].y = playerScreenY;
        vertices[1].u = 32 - spriteOffsetU;
        vertices[1].v = 32;

        sceGuTexMode(GU_PSM_8888, 0, 0, 0);
        sceGuTexImage(0, 32, 32, 32, playerTilemap);
        sceGuTexFunc(GU_TFX_ADD, GU_TCC_RGBA);
        sceGuTexFilter(GU_LINEAR, GU_LINEAR);
        sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, vertices);
        //sceGuColor(0xFF0000FF);
        //sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, vertices);
    }
}

static void cleanup(void) {
    unloadTexture(playerTilemap);
}

const GameState IN_GAME = {
    .init = &init,
    .update = &update,
    .render = &render,
    .cleanup = &cleanup
};
