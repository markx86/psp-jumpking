#include "king.h"
#include "state.h"
#include <string.h>

//#define PLAYER_HITBOX_WIDTH 18
//#define PLAYER_HITBOX_HEIGHT 26

#define PLAYER_HITBOX_WIDTH 24
#define PLAYER_HITBOX_HEIGHT 32

#define PLAYER_HITBOX_HALFW (PLAYER_HITBOX_WIDTH / 2)
#define PLAYER_HITBOX_HALFH (PLAYER_HITBOX_HEIGHT / 2)

#define PLAYER_HITBOX_BLOCK_WIDTH (PLAYER_HITBOX_WIDTH / LEVEL_BLOCK_SIZE)
#define PLAYER_HITBOX_BLOCK_HEIGHT (PLAYER_HITBOX_HEIGHT / LEVEL_BLOCK_SIZE)

#define PLAYER_HITBOX_BLOCK_HALFW (PLAYER_HITBOX_BLOCK_WIDTH / 2)
#define PLAYER_HITBOX_BLOCK_HALFH (PLAYER_HITBOX_BLOCK_HEIGHT / 2)

// Physics constants
#define PLAYER_UPDATE_DELTA (1.0f / 60.0f)
#define PLAYER_JUMP_HEIGHT 153.0f
#define PLAYER_JUMP_VSPEED 9.0f
//#define PLAYER_JUMP_VSPEED 525.0f
#define PLAYER_JUMP_HSPEED 3.5f
//#define PLAYER_JUMP_HSPEED 210.0f
#define PLAYER_WALK_SPEED 1.5f
//#define PLAYER_WALK_SPEED 90.0f
#define PLAYER_WALL_BOUNCE 0.45f
#define PLAYER_GRAVITY 0.275f
#define PLAYER_MAX_FALL_SPEED 10.0f
//#define PLAYER_MAX_FALL_SPEED 1200.0f

// Status constants
#define PLAYER_CHARGE_TIME 0.6f
#define PLAYER_STUN_TIME 0.5f
#define PLAYER_MAX_FALL_TIME 1.0f
#define PLAYER_GET_SPRITE(idx) (allSprites + PLAYER_SPRITE_WIDTH * PLAYER_SPRITE_HEIGHT * 4 * (idx))

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
} SpriteIndex;

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
} CollisionModifier;

// Coordinates
float worldX, worldY;
float velocityX, velocityY;
short screenX, screenY;
// Input
short direction, jumpPressed;
// Flags
char inAir, hitWallMidair, maxJumpPowerReached, isStunned;
// Status
float jumpPower, stunTime, fallTime;
// Graphics
short walkAnimCycle, spriteUOffset;
SpriteIndex currentSpriteIndex;
static char *currentSprite, *allSprites;

static float deltaU;

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

static void kingDoCollision(float newX, float newY, LevelScreen *screen) {
    // Convert new player position to screen coordinates.
    short newSX = ((short) newX) + (LEVEL_SCREEN_PXWIDTH / 2);
    short newSY = LEVEL_SCREEN_PXHEIGHT - ((short) newY);

    // Compute player's map coordinates
    // NOTE: add one in case we are moving from left to right or up to down to account
    //       for the size of the block. This has to be done because the block coordinates
    //       in the collision map are relative to the top-left corner of the block.
    short mapX = newSX / LEVEL_BLOCK_SIZE + (velocityX > 0.0f);
    short mapY = (newSY - PLAYER_HITBOX_HALFH) / LEVEL_BLOCK_SIZE + (velocityY < 0.0f);
    // Get collision info
    int minDist2 = -1;          // the distance of the closest block that has collided with the player
    short collX, collY;         // the screen coordinates of the collision (relative to the top-left corner of the block)
    short collMapX, collMapY;   // the map coordinates of the collision (relative to the top-left corner of the block)
    short collOffX, collOffY;   // the coordinates of the collision relative to the player sprite center's screen coordinates
    LevelScreenBlock collBlock; // the block the player has collided with
    char collModifiers = 0;     // the collision modifiers
    // Loop through all the blocks the player's sprite is occupying
    for (short y = -PLAYER_HITBOX_BLOCK_HALFH; y < PLAYER_HITBOX_BLOCK_HALFH; y++) {
        for (short x = -PLAYER_HITBOX_BLOCK_HALFW; x < PLAYER_HITBOX_BLOCK_HALFW; x++) {
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
            int refX = screenX;
            int refY = screenY - PLAYER_HITBOX_HALFH;
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
                newSY = adjY - collOffY + PLAYER_HITBOX_HALFH;
                newY = (float) (LEVEL_SCREEN_PXHEIGHT - newSY);
            } else {
                newSX = adjX - collOffX;
                newX = ((float) newSX) - ((float) LEVEL_SCREEN_PXWIDTH / 2);
            }

            int isBlockSlope = LEVEL_BLOCK_ISSLOPE(collBlock);

            // If the player is in the air and has collided vertically, while falling,
            // and the block it has collided with is not a slope.
            inAir = inAir && (!wasVerticalCollision || velocityY >= 0.0f);

            // If the player has hit the floor and has reached it's terminal falling velocity,
            // the player has to be stunned.
            // TODO: handle slopes
            isStunned = !isBlockSlope && !inAir && fallTime > PLAYER_MAX_FALL_TIME;
            // If the player has been stunned set the timer.
            stunTime = isStunned * PLAYER_STUN_TIME;

            // If the player is in the air and has collided with something
            // horizontally, they've hit a wall.
            // TODO: handle slopes
            hitWallMidair = (inAir || velocityY > 0.0f) && !wasVerticalCollision;

            // Reset the fall time if something was hit.
            if (isBlockSlope || wasVerticalCollision && velocityY > 0.0f) {
                fallTime = 0.0f;
            }

            // TODO: - apply speed dampening effects
            //       - handle slopes and sliding
            velocityX = (!wasVerticalCollision || velocityY > 0.0f) * -velocityX * PLAYER_WALL_BOUNCE;
            velocityY = !wasVerticalCollision * velocityY;
        }

        // TODO: take into account special modifiers that don't necessarily
        //       modify the player's position
    }

    // Update physics
    {
        // Update player position.
        worldX = newX;
        worldY = newY;
    }

    // Update graphics
    {
        // Update screeen coordinates.
        screenX = newSX;
        screenY = newSY;
    }
}

void kingCreate(void) {
    allSprites = loadTextureVram("host0://assets/king/base/regular.qoi", NULL, NULL);
    // Set the player starting position.
    worldX = 0.0f;
    worldY = 32.0f;
    // Set the player initial speed.
    velocityX = 0.0f;
    velocityY = 0.0f;
    // Update the player screen coordinates.
    screenX = ((short) worldX) + (LEVEL_SCREEN_PXWIDTH / 2);
    screenY = LEVEL_SCREEN_PXHEIGHT - ((short) worldY);
    // Reset input.
    direction = 0;
    jumpPressed = 0;
    // Reset flags.
    inAir = 0;
    hitWallMidair = 0;
    maxJumpPowerReached = 0;
    isStunned = 1;
    // Reset status.
    jumpPower = 0.0f;
    stunTime = 0.0f;
    fallTime = 0.0f;
    // Reset animation.
    walkAnimCycle = 0;
    spriteUOffset = 0;
    // Set the initial sprite.
    currentSpriteIndex = SPRITE_STUNNED;
    currentSprite = PLAYER_GET_SPRITE(currentSpriteIndex);
    // Initialize the update delta time.
    deltaU = 0.0f;
}

void kingUpdate(float delta, LevelScreen *screen, unsigned int *outScreenIndex) {
    // Fix the player update loop to 60 updates per second.
    deltaU += delta;
    if (deltaU < PLAYER_UPDATE_DELTA) {
        return;
    }
    deltaU -= PLAYER_UPDATE_DELTA;

    // Update status
    {
        if (velocityY) {
            // If the vertical velocity is not zero,
            // the player is in the air.
            inAir = 1;
            if (velocityY > 0.0f) {
                // If the player is jumping up (meaning the vertical velocity is positive),
                // reset the jump power.
                // NOTE: This is there because the player can be falling,
                //       and still be on a solid block (eg. sand block).
                // TODO: This still needs to be implemented properly.
                jumpPower = 0.0f;
                // Also reset the fall time.
                fallTime = 0.0f;
            } else if (fallTime < PLAYER_MAX_FALL_TIME) {
                // If the player is falling (meaning the vertical velocity is negative),
                // count up the fall time.
                fallTime += PLAYER_UPDATE_DELTA;
            }
        } else {
            // Check if the player is standing on solid ground.
            int mapX = screenX / LEVEL_BLOCK_SIZE;
            int mapY = screenY / LEVEL_BLOCK_SIZE;
            for (int x = -PLAYER_HITBOX_BLOCK_HALFW; x < PLAYER_HITBOX_BLOCK_HALFW; x++) {
                LevelScreenBlock block = screen->blocks[mapY][mapX + x];
                inAir |= LEVEL_BLOCK_ISSOLID(block) && !LEVEL_BLOCK_ISSLOPE(block);
            }
            inAir = !inAir;
            
            if (inAir) {
                // Update physics
                {
                    velocityX = direction * PLAYER_WALK_SPEED;
                }
                
                // Update status
                {
                    fallTime = 0.0f;
                }
            }
        }
    }

    if (!inAir) {
        // If the player is on the ground...
        
        // Resolve input
        {
            // Check which direction the player wants to go.
            short rightBias = (((Input.Buttons & PSP_CTRL_RIGHT) != 0) << 1) | ((Latch.uiBreak & PSP_CTRL_RIGHT) != 0);
            short leftBias = (((Input.Buttons & PSP_CTRL_LEFT) != 0) << 1) | ((Latch.uiBreak & PSP_CTRL_LEFT) != 0);
            direction = (leftBias > rightBias) ? -1 : (leftBias < rightBias) ? +1 : 0;
            // Check if the player is trying to jump.
            jumpPressed = (Input.Buttons & PSP_CTRL_CROSS);
        }

        // Update status
        {
            // Update stunned timer
            if (stunTime > 0) {
                stunTime -= PLAYER_UPDATE_DELTA;
            }

            // Un-stun the player if input was recieved
            // and the cooldown time has elapsed.
            if (isStunned && stunTime <= 0 && (jumpPressed || direction)) {
                isStunned = 0;
            }

            // Check if the player is pressing the jump button
            // and, if true, build up jump power.
            if (!isStunned && jumpPressed) {
                jumpPower += (PLAYER_JUMP_VSPEED / PLAYER_CHARGE_TIME) * PLAYER_UPDATE_DELTA;
                maxJumpPowerReached = jumpPower >= PLAYER_JUMP_VSPEED;
            }
        }

        // Update physics
        {
            if (!isStunned) {
                if (jumpPressed && !maxJumpPowerReached) {
                    // Freeze the player if the jump button is pressed.
                    velocityX = 0.0f;
                } else {
                    if (jumpPower) {
                        // If the jump button was released...
                        // - set the vertical speed to the jump power that was built up
                        velocityY = jumpPower;
                        velocityX = direction * PLAYER_JUMP_HSPEED;
                    } else {
                        // Otherwise, walk at normal speed.
                        velocityX = direction * PLAYER_WALK_SPEED;
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
            if (velocityY > -PLAYER_MAX_FALL_SPEED) {
                velocityY -= PLAYER_GRAVITY;
            }
        }
    }

    // Compute new player position.
    float newX = worldX + velocityX;
    float newY = worldY + velocityY;
    
    // If the player is within the leve screen bounds,
    // handle collisions.
    kingDoCollision(newX, newY, screen);

    if (screenY - PLAYER_HITBOX_HALFH < 0) {
        // If the player has left the screen from the top side...
        screenY += LEVEL_SCREEN_PXHEIGHT;
        worldY -= LEVEL_SCREEN_PXHEIGHT;
        *outScreenIndex += 1;
    } else if (screenY - PLAYER_HITBOX_HALFH >= LEVEL_SCREEN_PXHEIGHT) {
        // If the player has left the screen from the bottom side...
        screenY -= LEVEL_SCREEN_PXHEIGHT;
        worldY += LEVEL_SCREEN_PXHEIGHT;
        *outScreenIndex -= 1;
    } else if (screenX - PLAYER_HITBOX_HALFW < 0) {
        // If the player has left the screen from the left side...
        screenX += LEVEL_SCREEN_PXWIDTH;
        worldX += LEVEL_SCREEN_PXWIDTH;
        *outScreenIndex = screen->teleportIndex;
    } else if (screenX + PLAYER_HITBOX_HALFW > LEVEL_SCREEN_PXWIDTH) {
        // If the player has left the screen from the right side...
        screenX -= LEVEL_SCREEN_PXWIDTH;
        worldX -= LEVEL_SCREEN_PXWIDTH;
        *outScreenIndex = screen->teleportIndex;
    }

    // Update graphics
    {
        // If the player is not stunned...
        if (!isStunned) {
            // Flip the sprite according to the player direction.
            if (direction == +1) {
                spriteUOffset = 0;
            } else if (direction == -1) {
                spriteUOffset = PLAYER_SPRITE_WIDTH;
            }
        }

        // Find the appropriate sprite index for the current frame.
        SpriteIndex newSpriteIndex;
        if (jumpPower) {
            newSpriteIndex = SPRITE_CHARGING;
        } else if (isStunned) {
            newSpriteIndex = SPRITE_STUNNED;
        } else if (hitWallMidair) {
            newSpriteIndex = SPRITE_HITWALLMIDAIR;
        } else if (inAir) {
            newSpriteIndex = (velocityY > 0.0f) ? SPRITE_JUMPING : SPRITE_FALLING;
        } else if (velocityX && direction) {
            walkAnimCycle += 1;
            switch (walkAnimCycle / 4) {
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
                    walkAnimCycle = 0;
                    break;
            }
        } else {
            newSpriteIndex = SPRITE_STANDING;
            walkAnimCycle = 0;
        }

        // Update sprite pointer if the new sprite is different
        // from the previous one.
        if (currentSpriteIndex != newSpriteIndex) {
            currentSpriteIndex = newSpriteIndex;
            currentSprite = PLAYER_GET_SPRITE(currentSpriteIndex);
        }
    }
}

void kingRender(short *outSX, short *outSY, unsigned int currentScroll) {
    Vertex *vertices = (Vertex*) sceGuGetMemory(2 * sizeof(Vertex));
    // Translate the player's level screen coordinates
    // to the PSP's screen coordinates.
    vertices[0].x = screenX - PLAYER_SPRITE_HALFW;
    vertices[0].y = (screenY - currentScroll) - PLAYER_SPRITE_HEIGHT;
    vertices[1].x = screenX + PLAYER_SPRITE_HALFW;
    vertices[1].y = (screenY - currentScroll);
    // Set both vertices to have a depth of 1 so that the player's
    // sprite sits on top of the background.
    vertices[0].z = 1;
    vertices[1].z = 1;
    // Set the sprite's texture coordinates according to the direction
    // the player is currently facing.
    vertices[0].u = spriteUOffset;
    vertices[0].v = 0;
    vertices[1].u = PLAYER_SPRITE_WIDTH - spriteUOffset;
    vertices[1].v = PLAYER_SPRITE_HEIGHT;

    // Enable blending to account for transparency.
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    // Set the texture as the current selected sprite for the player.
    sceGuTexMode(GU_PSM_8888, 0, 0, GU_FALSE);
    sceGuTexImage(0, PLAYER_SPRITE_WIDTH, PLAYER_SPRITE_HEIGHT, PLAYER_SPRITE_WIDTH, currentSprite);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
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
    *outSX = screenX;
    *outSY = screenY;
}

void kingDestroy(void) {
    unloadTextureVram(allSprites);
};
