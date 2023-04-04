#include "king.h"
#include "state.h"
#include <string.h>

// Hitbox sizes
#define PLAYER_HITBOX_WIDTH 18
#define PLAYER_HITBOX_HEIGHT 26
#define PLAYER_HITBOX_HALFW (PLAYER_HITBOX_WIDTH / 2)
#define PLAYER_HITBOX_HALFH (PLAYER_HITBOX_HEIGHT / 2)
#define PLAYER_HITBOX_BLOCK_WIDTH LEVEL_COORDS_SCREEN2MAP(PLAYER_HITBOX_WIDTH)
#define PLAYER_HITBOX_BLOCK_HEIGHT LEVEL_COORDS_SCREEN2MAP(PLAYER_HITBOX_HEIGHT)
#define PLAYER_HITBOX_BLOCK_HALFW (PLAYER_HITBOX_BLOCK_WIDTH / 2)
#define PLAYER_HITBOX_BLOCK_HALFH (PLAYER_HITBOX_BLOCK_HEIGHT / 2)

// Physics constants
#define PLAYER_JUMP_HEIGHT 153.0f
#define PLAYER_JUMP_VSPEED 9.0f
#define PLAYER_JUMP_HSPEED 3.5f
#define PLAYER_WALK_SPEED 1.5f
#define PLAYER_WALL_BOUNCE 0.45f
#define PLAYER_GRAVITY 0.275f
#define PLAYER_MAX_FALL_SPEED 10.0f

// Status constants
#define PLAYER_CHARGE_TIME 0.6f
#define PLAYER_STUN_TIME 0.5f
#define PLAYER_MAX_FALL_TIME 1.0f

// Input constants
#define PLAYER_JUMP_LENIENCY_FRAMES 4

// Macros
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
    COLLMOD_SLOPE = 128,
} CollisionModifier;

typedef struct {
    // the collision modifiers
    union {
        unsigned char modifiers;
        struct {
            unsigned char noWind : 1;
            unsigned char isWater : 1;
            unsigned char isSand : 1;
            unsigned char isQuark : 1;
            unsigned char isIce : 1;
            unsigned char isSnow : 1;
            unsigned char isSolid : 1;
            unsigned char isSlope : 1;
        };
    };
    // the block the player has collided with
    LevelScreenBlock block;
    // the map coordinates of the collision (relative to the top-left corner of the block)
    short brSX, brSY;
    // the screen coordinates of the collision (relative to the top-left corner of the block)
    short tlSX, tlSY;
    // the coordinates of the collision relative to the player sprite center's screen coordinates
    short width, height;
} CollisionInfo;

// Coordinates
static float worldX, worldY;
static float velocityX, velocityY;
static short screenX, screenY;
// Input
static short direction, jumpPressed, leniencyFrames, leniencyDirection;
// Flags
static char inAir, hitWallMidair, maxJumpPowerReached, isStunned;
// Status
static float jumpPower, stunTime, fallTime;
// Graphics
static short walkAnimCycle, spriteUOffset;
static SpriteIndex currentSpriteIndex;
static char *currentSprite, *allSprites;

static char blockCollisionData[] = {
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
    COLLMOD_QUARK
};

static float slopeNormals[4][2] = {
    { -1.0f, -1.0f },
    { +1.0f, -1.0f },
    { -1.0f, +1.0f },
    { +1.0f, +1.0f }
};

static short checkCollision(short sx, short sy, LevelScreen* screen, CollisionInfo* info) {
    short collisions = 0;
    short positiveDirectionX = velocityX > 0.0f;
    short positiveDirectionY = velocityY < 0.0f;
    sy -= PLAYER_HITBOX_HALFH;
    short borderX = sx + ((positiveDirectionX) ? +PLAYER_HITBOX_HALFW : -PLAYER_HITBOX_HALFW);
    short borderY = sy + ((positiveDirectionY) ? +PLAYER_HITBOX_HALFH : -PLAYER_HITBOX_HALFH);
    info->tlSX = (positiveDirectionX) ? -1 : borderX;
    info->tlSY = (positiveDirectionY) ? -1 : borderY;
    info->brSX = (positiveDirectionX) ? borderX : -1;
    info->brSY = (positiveDirectionY) ? borderY : -1;
    short minDist2 = -1;
    for (short oy = -PLAYER_HITBOX_HALFH; oy < PLAYER_HITBOX_HALFH; oy += LEVEL_BLOCK_SIZE) {
        for (short ox = -PLAYER_HITBOX_HALFW; ox < PLAYER_HITBOX_HALFW; ox += LEVEL_BLOCK_SIZE) {
            short cx = sx + ox;
            short cy = sy + oy;
            short mx = LEVEL_COORDS_SCREEN2MAP(cx);
            short my = LEVEL_COORDS_SCREEN2MAP(cy);
            if (mx < 0 || mx >= LEVEL_SCREEN_BLOCK_WIDTH) {
                continue;
            }
            if (my < 0 || my >= LEVEL_SCREEN_BLOCK_HEIGHT) {
                continue;
            }
            LevelScreenBlock block = screen->blocks[my][mx];
            info->modifiers |= blockCollisionData[block];
            if (LEVEL_BLOCK_ISSOLID(block)) {
                    ++collisions;
                    short csx = mx << 3;
                    short csy = my << 3;
                    if (positiveDirectionX) {
                        info->tlSX = (info->tlSX < 0 || csx < info->tlSX) ? csx : info->tlSX;
                    } else {
                        info->brSX = (info->brSX < 0 || csx > info->brSX) ? csx + LEVEL_BLOCK_SIZE : info->brSX;
                    }
                    if (positiveDirectionY) {
                        info->tlSY = (info->tlSY < 0 || csy < info->tlSY) ? csy : info->tlSY;
                    } else {
                        info->brSY = (info->brSY < 0 || csy > info->brSY) ? csy + LEVEL_BLOCK_SIZE : info->brSY;
                    }
                    short dist2 = ox * ox + oy * oy;
                    if (minDist2 < 0 || dist2 < minDist2) {
                        minDist2 = dist2;
                        info->block = block;
                    }
            }
        }
    }
    info->width = info->brSX - info->tlSX;
    info->height = info->brSY - info->tlSY;
    return collisions;
}

static void doCollision(float newX, float newY, LevelScreen *screen) {
    // Convert new player position to screen coordinates.
    short newSX = ((short) newX) + (LEVEL_SCREEN_WIDTH / 2);
    short newSY = LEVEL_SCREEN_HEIGHT - ((short) newY);
    CollisionInfo info;

    // Check if the player has collided with something.
    short collisions = checkCollision(newSX, newSY, screen, &info);
    if (collisions) {
        // If they have, check if the collision was along the X or the Y axis.
        short wasVerticalCollision;
        short isDiagonalCollision = info.width == info.height;
        short absVX = (velocityX > 0.0f) ? velocityX : -velocityX; // |velocityX|
        short absVY = (velocityY > 0.0f) ? velocityY : -velocityY; // |velocityY|
        if (info.width > info.height || (isDiagonalCollision && absVY >= absVX)) {
            // A collision is vertical if the intersection rectangle
            // is wider than it is tall. If the intersection is a square,
            // the collision is considered vertical if the Y component of the
            // velocity vector is greater (or equal) than its X component.
            wasVerticalCollision = 1;
            short vDir = (velocityY < 0.0f) ? +1 : -1;
            newSY -= vDir * info.height;
            newY = (float)(LEVEL_SCREEN_HEIGHT - newSY);
        } else if (info.width < info.height || (isDiagonalCollision && absVX > absVY)) {
            // A collision is horizontal if the intersection rectangle
            // is taller than it is wide. If the intersection is a square,
            // the collision is considered horizontal if the X component 
            // of the velocity vector is greater than its Y component.
            wasVerticalCollision = 0;
            short hDir = (velocityX > 0.0f) ? +1 : -1;
            newSX -= hDir * info.width;
            newX = (float)(newSX - (short)(LEVEL_SCREEN_WIDTH / 2));
        }

        // TODO: Better collision handling.
        inAir = inAir && (!wasVerticalCollision || velocityY > 0.0f);
        isStunned = !info.isSlope && !inAir && fallTime > PLAYER_MAX_FALL_TIME;
        stunTime = isStunned * PLAYER_STUN_TIME;
        hitWallMidair = (inAir || velocityY > 0.0f) && !wasVerticalCollision;
        if (info.isSlope || (wasVerticalCollision && velocityY > 0.0f)) {
            fallTime = 0.0f;
        }
        velocityX = (!wasVerticalCollision || velocityY > 0.0f) * -velocityX * PLAYER_WALL_BOUNCE;
        velocityY *= !wasVerticalCollision;
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
    allSprites = loadTextureVram("assets/king/base/regular.qoi", NULL, NULL);
    // Set the player starting position.
    worldX = 0.0f;
    worldY = 32.0f;
    // Set the player initial speed.
    velocityX = 0.0f;
    velocityY = 0.0f;
    // Update the player screen coordinates.
    screenX = ((short) worldX) + (LEVEL_SCREEN_WIDTH / 2);
    screenY = LEVEL_SCREEN_HEIGHT - ((short) worldY);
    // Reset input.
    direction = 0;
    jumpPressed = 0;
    leniencyFrames = 0;
    leniencyDirection = 0;
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
}

void kingUpdate(float delta, LevelScreen *screen, unsigned int *outScreenIndex) {
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
                fallTime += delta;
            }
        } else {
            // Check if the player is standing on solid ground.
            int mapX = LEVEL_COORDS_SCREEN2MAP(screenX);
            int mapY = LEVEL_COORDS_SCREEN2MAP(screenY);
            for (int x = -PLAYER_HITBOX_BLOCK_HALFW; x <= PLAYER_HITBOX_BLOCK_HALFW; x++) {
                LevelScreenBlock block = screen->blocks[mapY][mapX + x];
                inAir |= LEVEL_BLOCK_ISSOLID(block) && !LEVEL_BLOCK_ISSLOPE(block);
            }
            inAir = !inAir;
            
            if (inAir) {
                // Update physics
                velocityX = direction * PLAYER_WALK_SPEED;
                
                // Update status
                fallTime = 0.0f;
            }
        }
    }

    if (!inAir) {
        // If the player is on the ground...
        
        // Resolve input
        {
            // Check which direction the player wants to go.
            direction = (Input.Buttons & PSP_CTRL_LEFT) ? -1 : (Input.Buttons & PSP_CTRL_RIGHT) ? +1 : 0;
            // Handle the leniency direction.
            if (direction) {
                leniencyFrames = PLAYER_JUMP_LENIENCY_FRAMES;
                leniencyDirection = direction;
            } else {
                --leniencyFrames;
            }
            // Check if the player is trying to jump.
            jumpPressed = (Input.Buttons & PSP_CTRL_CROSS);
        }

        // Update status
        {
            // Update stunned timer
            if (stunTime > 0) {
                stunTime -= delta;
            }

            // Un-stun the player if input was recieved
            // and the cooldown time has elapsed.
            if (isStunned && stunTime <= 0 && (jumpPressed || direction)) {
                isStunned = 0;
            }

            // Check if the player is pressing the jump button
            // and, if true, build up jump power.
            if (!isStunned && jumpPressed) {
                jumpPower += (PLAYER_JUMP_VSPEED / PLAYER_CHARGE_TIME) * delta;
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
                        // - check if the input direction is 0. If it is, check if the
                        //   last non 0 direction was within the last two frames
                        if (leniencyFrames > 0 && direction == 0) {
                            // - if it was, override the player's direction
                            direction = leniencyDirection;
                        }
                        // - set the horizontal speed to PLAYER_JUMP_HSPEED in the direction
                        //   the player is facing
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
    doCollision(newX, newY, screen);

    if (screenY - PLAYER_HITBOX_HALFH < 0) {
        // If the player has left the screen from the top side...
        screenY += LEVEL_SCREEN_HEIGHT;
        worldY -= LEVEL_SCREEN_HEIGHT;
        *outScreenIndex += 1;
    } else if (screenY - PLAYER_HITBOX_HALFH >= LEVEL_SCREEN_HEIGHT) {
        // If the player has left the screen from the bottom side...
        screenY -= LEVEL_SCREEN_HEIGHT;
        worldY += LEVEL_SCREEN_HEIGHT;
        *outScreenIndex -= 1;
    } else if (screenX < 0) {
        // If the player has left the screen from the left side...
        screenX += LEVEL_SCREEN_WIDTH;
        worldX += LEVEL_SCREEN_WIDTH;
        *outScreenIndex = screen->teleportIndex;
    } else if (screenX > LEVEL_SCREEN_WIDTH) {
        // If the player has left the screen from the right side...
        screenX -= LEVEL_SCREEN_WIDTH;
        worldX -= LEVEL_SCREEN_WIDTH;
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
