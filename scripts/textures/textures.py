#!/bin/env python3

import os
import pathlib
import json
import argparse
import imageio.v3 as iio
import numpy as np
import math
import qoi

def optional_key(json_data, key, default):
    try:
        return json_data[key]
    except:
        return default

def require_key(json_data, key):
    try:
        return json_data[key]
    except:
        print("Cannot find key '{}' in json data".format(key))
        exit(-1)

def check_key(json_data, key):
    return json_data.get(key) is not None

def swizzle(in_pixels):
    in_shape = in_pixels.shape
    width = in_shape[1]
    height = in_shape[0]
    bpp = in_shape[2]
    bytes_width = width * bpp
    row_blocks = int(bytes_width / 16);
    in_pixels = in_pixels.flatten()
    swizzled_pixels = np.zeros(bytes_width * height, dtype=np.uint8)
    for j in range(height):
        for i in range(bytes_width):
            block_x = i >> 4
            block_y = j >> 3
            x = i - (block_x << 4)
            y = j - (block_y << 3)
            block_index = block_x + (block_y * row_blocks)
            block_offset = block_index * 16 * 8;
            swizzled_pixels[block_offset + x + y * 16] = in_pixels[i + j * bytes_width]
    return swizzled_pixels.reshape(in_shape)

def to_pow2_size(size):
    width2 = math.log2(size.x)
    height2 = math.log2(size.y)
    if width2 - int(width2) > 0:
        size.x = int(pow(2, math.ceil(width2)))
    if height2 - int(height2) > 0:
        size.y = int(pow(2, math.ceil(height2)))
    return size

def pad_arr(arr, shape):
    for axis in range(len(shape)):
        axis_pad = shape[axis]
        if axis_pad == 0:
            continue
        elif axis_pad > 0:
            axis_shape = [x for x in arr.shape]
            axis_shape[axis] = abs(axis_pad)
            pad = np.zeros(axis_shape, dtype=np.uint8)
            arr = np.append(arr, pad, axis=axis)
        else:
            axis_pad = abs(axis_pad)
            axis_shape = [x for x in arr.shape[axis+1:]]
            axis_indices = [0 for _ in range(axis_pad)]
            pad = np.zeros(axis_shape, dtype=np.uint8)
            arr = np.insert(arr, axis_indices, pad, axis=axis)
    return arr

class Vector2:
    def __init__(self, x, y):
        self.x = x
        self.y = y

class Tile:
    def __init__(self, image, offset, size, ypad, xpad):
        self._pixels = []
        for y in range(size.y):
            self._pixels.append([])
            for x in range(size.x):
                self._pixels[y].append([])
                self._pixels[y][x] = image[offset.y + y][offset.x + x]
        self._size = Vector2(size.x, size.y)
        self._xpad = xpad
        self._ypad = ypad

    def crop_to(self, new_size):
        horizontal_delta = new_size.x - self._size.x
        vertical_delta = new_size.y - self._size.y

        if horizontal_delta > 0:
            self._grow_horizontally(horizontal_delta)
        elif horizontal_delta < 0:
            self._shrink_horizontally(-horizontal_delta)
        self._size.x = new_size.x

        if vertical_delta > 0:
            self._grow_vertically(vertical_delta)
        elif vertical_delta < 0:
            self._shrink_vertically(-vertical_delta)
        self._size.y = new_size.y

    def _grow_horizontally(self, horizontal_delta):
        if self._xpad == "center":
            horizontal_delta = horizontal_delta >> 1
        if self._xpad == "left" or self._xpad == "center":
            self._pixels = pad_arr(self._pixels, (0, -horizontal_delta))
        if self._xpad == "right" or self._xpad == "center":
            self._pixels = pad_arr(self._pixels, (0, +horizontal_delta))
    
    def _shrink_horizontally(self, horizontal_delta):
        if self._xpad == "center":
            horizontal_delta = horizontal_delta >> 1
        if self._xpad == "left" or self._xpad == "center":
            for _ in range(horizontal_delta):
                self._pixels = np.delete(self._pixels, 0, axis=1)
        if self._xpad == "right" or self._xpad == "center":
            for _ in range(horizontal_delta):
                self._pixels = np.delete(self._pixels, self._pixels.shape[1] - 1, axis=1)
    
    def _grow_vertically(self, vertical_delta):
        if self._ypad == "center":
            vertical_delta = vertical_delta >> 1
        if self._ypad == "top" or self._ypad == "center":
            self._pixels = pad_arr(self._pixels, (-vertical_delta, 0))
        if self._ypad == "bottom" or self._ypad == "center":
            self._pixels = pad_arr(self._pixels, (+vertical_delta, 0))

    def _shrink_vertically(self, vertical_delta):
        if self._ypad == "center":
            vertical_delta = vertical_delta >> 1
        if self._ypad == "top" or self._ypad == "center":
            for _ in range(vertical_delta):
                self._pixels = np.delete(self._pixels, 0, axis=0)
        if self._ypad == "bottom" or self._ypad == "center":
            for _ in range(vertical_delta):
                self._pixels = np.delete(self._pixels, self._pixels.shape[0] - 1, axis=0)
    
    def get_bounding_box(self):
        top_left = Vector2(0, 0)
        bottom_right = Vector2(self._size.x, self._size.y)
        left_boundary_found = False
        right_boundary_found = False
        for x in range(self._size.x):
            for y in range(self._size.y):
                left_boundary_found = left_boundary_found or self._pixels[y][x][3] > 0
                right_boundary_found = right_boundary_found or self._pixels[y][(self._size.x - 1) - x][3] > 0
            if not left_boundary_found:
                top_left.x = x
            if not right_boundary_found:
                bottom_right.x = self._size.x - x
        top_boundary_found = False
        bottom_boundary_found = False
        for y in range(self._size.y):
            for x in range(self._size.x):
                top_boundary_found = top_boundary_found or self._pixels[y][x][3] > 0
                bottom_boundary_found = bottom_boundary_found or self._pixels[(self._size.y - 1) - y][x][3] > 0
            if not top_boundary_found:
                top_left.y = y
            if not bottom_boundary_found:
                bottom_right.y = self._size.y - y
        return top_left, bottom_right

    def get_pixels(self):
        return self._pixels

class Tilemap:
    def __init__(self, json_data):
        self._name = require_key(json_data, "name")
        offset = require_key(json_data, "offset")
        self._offset = Vector2(offset[0], offset[1])
        tile_size = require_key(json_data, "tileSize")
        self._tile_size = Vector2(tile_size[0], tile_size[1])
        size_in_tiles = require_key(json_data, "sizeInTiles")
        self._size_in_tiles = Vector2(size_in_tiles[0], size_in_tiles[1])
        self._total_tiles = require_key(json_data, "totalTiles")
        self._horizontal_padding = require_key(json_data, "xpad")
        self._vertical_padding = require_key(json_data, "ypad")
    
    def _generate_image(self, tiles, top_left, bottom_right):
        pixels = []
        desired_size = Vector2(bottom_right.x - top_left.x - 1, bottom_right.y - top_left.y - 1)
        new_size = to_pow2_size(desired_size)
        for tile in tiles:
            tile.crop_to(new_size)
            pixels.extend(tile.get_pixels())
        rgba = np.array(pixels, dtype=np.uint8)
        return rgba

    def extract(self, image):
        print(" \\-> extracting {} tiles from tilemap '{}'".format(
            self._total_tiles, self._name))
        tilestrip_images = []
        tiles_read = 0
        top_left = Vector2(self._tile_size.x, self._tile_size.y)
        bottom_right = Vector2(0, 0)
        for y in range(self._size_in_tiles.y):
            for x in range(self._size_in_tiles.x):
                tile_offset = Vector2(self._offset.x + x * self._tile_size.x, self._offset.y + y * self._tile_size.y)
                tile = Tile(image, tile_offset, self._tile_size, self._vertical_padding, self._horizontal_padding)
                tile_top_left, tile_bottom_right = tile.get_bounding_box()
                top_left.x = min(tile_top_left.x, top_left.x)
                top_left.y = min(tile_top_left.y, top_left.y)
                bottom_right.x = max(tile_bottom_right.x, bottom_right.x)
                bottom_right.y = max(tile_bottom_right.y, bottom_right.y)
                tilestrip_images.append(tile)
                tiles_read += 1
                if tiles_read == self._total_tiles:
                    return self._generate_image(tilestrip_images, top_left, bottom_right)
    
    def name(self):
        return self._name

class Screen:
    def __init__(self, json_data):
        self._number = require_key(json_data, "number")
        self._size = require_key(json_data, "size")
        self._swizzle = optional_key(json_data, "swizzle", True)
    
    def extract(self, background_image, foreground_image=None):
        print(" \\-> extracting screen #{} {} foreground".format(
            self._number,
            "without" if foreground_image is None else "with"
        ))
        width = background_image.shape[1]
        height = background_image.shape[0]
        if foreground_image is not None:
            width = max(width, foreground_image.shape[1])
            height = max(height, foreground_image.shape[0])
        new_size = to_pow2_size(Vector2(width, height))
        x_delta = new_size.x - width
        y_delta = new_size.y - height
        rgba = pad_arr(background_image, (0, x_delta))
        if foreground_image is not None:
            foreground_image = pad_arr(foreground_image, (0, x_delta))
            rgba = np.concatenate((rgba, foreground_image), axis=0)
        if self._swizzle is True:
            rgba = swizzle(rgba)
        return rgba
    
    def number(self):
        return str(self._number)

class TextureFile:
    def __init__(self, json_data, input_folder):
        self._type = optional_key(json_data, "type", "screen")
        if self._type == "screen":
            self._background_files = require_key(json_data, "background")
            self._foreground_file = optional_key(json_data, "foreground", None)
            self._background_paths = [pathlib.Path(input_folder).joinpath(file) for file in self._background_files]
            if self._foreground_file is not None:
                self._foreground_path = pathlib.Path(input_folder).joinpath(self._foreground_file)
            texture_data = require_key(json_data, "screen")
            self._screen = Screen(texture_data)
        elif self._type == "tilemap":
            self._tilemaps = []
            self._file = require_key(json_data, "file")
            self._path = pathlib.Path(input_folder).joinpath(self._file)
            tilemaps = require_key(json_data, "tilemaps")
            for tilemap in tilemaps:
                self._tilemaps.append(Tilemap(tilemap)) 
        else:
            print("Unknown texture type '{}'".format(self._type))
            exit(-1)
    
    def _extract_all_tilemaps(self, output_path):
        print("Extracting tilemaps from file: {}".format(self._file))
        output_folder = pathlib.Path(output_path)
        relative_output = self._file.split(".")[0]
        output_folder = output_folder.joinpath(relative_output)
        if not output_folder.exists():
            output_folder.mkdir(parents=True, exist_ok=True)
        image = iio.imread(self._path, mode="RGBA")
        for tilemap in self._tilemaps:
            output_path = output_folder.joinpath(tilemap.name() + ".qoi")
            rgba = tilemap.extract(image)
            qoi.write(output_path, rgba)

    def _extract_screen(self, output_path):
        print("Extracting screen from file(s): \n |> {}".format("\n |> ".join(self._background_files)))
        background_image = np.copy(iio.imread(self._background_paths[0], mode="RGBA"))
        for i in range(1, len(self._background_paths)):
            composite_image = iio.imread(self._background_paths[i], mode="RGBA")
            if composite_image.shape != background_image.shape:
                print("Incompatible image sizes:\n {} has size {} while {} has size {}".format(
                    self._background_files[0], str(background_image.shape), self._background_files[i], str(composite_image.shape)))
                exit(-1)
            for y in range(background_image.shape[0]):
                for x in range(background_image.shape[1]):
                    pixel_color = background_image[y][x]
                    if pixel_color[3] == 0:
                        pixel_color = composite_image[y][x]
                        background_image[y][x] = pixel_color
        if self._foreground_file is not None:
            foreground_image = iio.imread(self._foreground_path, mode="RGBA")
            rgba = self._screen.extract(background_image, foreground_image=foreground_image)
        else:
            rgba = self._screen.extract(background_image)
        output_folder = pathlib.Path(output_path)
        relative_output = self._background_files[0].split("/")[0]
        output_folder = output_folder.joinpath(relative_output)
        if not output_folder.exists():
            output_folder.mkdir(parents=True, exist_ok=True)
        output_path = output_folder.joinpath(self._screen.number() + ".qoi")
        qoi.write(output_path, rgba)

    def extract_all(self, output_path):
        if self._type == "tilemap":
            self._extract_all_tilemaps(output_path)
        else:
            self._extract_screen(output_path)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-i", "--input", help="Input folder", required=True)
    parser.add_argument("-o", "--output", help="Output folder", required=True)
    parser.add_argument("-d", "--descriptor", help="Atlas descriptor file", required=True)
    args = vars(parser.parse_args())

    descriptor_path = args["descriptor"]
    input_folder = args["input"]
    output_folder = args["output"]

    json_text = ""
    with open(descriptor_path) as descriptor:
        for line in descriptor:
            json_text += line
    
    try:
        json_data = json.loads(json_text)
    except Exception as e:
        print("Error: failed to read descriptor file: {}.".format(str(e)))
        exit(-1)

    for texture_json in json_data:
        if check_key(texture_json, "file") is True:
            files = [require_key(texture_json, "file")] 
        else:
            files = require_key(texture_json, "background")
            foreground_file = optional_key(json_data, "foreground", None)
            if foreground_file is not None:
                files.append(foreground_file)
        for file in files:
            texture_path = pathlib.Path(input_folder).joinpath(file)
            if not texture_path.exists():
                print("Error: file '{}' does not exist.".format(texture_path))
                exit(-1)
        texture_file = TextureFile(texture_json, input_folder)
        texture_file.extract_all(output_folder)
