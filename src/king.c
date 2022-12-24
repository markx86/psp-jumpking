#include "king.h"
#include "state.h"
#include <string.h>

// Physics constants
#define PLAYER_MAX_VSPEED 450.0f
#define PLAYER_MAX_HSPEED 100.0f
#define PLAYER_GRAVITY 13.5f

// Status constants
#define PLAYER_CHARGING_SPEED (PLAYER_MAX_VSPEED * 2.0f)
#define PLAYER_STUN_TIME 5.5f
#define PLAYER_MAX_FALL_HEIGHT (SCREEN_HEIGHT / 2.0f)

#define PLAYER_MAP_COORDS(px, py) { ((px) / LEVEL_BLOCK_WIDTH), ((py) / LEVEL_BLOCK_HEIGHT)}
#define PLAYER_GET_SPRITE(idx) (player.graphics.sprites + PLAYER_SPRITE_WIDTH * PLAYER_SPRITE_HEIGHT * 4 * (idx))
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
} SpriteIndex;

typedef struct {
    float x, y;
    float vx, vy;
} PlayerPhysics;

typedef struct {
    short direction;
    short jumping;
} PlayerInput;

typedef struct {
    char inAir : 1;
    char hitWall : 1;
    char hitFloor : 1;
    char maxJumpPower : 1;
    char stunned : 1;
    char reserved : 3;
    float jumpPower;
    float fallDistance;
    float stunTime;
} PlayerStatus;

typedef struct {
    short sx;
    short sy;
    short walkAnimCycle;
    short spriteUOffset;
    SpriteIndex spriteIndex;
    char *sprites;
    char *sprite;
} PlayerGraphics;

typedef struct {
    PlayerInput input;
    PlayerStatus status;
    PlayerPhysics physics;
    PlayerGraphics graphics;
} Player;

static Player player;

void kingCreate(void) {
    memset(&player, 0, sizeof(Player));
    player.graphics.sprites = loadTextureVram("host0://assets/king/base/regular.qoi", NULL, NULL);
    player.graphics.spriteIndex = SPRITE_HITFLOOR;
    player.graphics.sprite = PLAYER_GET_SPRITE(player.graphics.spriteIndex);
}

void kingUpdate(float delta, LevelScreen *screen) {
    // Update status
    {
        if (player.physics.vy) {
            // If the vertical velocity is not zero,
            // the player is in the air.
            player.status.inAir = 1;
            // If the player is jumping up, reset the jump power.
            // (NOTE) This is there because the player can be falling,
            //        and still be on a solid block (eg. sand block).
            // (TODO) This still needs to be implemented properly.
            if (player.physics.vy > 0.0f) {
                player.status.jumpPower = 0.0f;
            }
        } else {
            player.status.inAir = 0;
        }
    }

    if (!player.status.inAir) {
        // If the player is on the ground...
        
        // Resolve input
        {
            // Check which direction the player wants to go.
            short rightBias = (((Input.Buttons & PSP_CTRL_RIGHT) != 0) << 1) | ((Latch.uiBreak & PSP_CTRL_RIGHT) != 0);
            short leftBias = (((Input.Buttons & PSP_CTRL_LEFT) != 0) << 1) | ((Latch.uiBreak & PSP_CTRL_LEFT) != 0);
            player.input.direction = (leftBias > rightBias) ? -1 : (leftBias < rightBias) ? +1 : 0;
            // Check if the player is trying to jump.
            player.input.jumping = (Input.Buttons & PSP_CTRL_CROSS);
        }

        // Update status
        {
            // Update stunned status
            if (player.status.stunned) {
                if (player.status.stunTime < PLAYER_STUN_TIME) {
                    player.status.stunTime += delta;
                } else {
                    player.status.stunned = 0;
                    player.status.stunTime = PLAYER_STUN_TIME;
                }
            }
            // Un-stun the player if input was recieved
            // and the cooldown time has elapsed.
            if (!player.status.stunned && (player.input.jumping || player.input.direction)) {
                player.status.hitFloor = 0;
            }
            // Check if the player is pressing the jump button
            // and, if true, build up jump power.
            if (player.input.jumping) {
                player.status.jumpPower += PLAYER_CHARGING_SPEED * delta;
                player.status.maxJumpPower = player.status.jumpPower >= PLAYER_MAX_VSPEED;
            }
        }

        // Update physics
        {
            if (!player.status.stunned) {
                if (player.input.jumping && !player.status.maxJumpPower) {
                    // Freeze the player if the jump button is pressed.
                    player.physics.vx = 0.0f;
                } else {
                    // If the jump button is not pressed, move the player.
                    player.physics.vx = player.input.direction * PLAYER_MAX_HSPEED;
                    if (player.status.jumpPower) {
                        // If the jump button was released...
                        // - set the vertical speed to the jump power that was built up
                        player.physics.vy = player.status.jumpPower;
                        // - double the horizontal speed
                        player.physics.vx *= 2.0f;
                    }
                }
            }
        }
    } else {
        // If the player is in the air...

        // Update physics
        {
            // If the falling terminal velocity has not been reached,
            // apply gravity.
            if (player.physics.vy > -PLAYER_MAX_VSPEED) {
                player.physics.vy -= PLAYER_GRAVITY;
            }
        }

        // Update status
        {
            if (player.status.fallDistance > PLAYER_MAX_FALL_HEIGHT) {
                // If the maximum fall time was reached,
                // the player will be stunned when they hit the floor.
                player.status.stunned = 1;
                player.status.fallDistance = 0.0f;
            } else if (!player.status.stunned && player.physics.vy) {
                // If the player is falling and the fall time has
                // not reached its maximum value, count up the fall time.
                player.status.fallDistance += player.physics.vy * delta;
            }
        }
    }

    //// Compute new player position
    //float newX = player.physics.x + player.physics.vx * delta;
    //float newY = player.physics.y + player.physics.vy * delta;
    //// Compute new screen position
    //short newSX = ((short) newX) + (LEVEL_SCREEN_PXWIDTH / 2);
    //short newSY = LEVEL_SCREEN_PXHEIGHT - ((short) newY);
    //
    //LevelScreenBlock block;
//
    //short coordsTL[] = PLAYER_MAP_COORDS(newSX - PLAYER_SPRITE_HALFW, newSY - PLAYER_SPRITE_HALFH);
    //short coordsTR[] = PLAYER_MAP_COORDS(newSX + PLAYER_SPRITE_HALFW, newSY - PLAYER_SPRITE_HALFH);
    //short coordsBL[] = PLAYER_MAP_COORDS(newSX - PLAYER_SPRITE_HALFW, newSY + PLAYER_SPRITE_HALFH);
    //short coordsBR[] = PLAYER_MAP_COORDS(newSX + PLAYER_SPRITE_HALFW, newSY + PLAYER_SPRITE_HALFH);
    //LevelScreenBlock blockTL = screen->blocks[coordsTL[1]][coordsTL[0]];
    //LevelScreenBlock blockTR = screen->blocks[coordsTR[1]][coordsTR[0]];
    //LevelScreenBlock blockBL = screen->blocks[coordsBL[1]][coordsBL[0]];
    //LevelScreenBlock blockBR = screen->blocks[coordsBR[1]][coordsBR[0]];
//
    //LevelScreenBlock blockT = (blockTL) ? blockTL : blockTR;
    //LevelScreenBlock blockB = (blockBL) ? blockBL : blockBR;
    //block = blockT | blockB;
    //if (block && block != BLOCK_FAKE) {
    //    if (player.status.inAir) {
    //        if (blockB) {
    //            // Update status
    //            if (player.status.stunned) {
    //                player.status.hitFloor = 1;
    //            }
    //            player.status.hitWall = 0;
    //            player.status.inAir = 0;
    //            // Update physics
    //            player.physics.vy = 0.0f;
    //            player.physics.vx = 0.0f;
    //        } else {
    //            // Update physics
    //            player.physics.vy *= -0.5f;
    //        }
    //        player.physics.y = (float)(((short) newY));
    //    }
    //} else {
    //    // Update status
    //    player.status.inAir = 1;
    //    // Update physics
    //    player.physics.y = newY;
//
    //    // If the player is jumping up, reset the jump power
    //    // (NOTE) This is there because the player can be falling,
    //    //        and still be on a solid block (eg. sand block);
    //    // (TODO) This still needs to be implemented properly
    //    if (player.physics.vy > 0.0f) {
    //        player.status.jumpPower = 0.0f;
    //    }
    //}
//
    //// The player cannot hit a block with both the left side and the right side
    //// so it's (or at least should be) ok to or the results
    //LevelScreenBlock blockL = (blockTL) ? blockTL : blockBL;
    //LevelScreenBlock blockR = (blockTR) ? blockTR : blockBR;
    //block = blockL | blockR;
    //if (block && block != BLOCK_FAKE) {
    //    if (player.status.inAir) {
    //        // Update status
    //        player.status.hitWall = 1;
    //        // Update physics
    //        player.physics.vx *= -0.5f;
    //    }
    //    player.physics.x = (float)((short) newX);
    //} else {
    //    // Update physics
    //    player.physics.x = newX;
    //}

    // Check for y-collisions and scroll the screen
    if (player.physics.y < 0.0f) {
        if (player.status.stunned) {
            player.status.hitFloor = 1;
        }
        player.physics.vx = 0.0f;
        player.physics.vy = 0.0f;
        player.physics.y = 0.0f;
        player.status.hitWall = 0;
    } else if (player.physics.y > SCREEN_HEIGHT) {
        player.physics.y = 0.0f;
    }

    // Check for x-collisions
    if (player.physics.x < -BORDER_OFFSET) {
        player.physics.x = -BORDER_OFFSET;
        if (player.status.inAir) {
            player.physics.vx = -(player.physics.vx / 2.0f);
            player.status.hitWall = 1;
        }
    } else if (player.physics.x > BORDER_OFFSET) {
        player.physics.x = BORDER_OFFSET;
        if (player.status.inAir) {
            player.physics.vx = -(player.physics.vx / 2.0f);
            player.status.hitWall = 1;
        }
    }

    // Update physics
    {     
        player.physics.x += player.physics.vx * delta;
        player.physics.y += player.physics.vy * delta;
    }

    // Update graphics
    {
        // Flip the sprite according to the player direction.
        if (player.input.direction == +1) {
            player.graphics.spriteUOffset = 0;
        } else if (player.input.direction == -1) {
            player.graphics.spriteUOffset = PLAYER_SPRITE_WIDTH;
        }

        // Update screeen coordinates.
        player.graphics.sx = ((short) player.physics.x) + (LEVEL_SCREEN_PXWIDTH / 2);
        player.graphics.sy = LEVEL_SCREEN_PXHEIGHT - ((short) player.physics.y);

        // Find the appropriate sprite index for the current frame.
        SpriteIndex newSpriteIndex;
        if (player.status.jumpPower) {
            newSpriteIndex = SPRITE_CHARGING;
        } else if (player.status.hitFloor) {
            newSpriteIndex = SPRITE_HITFLOOR;
        } else if (player.status.hitWall) {
            newSpriteIndex = SPRITE_HITWALL;
        } else if (player.status.inAir) {
            newSpriteIndex = (player.physics.vy > 0.0f) ? SPRITE_JUMPING : SPRITE_FALLING;
        } else if (player.physics.vx && player.input.direction) {
            player.graphics.walkAnimCycle += 1;
            switch (player.graphics.walkAnimCycle / 4) {
                case 0:
                case 1:
                case 2:
                    newSpriteIndex = SPRITE_WALKING0;
                    break;
                case 3:
                    newSpriteIndex = SPRITE_WALKING1;
                    break;
                case 4:
                case 5:
                case 6:
                    newSpriteIndex = SPRITE_WALKING2;
                    break;
                case 7:
                    newSpriteIndex = SPRITE_WALKING1;
                    break;
                default:
                    player.graphics.walkAnimCycle = 0;
                    break;
            }
        } else {
            newSpriteIndex = SPRITE_STANDING;
        }

        // Update sprite pointer if the new sprite is different
        // from the previous one.
        if (player.graphics.spriteIndex != newSpriteIndex) {
            player.graphics.spriteIndex = newSpriteIndex;
            player.graphics.sprite = PLAYER_GET_SPRITE(player.graphics.spriteIndex);
        }
    }
}

void kingRender(short *outSX, short *outSY) {
    Vertex *vertices = (Vertex*) sceGuGetMemory(2 * sizeof(Vertex));
    // Translate the player's level screen coordinates
    // to the PSP's screen coordinates.
    vertices[0].x = player.graphics.sx - (PLAYER_SPRITE_WIDTH / 2);
    vertices[0].y = (player.graphics.sy - (LEVEL_SCREEN_PXHEIGHT - SCREEN_HEIGHT)) - PLAYER_SPRITE_HEIGHT;
    vertices[1].x = player.graphics.sx + (PLAYER_SPRITE_WIDTH / 2);
    vertices[1].y = (player.graphics.sy - (LEVEL_SCREEN_PXHEIGHT - SCREEN_HEIGHT));
    // Set both vertices to have a depth of 1 so that the player's
    // sprite sits on top of the background.
    vertices[0].z = 1;
    vertices[1].z = 1;
    // Set the sprite's texture coordinates according to the direction
    // the player is currently facing.
    vertices[0].u = player.graphics.spriteUOffset;
    vertices[0].v = 0;
    vertices[1].u = PLAYER_SPRITE_WIDTH - player.graphics.spriteUOffset;
    vertices[1].v = PLAYER_SPRITE_HEIGHT;

    // Enable blending to account for transparency.
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    // Set the texture as the current selected sprite for the player.
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexImage(0, PLAYER_SPRITE_WIDTH, PLAYER_SPRITE_HEIGHT, PLAYER_SPRITE_WIDTH, player.graphics.sprite);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    // Draw it!
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, vertices);
    // Disable blending since it's not needed anymore.
    sceGuDisable(GU_BLEND);

    // Output the previous' frame level screen coordinates.
    // This is needed by in the game's render function to paint
    // over the player sprite in the previous frame, which is
    // needed since we're not clearing the framebuffer every frame
    // to avoid having to re-render the entire midground texture
    // which is stored in RAM.
    *outSX = player.graphics.sx;
    *outSY = player.graphics.sy;
}

void kingDestroy(void) {
    unloadTextureVram(player.graphics.sprites);
};
