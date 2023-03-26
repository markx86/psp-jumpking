#!/usr/bin/python3

import pathlib
import argparse
import imageio.v3 as iio

TOTAL_SCREENS = 164

TILE_WIDTH = 60
TILE_HEIGHT = 45

BLOCK_SLOPE_TL = 0
BLOCK_SLOPE_TR = 1
BLOCK_SLOPE_BL = 2
BLOCK_SLOPE_BR = 3
BLOCK_EMPTY = 4
BLOCK_SOLID = 5
BLOCK_FAKE = 6
BLOCK_ICE = 7
BLOCK_SNOW = 8
BLOCK_SAND = 9
BLOCK_NOWIND = 10
BLOCK_WATER = 11
BLOCK_QUARK = 12

class Color:
    def __init__(self, vec):
        self.r = vec[0]
        self.g = vec[1]
        self.b = vec[2]
        self.a = 255
        if len(vec) == 4:
            self.a = vec[3]
    
    def __eq__(self, other):
        return self.r == other.r and self.g == other.g and self.b == other.b and self.a == other.a

RGBA_EMPTY = Color([0, 0, 0, 0])
RGB_SOLID = Color([0, 0, 0])
RGB_SLOPE = Color([255, 0, 0])
RGB_FAKE = Color([128, 128, 128])
RGB_ICE = Color([0, 255, 255])
RGB_SNOW = Color([255, 255, 0])
RGB_WIND = Color([0, 255, 0])
RGB_SAND = Color([255, 106, 0])
RGB_NOWIND = Color([255, 255, 255])
RGB_WATER = Color([0, 170, 170])
RGB_QUARK = Color([182, 255, 0])

class Line:
    def __init__(self, x, y, l, b):
        self.x = x
        self.y = y
        self.length = l
        self.block = b
        self._free = True
    
    def take(self):
        self._free = False
    
    def free(self):
        return self._free
    
    def __str__(self):
        return "Line(x:{}, y:{}, l:{}, b:{})".format(self.x, self.y, self.length, self.block)

class Screen:
    def __init__(self, rgba, mx, my):
        self._blocks = []
        self._teleport = 255
        self._wind = 0
        for y in range(my, my + TILE_HEIGHT):
            for x in range(mx, mx + TILE_WIDTH):
                c = Color(rgba[y][x])
                if c.g == 0 and c.b == 255:
                    self._teleport = c.r
                    self._blocks.append(BLOCK_EMPTY)
                elif c == RGBA_EMPTY:
                    self._blocks.append(BLOCK_EMPTY)
                elif c == RGB_SOLID:
                    self._blocks.append(BLOCK_SOLID)
                elif c == RGB_SLOPE:
                    if is_block_edge(rgba, x + 1, y) and is_block_edge(rgba, x, y + 1):
                        self._blocks.append(BLOCK_SLOPE_TL)
                    elif is_block_edge(rgba, x - 1, y) and is_block_edge(rgba, x, y + 1):
                        self._blocks.append(BLOCK_SLOPE_TR)
                    elif is_block_edge(rgba, x + 1, y) and is_block_edge(rgba, x, y - 1):
                        self._blocks.append(BLOCK_SLOPE_BL)
                    elif is_block_edge(rgba, x - 1, y) and is_block_edge(rgba, x, y - 1):
                        self._blocks.append(BLOCK_SLOPE_BR)
                    else:
                        print("Error: unknown slope type at {}:{}".format(x, y))
                        exit(-1)
                elif c == RGB_FAKE:
                    self._blocks.append(BLOCK_FAKE)
                elif c == RGB_ICE:
                    self._blocks.append(BLOCK_ICE)
                elif c == RGB_SNOW:
                    self._blocks.append(BLOCK_SNOW)
                elif c == RGB_WIND:
                    self._wind = 1
                    self._blocks.append(BLOCK_EMPTY)
                elif c == RGB_SAND:
                    self._blocks.append(BLOCK_SAND)
                elif c == RGB_NOWIND:
                    self._blocks.append(BLOCK_NOWIND)
                elif c == RGB_WATER:
                    self._blocks.append(BLOCK_WATER)
                elif c == RGB_QUARK:
                    self._blocks.append(BLOCK_QUARK)
                else:
                    print("Error: unknown block with color [{}, {}, {}]".format(c.r, c.g, c.b))

    def to_bytearray(self):
        arr = []
        byte1 = 0xBE
        byte2 = 0xBA
        byte3 = self._wind
        byte4 = self._teleport
        arr.append(byte1)
        arr.append(byte2)
        arr.append(byte3)
        arr.append(byte4)
        arr.extend(self._blocks)
        return bytearray(arr)

def is_block_solid(block):
    return not (block == BLOCK_EMPTY or block == BLOCK_FAKE or block == BLOCK_NOWIND or block == BLOCK_QUARK or block == BLOCK_WATER)

def is_block_slope(block):
    return (block == BLOCK_SLOPE_TL or block == BLOCK_SLOPE_BL or block == BLOCK_SLOPE_TR or block == BLOCK_SLOPE_BR)

def is_block_edge(rgba, x, y):
    if x >= TILE_WIDTH or y >= TILE_HEIGHT:
        return True
    elif x < 0 or y < 0:
        return True
    c = Color(rgba[y][x])
    if c == RGB_SOLID or c == RGB_SLOPE or c == RGB_ICE or c == RGB_SNOW or c == RGB_SAND or c == RGB_FAKE:
        return True
    else:
        return False

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-i", "--input", help="Input file", required=True)
    parser.add_argument("-o", "--output", help="Output folder", required=True)
    args = vars(parser.parse_args())
    
    input_file = pathlib.Path(args["input"])
    if not input_file.exists():
        print("Error: input file does not exist!")
        exit(-1)
    
    output_path = pathlib.Path(args["output"])
    if not output_path.exists():
        output_path.mkdir(parents=True)
    output_file = output_path.joinpath("level.bin")
    
    rgba = iio.imread(input_file, mode="RGBA")
    width = rgba.shape[1]
    height = rgba.shape[0]

    screens = 0
    with open(output_file, "wb") as file:
        for x in range(0, width, TILE_WIDTH):
            for y in range(0, height, TILE_HEIGHT):
                screen = Screen(rgba, x, y)
                bytes_arr = screen.to_bytearray()
                file.write(bytes_arr)
                screens += 1
                if screens == TOTAL_SCREENS:
                    break
            if screens == TOTAL_SCREENS:
                break
        file.flush()
