# JumpKing decompilation notes

## Blocks

A block is a group of **8x8 pixels**. There are various types of blocks:

- Solid blocks
- Slopes
- Fake blocks
- Ice blocks (slippery)
- Snow blocks (inhibit walking)
- Wind tiles
- Sand blocks
- No wind tiles (cancel wind tiles effect)
- Water blocks
- Quark blocks

## Screens

A screen is **60 blocks wide (or 480 pixels)** and **45 blocks tall (or 360 pixels)**.  
Each screen can have:

- a foreground texture
- a midground texture (`background` in code)
- a background texture (`backbackground` in code)
- a scrolling background object
- a weather property

## Levels

Each level as a set number of screens, that are loaded from the `level.xnb` image file. 
**The screens are stored as columns of 14 tiles (each with a size of 60x45 pixels)**, with the top-left tile being the first screen of the first level.
Each pixel in a tile marks a block in the screen. The block type is determined by the color of the pixel:

| R | G | B | Block   |
|---|---|---|---------|
| 0 | 0 | 0 | Solid   |
|255| 0 | 0 | Slope   |
|128|128|128| Fake    |
| 0 |255|255| Ice     |
|255|255| 0 | Snow    |
| 0 |255| 0 | Wind    |
|255|106| 0 | Sand    |
|255|255|255| No Wind |
| 0 |170|170| Water   |
|182|255| 0 | Quark   |
|XXX| 0 |255| **Teleport Link** |

The teleport link is a special tile that is used to make the entrance effects for the New Babe+ and Ghost of the Babe maps.  
The red channel of the teleport link tile, stores the destination screen index.
