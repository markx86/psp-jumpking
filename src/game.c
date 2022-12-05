#include "state.h"
#include <string.h>

#define PLAYER_MAX_VSPEED 40
#define PLAYER_MAX_HSPEED 15

#define GAME_SCREEN_WIDTH (SCREEN_HEIGHT * 4 / 3)

static struct {
    short x, y;
    short vx, vy;
    short jumpPower;
    short direction;
    short inAir;
} Player;

static int currentScreen;

static void init(void) {
    currentScreen = 0;
    memset(&Player, 0, sizeof(Player));
}

static void update(long delta) {
    // Check which direction the player wants to go
    if (Input.Buttons & PSP_CTRL_RIGHT) {
        Player.direction = +1;
    } else if (Input.Buttons & PSP_CTRL_LEFT) {
        Player.direction = -1;
    } else {
        Player.direction = 0;
    }

    // If the player is on the ground
    if (!Player.inAir) {
        if ((Input.Buttons & PSP_CTRL_CROSS) && Player.jumpPower < PLAYER_MAX_VSPEED) {
            // If cross is pressed, build up jump power
            Player.jumpPower += 1;
            Player.vx = 0;
        } else if (Player.jumpPower) {
            // If some power was built up, jump
            Player.vx = Player.direction * PLAYER_MAX_HSPEED;
            Player.vy = Player.jumpPower;
            Player.jumpPower = 0;
            Player.inAir = 1;
        } else {
            // Otherwise move the player
            Player.vx = Player.direction * PLAYER_MAX_HSPEED;
        }
    }

    // Move the player
    Player.x += timesDelta(Player.vx, delta) / 10;
    Player.y += timesDelta(Player.vy, delta) / 6;

    // Apply gravity if in air
    if (Player.inAir && Player.vy > -PLAYER_MAX_VSPEED) {
        Player.vy -= 1;
    }

    // Check for y-collisions and scroll the screen
    if (Player.y < 0) {
        if (currentScreen > 0) {
            --currentScreen;
            Player.y = SCREEN_HEIGHT;
        } else {
            Player.vy = 0;
            Player.y = 0;
            Player.inAir = 0;
        }
    } else if (Player.y > SCREEN_HEIGHT) {
        ++currentScreen;
        Player.y = 0;
    }

    // Check for x-collisions
    if (Player.x < -GAME_SCREEN_WIDTH / 2) {
        Player.x = -GAME_SCREEN_WIDTH / 2;
        if (Player.inAir) {
            Player.vx = -Player.vx / 2;
        }
    } else if (Player.x > GAME_SCREEN_WIDTH / 2) {
        Player.x = GAME_SCREEN_WIDTH / 2;
        if (Player.inAir) {
            Player.vx = -Player.vx / 2;
        }
    }
}

static struct {
    unsigned short u, v;
    short x, y, z;
} __attribute__((packed)) vertices[2];

static void render(void) {
    short playerScreenX = Player.x + (SCREEN_WIDTH / 2);
    short playerScreenY = SCREEN_HEIGHT - Player.y;
    
    vertices[0].x = playerScreenX -8;
    vertices[0].y = playerScreenY -16;
    vertices[1].x = playerScreenX +8;
    vertices[1].y = playerScreenY;

    sceGuColor(0xFF0000FF);
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, vertices);
}

static void cleanup(void) {
    
}

const GameState IN_GAME = {
    .init = &init,
    .update = &update,
    .render = &render,
    .cleanup = &cleanup
};
