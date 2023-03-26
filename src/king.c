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
    short mapX, mapY;
    // the screen coordinates of the collision (relative to the top-left corner of the block)
    short screenX, screenY;
    // the coordinates of the collision relative to the player sprite center's screen coordinates
    short offsetX, offsetY;
} CollisionInfo;

// Coordinates
static float worldX, worldY;
static float velocityX, velocityY;
static short screenX, screenY;
// Input
static short direction, jumpPressed;
// Flags
static char inAir, hitWallMidair, maxJumpPowerReached, isStunned;
// Status
static float jumpPower, stunTime, fallTime;
// Graphics
static short walkAnimCycle, spriteUOffset;
static SpriteIndex currentSpriteIndex;
static char *currentSprite, *allSprites;

static float deltaU;

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
    short minDist2 = -1;
    short padX = velocityX > 0.0f;
    short padY = velocityY < 0.0f;
    sy -= PLAYER_HITBOX_HALFH;
    for (short oy = -PLAYER_HITBOX_HALFH; oy < PLAYER_HITBOX_HALFH; oy += LEVEL_BLOCK_SIZE) {
        for (short ox = -PLAYER_HITBOX_HALFW; ox < PLAYER_HITBOX_HALFW; ox += LEVEL_BLOCK_SIZE) {
            short cx = sx + ox;
            short cy = sy + oy;
            short mx = cx / LEVEL_BLOCK_SIZE + padX;
            short my = cy / LEVEL_BLOCK_SIZE + padY;
            if (mx < 0 || mx >= LEVEL_SCREEN_WIDTH) {
                continue;
            }
            if (my < 0 || my >= LEVEL_SCREEN_HEIGHT) {
                continue;
            }
            LevelScreenBlock block = screen->blocks[my][mx];
            info->modifiers |= blockCollisionData[block];
            if (LEVEL_BLOCK_ISSOLID(block)) {
                short dist2 = ox * ox + oy * oy;
                if (minDist2 < 0 || dist2 <= minDist2) {
                    minDist2 = dist2;
                    info->screenX = cx;
                    info->screenY = cy;
                    info->mapX = mx;
                    info->mapY = my;
                    info->offsetX = ox;
                    info->offsetY = oy;
                    info->block = block;
                }
            }
        }
    }
    return minDist2 >= 0;
}

static short debugX = 0, debugY = 0;
static short debugCX = 0, debugCY = 0;
static short prevHDir = 0, prevVDir = 0;

static void doCollision(float newX, float newY, LevelScreen *screen) {
    // Convert new player position to screen coordinates.
    short newSX = ((short) newX) + (LEVEL_SCREEN_PXWIDTH / 2);
    short newSY = LEVEL_SCREEN_PXHEIGHT - ((short) newY);
    CollisionInfo info;

    short xCheck = (velocityX != 0.0f);
    short yCheck = (velocityY != 0.0f);
    short hDir = (velocityX > 0.0f) ? +1 : -1;
    short vDir = (velocityY < 0.0f) ? +1 : -1;

    // If the player has collided with something...
    if (checkCollision(newSX, newSY, screen, &info)) {
        short wasVerticalCollision = 0;
        short collX = info.mapX;
        short collY = info.mapY;
        short invalidCollX = 0;
        short invalidCollY = 0;
        while (!invalidCollX || !invalidCollY) {
            if (!xCheck || collX < 0 || collX >= LEVEL_SCREEN_PXWIDTH) {
                invalidCollX = 1;
            } else {
                if (LEVEL_BLOCK_ISSOLID(screen->blocks[info.mapY][collX])) {
                    collX -= hDir;
                } else {
                    wasVerticalCollision = 0;
                    newSX += (collX * LEVEL_BLOCK_SIZE) - info.screenX;
                    newX = (float)(newSX - (short)(LEVEL_SCREEN_PXWIDTH / 2));
                    debugCX = collX << 3;
                    debugCY = info.screenY;
                    debugX = info.mapX << 3;
                    debugY = info.screenY;
                    break;
                }
            }
            if (!yCheck || collY < 0 || collY >= LEVEL_SCREEN_PXHEIGHT) {
                invalidCollY = 1;
            } else {
                if (LEVEL_BLOCK_ISSOLID(screen->blocks[collY][info.mapX])) {
                    collY -= vDir;
                } else {
                    wasVerticalCollision = 1;
                    newSY += (collY * LEVEL_BLOCK_SIZE) - info.screenY;
                    newY = (float)(LEVEL_SCREEN_PXHEIGHT - newSY);
                    debugCX = info.screenX;
                    debugCY = collY << 3;
                    debugX = info.screenX;
                    debugY = info.mapY << 3;
                    break;
                }
            }
        }
        if (invalidCollX && invalidCollY) {
            return;
        }

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
    worldX = 13.0f * 8.0f;
    worldY = 22.0f * 8.0f;
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
            for (int x = -PLAYER_HITBOX_BLOCK_HALFW; x <= PLAYER_HITBOX_BLOCK_HALFW; x++) {
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
    doCollision(newX, newY, screen);

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
    } else if (screenX < 0) {
        // If the player has left the screen from the left side...
        screenX += LEVEL_SCREEN_PXWIDTH;
        worldX += LEVEL_SCREEN_PXWIDTH;
        *outScreenIndex = screen->teleportIndex;
    } else if (screenX > LEVEL_SCREEN_PXWIDTH) {
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

    sceGuDisable(GU_TEXTURE_2D);

    Vertex* hitboxVerts = sceGuGetMemory(2 * sizeof(Vertex));
    hitboxVerts[0].x = screenX - PLAYER_HITBOX_HALFW;
    hitboxVerts[0].y = screenY - currentScroll - PLAYER_HITBOX_HEIGHT;
    hitboxVerts[0].z = 3;
    hitboxVerts[1].x = screenX + PLAYER_HITBOX_HALFW;
    hitboxVerts[1].y = screenY - currentScroll;
    hitboxVerts[1].z = 3;
    sceGuColor(0xA00000FF);
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, hitboxVerts);

    Vertex* debugVerts = sceGuGetMemory(2 * sizeof(Vertex));
    debugVerts[0].x = debugX;
    debugVerts[0].y = debugY - currentScroll;
    debugVerts[0].z = 4;
    debugVerts[1].x = debugX + LEVEL_BLOCK_SIZE;
    debugVerts[1].y = debugY - currentScroll + LEVEL_BLOCK_SIZE;
    debugVerts[1].z = 4;
    sceGuColor(0xA000FFFF);
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, debugVerts);

    Vertex* debugCollVerts = sceGuGetMemory(2 * sizeof(Vertex));
    debugCollVerts[0].x = debugCX;
    debugCollVerts[0].y = debugCY - currentScroll;
    debugCollVerts[0].z = 5;
    debugCollVerts[1].x = debugCX + LEVEL_BLOCK_SIZE;
    debugCollVerts[1].y = debugCY - currentScroll + LEVEL_BLOCK_SIZE;
    debugCollVerts[1].z = 5;
    sceGuColor(0xA0FF0000);
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, debugCollVerts);


    sceGuEnable(GU_TEXTURE_2D);

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
