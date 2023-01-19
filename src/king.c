#include "king.h"
#include "state.h"
#include <string.h>

#define PLAYER_BLOCK_WIDTH (PLAYER_SPRITE_WIDTH / LEVEL_BLOCK_SIZE)
#define PLAYER_BLOCK_HEIGHT (PLAYER_SPRITE_HEIGHT / LEVEL_BLOCK_SIZE)

#define PLAYER_BLOCK_HALFW (PLAYER_BLOCK_WIDTH / 2)
#define PLAYER_BLOCK_HALFH (PLAYER_BLOCK_HEIGHT / 2)

// Physics constants
#define PLAYER_MAX_VSPEED 450.0f
#define PLAYER_MAX_HSPEED 100.0f
#define PLAYER_GRAVITY 11.5f

// Status constants
#define PLAYER_CHARGING_SPEED (PLAYER_MAX_VSPEED * 2.0f)
#define PLAYER_STUN_TIME 1.0f
#define PLAYER_MAX_FALL_TIME 2.5f
#define PLAYER_GET_SPRITE(idx) (playerSprites + PLAYER_SPRITE_WIDTH * PLAYER_SPRITE_HEIGHT * 4 * (idx))

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
} SpriteIndex;

typedef enum {
    COLLMOD_NONE = 0,
    COLLMOD_NOWIND = 1,
    COLLMOD_WATER = 2,
    COLLMOD_SAND = 4,
    COLLMOD_QUARK = 8,
    COLLMOD_ICE = 16,
    COLLMOD_SNOW = 32,
    COLLMOD_SOLID = 64,
} CollisionModifier;

typedef struct {
    float x, y;
    float vx, vy;
} PlayerPhysics;

typedef struct {
    short direction;
    short jumping;
} PlayerInput;

typedef struct {
    char inAir;
    char hitWallMidair;
    char maxJumpPower;
    char stunned;
    float jumpPower;
    float stunTime;
    float fallTime;
} PlayerStatus;

typedef struct {
    short sx;
    short sy;
    short walkAnimCycle;
    short spriteUOffset;
    SpriteIndex spriteIndex;
    char *sprite;
} PlayerGraphics;

typedef struct {
    PlayerInput input;
    PlayerStatus status;
    PlayerPhysics physics;
    PlayerGraphics graphics;
} Player;

static char blockPropertiesMap[] = {
    COLLMOD_NONE,
    COLLMOD_SOLID,
    COLLMOD_SOLID,
    COLLMOD_SOLID,
    COLLMOD_SOLID,
    COLLMOD_SOLID,
    COLLMOD_NONE,
    COLLMOD_SOLID | COLLMOD_ICE,
    COLLMOD_SOLID | COLLMOD_SNOW,
    COLLMOD_SAND,
    COLLMOD_NOWIND,
    COLLMOD_WATER,
    COLLMOD_QUARK
};

static Player player;
static char *playerSprites;

static void kingDoCollision(float *pNewX, float *pNewY, short *pNewSX, short *pNewSY, LevelScreen *screen) {
    float newX = *pNewX;
    float newY = *pNewY;
    short newSX = *pNewSX;
    short newSY = *pNewSY;

    // Compute player's map coordinates
    // NOTE: add one in case we are moving from left to right or up to down to account
    //       for the size of the block. This has to be done because the block coordinates
    //       in the collision map are relative to the top-left corner of the block.
    short mapX = newSX / LEVEL_BLOCK_SIZE + (player.physics.vx > 0.0f);
    short mapY = (newSY - PLAYER_SPRITE_HALFH) / LEVEL_BLOCK_SIZE + (player.physics.vy < 0.0f);
    // Get collision info
    int minDist2 = -1;          // the distance of the closest block that has collided with the player
    short collX, collY;         // the screen coordinates of the collision (relative to the top-left corner of the block)
    short collMapX, collMapY;   // the map coordinates of the collision (relative to the top-left corner of the block)
    short collOffX, collOffY;   // the coordinates of the collision relative to the player sprite center's screen coordinates
    LevelScreenBlock collBlock; // the block the player has collided with
    char collModifiers = 0;     // the collision modifiers
    // Loop through all the blocks the player's sprite is occupying
    for (short y = -PLAYER_BLOCK_HALFH; y < PLAYER_BLOCK_HALFH; y++) {
        for (short x = -PLAYER_BLOCK_HALFW; x < PLAYER_BLOCK_HALFW; x++) {
            int lx = mapX + x;
            int ly = mapY + y;
            // Check if the block is in the current screen...
            if (lx < 0 || lx >= LEVEL_SCREEN_WIDTH || ly < 0 || ly >= LEVEL_SCREEN_HEIGHT) {
                continue;
            }
            LevelScreenBlock block = screen->blocks[ly][lx];
            // Update the collision modifiers
            collModifiers |= blockPropertiesMap[block];
            if (LEVEL_BLOCK_ISSOLID(block)) {
                // If the player is colliding with a block
                // compute the distance of the collision relative
                // to the center of the player's sprite
                int dist2 = x * x + y * y;
                if (minDist2 < 0 || dist2 <= minDist2) {
                    // If this is the closest collision,
                    // store the collision information
                    minDist2 = dist2;
                    collX = lx * LEVEL_BLOCK_SIZE;
                    collY = ly * LEVEL_BLOCK_SIZE;
                    collMapX = lx;
                    collMapY = ly;
                    collOffX = x * LEVEL_BLOCK_SIZE;
                    collOffY = y * LEVEL_BLOCK_SIZE;
                    collBlock = block;
                }
            }
        }
    }

    // If the player has collided with something...
    if (minDist2 >= 0) {
        if (collModifiers & COLLMOD_SOLID) {
            minDist2 = -1;
            int adjX = -1, adjY = -1;
            int deltaX, deltaY;
            int refX = player.graphics.sx;
            int refY = player.graphics.sy - PLAYER_SPRITE_HALFH;
            // Find the closest free block to the collision block.
            for (short y = -2; y <= 2; y++) {
                for (short x = -2; x <= 2; x++) {
                    int lx = collMapX + x;
                    int ly = collMapY + y;
                    // Check if the block is in the current screen...
                    if (lx < 0 || lx >= LEVEL_SCREEN_WIDTH || ly < 0 || ly >= LEVEL_SCREEN_HEIGHT) {
                        continue;
                    }
                    if (!LEVEL_BLOCK_ISSOLID(screen->blocks[ly][lx])) {
                        int newAdjX = lx * LEVEL_BLOCK_SIZE;
                        int newAdjY = ly * LEVEL_BLOCK_SIZE;
                        int newDeltaX = (newAdjX > collX) ? (newAdjX - collX) : (collX - newAdjX);
                        int newDeltaY = (newAdjY > collY) ? (newAdjY - collY) : (collY - newAdjY);
                        int dx = newAdjX + LEVEL_BLOCK_HALF - refX;
                        int dy = newAdjY + LEVEL_BLOCK_HALF - refY;
                        int dist2 = (dx * dx + dy * dy) + (newDeltaX * newDeltaX + newDeltaY * newDeltaY) * 2;
                        if (minDist2 < 0 || dist2 <= minDist2) {
                            if (dist2 == minDist2) {
                                // If the distance between two free blocks is the same,
                                // prioritize the block with the least distance from the
                                // center of the collision block.
                                if (newDeltaX < deltaX) {
                                    adjX = newAdjX;
                                    deltaX = newDeltaX;
                                }
                                if (newDeltaY < deltaY) {
                                    adjY = newAdjY;
                                    deltaY = newDeltaY;
                                }
                            } else {
                                adjX = newAdjX;
                                adjY = newAdjY;
                                deltaX = newDeltaX;
                                deltaY = newDeltaY;
                            }
                            minDist2 = dist2;
                        }
                    }
                }
            }

            // If the correction applied to the player position was
            // greater along the Y-axis, then the collision was a vertical one
            int wasVerticalCollision = deltaX <= deltaY;

            // Compute new corrected player screen coordinates and position
            if (wasVerticalCollision) {
                newSY = adjY - collOffY + PLAYER_SPRITE_HALFH;
                newY = (float) (LEVEL_SCREEN_PXHEIGHT - newSY);
            } else {
                newSX = adjX - collOffX;
                newX = ((float) newSX) - ((float) LEVEL_SCREEN_PXWIDTH / 2);
            }

            int isBlockSlope = LEVEL_BLOCK_ISSLOPE(collBlock);

            // If the player is in the air and has collided vertically, while falling,
            // and the block it has collided with is not a slope.
            player.status.inAir = player.status.inAir && (!wasVerticalCollision || player.physics.vy >= 0.0f);

            // If the player has hit the floor and has reached it's terminal falling velocity,
            // the player has to be stunned.
            // TODO: handle slopes
            player.status.stunned = !isBlockSlope && !player.status.inAir && player.status.fallTime > PLAYER_MAX_FALL_TIME;
            // If the player has been stunned set the timer.
            player.status.stunTime = player.status.stunned * PLAYER_STUN_TIME;

            // If the player is in the air and has collided with something
            // horizontally, they've hit a wall.
            // TODO: handle slopes
            player.status.hitWallMidair = (player.status.inAir || player.physics.vy > 0.0f) && !wasVerticalCollision;

            // TODO: - apply speed dampening effects
            //       - handle slopes and sliding
            player.physics.vy = !wasVerticalCollision * player.physics.vy;
            player.physics.vx = !wasVerticalCollision * -player.physics.vx * 0.5f;

            // Reset the fall time if something was hit.
            player.status.fallTime = 0.0f;
        }

        // TODO: take into account special modifiers that don't necessarily
        //       modify the player's position
    }

    *pNewX = newX;
    *pNewY = newY;
    *pNewSX = newSX;
    *pNewSY = newSY;
}

void kingCreate(void) {
    memset(&player, 0, sizeof(Player));
    playerSprites = loadTextureVram("host0://assets/king/base/regular.qoi", NULL, NULL);
    player.physics.y = 32.0f;
    player.graphics.spriteIndex = SPRITE_STANDING;
    player.graphics.sprite = PLAYER_GET_SPRITE(player.graphics.spriteIndex);
}

void kingUpdate(float delta, LevelScreen *screen, unsigned int *outScreen) {
    // Update status
    {
        if (player.physics.vy) {
            // If the vertical velocity is not zero,
            // the player is in the air.
            player.status.inAir = 1;
            if (player.physics.vy > 0.0f) {
                // If the player is jumping up (meaning the vertical velocity is positive),
                // reset the jump power.
                // NOTE: This is there because the player can be falling,
                //       and still be on a solid block (eg. sand block).
                // TODO: This still needs to be implemented properly.
                player.status.jumpPower = 0.0f;
                // Also reset the fall time.
                player.status.fallTime = 0.0f;
            } else if (player.status.fallTime == 0.0f) {
                // If the player is falling (meaning the vertical velocity is negative),
                // count up the fall time.
                player.status.fallTime += delta;
            }
        } else {
            // Check if the player is standing on solid ground.
            int mapX = player.graphics.sx / LEVEL_BLOCK_SIZE;
            int mapY = player.graphics.sy / LEVEL_BLOCK_SIZE;
            for (int x = -PLAYER_BLOCK_HALFW; x < PLAYER_BLOCK_HALFW; x++) {
                LevelScreenBlock block = screen->blocks[mapY][mapX + x];
                player.status.inAir |= LEVEL_BLOCK_ISSOLID(block) && !LEVEL_BLOCK_ISSLOPE(block);
            }
            player.status.inAir = !player.status.inAir;
            
            // Update physics
            {
                if (player.status.inAir) {
                    player.physics.vx = player.input.direction * PLAYER_MAX_HSPEED;
                }
            }
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
            // Update stunned timer
            if (player.status.stunTime > 0) {
                player.status.stunTime -= delta;
            }

            // Un-stun the player if input was recieved
            // and the cooldown time has elapsed.
            if (player.status.stunned && player.status.stunTime <= 0 && (player.input.jumping || player.input.direction)) {
                player.status.stunned = 0;
            }

            // Check if the player is pressing the jump button
            // and, if true, build up jump power.
            if (!player.status.stunned && player.input.jumping) {
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
    }

    // Compute new player position.
    float newX = player.physics.x + player.physics.vx * delta;
    float newY = player.physics.y + player.physics.vy * delta;
    // Convert new player position to screen coordinates.
    short newSX = ((short) newX) + (LEVEL_SCREEN_PXWIDTH / 2);
    short newSY = LEVEL_SCREEN_PXHEIGHT - ((short) newY);
    
    // If the player is within the leve screen bounds,
    // handle collisions.
    kingDoCollision(&newX, &newY, &newSX, &newSY, screen);

    if (newSY - PLAYER_SPRITE_HALFH < 0) {
        // If the player has left the screen from the top side...
        newSY += LEVEL_SCREEN_PXHEIGHT;
        newY -= LEVEL_SCREEN_PXHEIGHT;
        *outScreen += 1;
    } else if (newSY - PLAYER_SPRITE_HALFH >= LEVEL_SCREEN_PXHEIGHT) {
        // If the player has left the screen from the bottom side...
        newSY -= LEVEL_SCREEN_PXHEIGHT;
        newY += LEVEL_SCREEN_PXHEIGHT;
        *outScreen -= 1;
    } else if (newSX - PLAYER_SPRITE_HALFW < 0) {
        // If the player has left the screen from the left side...
        newSX += LEVEL_SCREEN_PXWIDTH;
        newX += LEVEL_SCREEN_PXWIDTH;
        *outScreen = screen->teleportIndex;
    } else if (newSX + PLAYER_SPRITE_HALFW > LEVEL_SCREEN_PXWIDTH) {
        // If the player has left the screen from the right side...
        newSX -= LEVEL_SCREEN_PXWIDTH;
        newX -= LEVEL_SCREEN_PXWIDTH;
        *outScreen = screen->teleportIndex;
    }

    // Update physics
    {
        // Update player position.
        player.physics.x = newX;
        player.physics.y = newY;
    }

    // Update graphics
    {
        // Update screeen coordinates.
        player.graphics.sx = newSX;
        player.graphics.sy = newSY;

        // If the player is not stunned...
        if (!player.status.stunned) {
            // Flip the sprite according to the player direction.
            if (player.input.direction == +1) {
                player.graphics.spriteUOffset = 0;
            } else if (player.input.direction == -1) {
                player.graphics.spriteUOffset = PLAYER_SPRITE_WIDTH;
            }
        }

        // Find the appropriate sprite index for the current frame.
        SpriteIndex newSpriteIndex;
        if (player.status.jumpPower) {
            newSpriteIndex = SPRITE_CHARGING;
        } else if (player.status.stunned) {
            newSpriteIndex = SPRITE_STUNNED;
        } else if (player.status.hitWallMidair) {
            newSpriteIndex = SPRITE_HITWALLMIDAIR;
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
                default:
                    player.graphics.walkAnimCycle = 0;
                    break;
            }
        } else {
            newSpriteIndex = SPRITE_STANDING;
            player.graphics.walkAnimCycle = 0;
        }

        // Update sprite pointer if the new sprite is different
        // from the previous one.
        if (player.graphics.spriteIndex != newSpriteIndex) {
            player.graphics.spriteIndex = newSpriteIndex;
            player.graphics.sprite = PLAYER_GET_SPRITE(player.graphics.spriteIndex);
        }
    }
}

void kingRender(short *outSX, short *outSY, unsigned int currentScroll) {
    Vertex *vertices = (Vertex*) sceGuGetMemory(2 * sizeof(Vertex));
    // Translate the player's level screen coordinates
    // to the PSP's screen coordinates.
    vertices[0].x = player.graphics.sx - PLAYER_SPRITE_HALFW;
    vertices[0].y = (player.graphics.sy - currentScroll) - PLAYER_SPRITE_HEIGHT;
    vertices[1].x = player.graphics.sx + PLAYER_SPRITE_HALFW;
    vertices[1].y = (player.graphics.sy - currentScroll);
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
    unloadTextureVram(playerSprites);
};
