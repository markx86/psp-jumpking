#!/usr/bin/python3

import os
import pathlib
import json
import argparse
import imageio.v3 as iio
import numpy as np
import math
import qoi

def optional_key(json_data, key, default):
    if json_data.get(key) is not None:
        return json_data[key]
    return default

def require_key(json_data, key):
    if json_data.get(key) is not None:
        return json_data[key]
    print("Cannot find key '{}' in json data".format(key))
    exit(-1)

def check_key(json_data, key):
    return json_data.get(key) is not None

def swizzle(in_pixels, width, height):
    bytes_width = width * 4
    row_blocks = int(bytes_width / 16);
    swizzled_pixels = [0 for _ in range(bytes_width * height)]
    for j in range(height):
        for i in range(bytes_width):
            block_x = int(i / 16)
            block_y = int(j / 8)
            x = (i - block_x * 16)
            y = (j - block_y * 8)
            block_index = block_x + (block_y * row_blocks)
            block_offset = block_index * 16 * 8;
            swizzled_pixels[block_offset + x + y * 16] = in_pixels[i + j * bytes_width]
    out_pixels = []
    for j in range(height):
        line = []
        for i in range(width):
            line_offset = j * bytes_width + i * 4
            line.append([
                swizzled_pixels[line_offset], 
                swizzled_pixels[line_offset + 1], 
                swizzled_pixels[line_offset + 2], 
                swizzled_pixels[line_offset + 3]
            ])
        out_pixels.append(line)
    return np.array(out_pixels, dtype=np.uint8)

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
            horizontal_delta = round(horizontal_delta / 2)
        if self._xpad == "left" or self._xpad == "center":
            self._pixels = np.insert(self._pixels, [0 for _ in range(horizontal_delta)], [0, 0, 0, 0], axis=1)
        if self._xpad == "right" or self._xpad == "center":
            self._pixels = np.append(self._pixels, [[[0, 0, 0, 0] for _ in range(horizontal_delta)] for _ in range(self._size.y)], axis=1)
    
    def _shrink_horizontally(self, horizontal_delta):
        if self._xpad == "center":
            horizontal_delta = round(horizontal_delta / 2)
        if self._xpad == "left" or self._xpad == "center":
            for _ in range(horizontal_delta):
                self._pixels = np.delete(self._pixels, 0, axis=1)
        if self._xpad == "right" or self._xpad == "center":
            for _ in range(horizontal_delta):
                self._pixels = np.delete(self._pixels, self._pixels.shape[1] - 1, axis=1)
    
    def _grow_vertically(self, vertical_delta):
        if self._ypad == "center":
            vertical_delta = round(vertical_delta / 2)
        if self._ypad == "top" or self._ypad == "center":
            self._pixels = np.insert(self._pixels, [0 for _ in range(vertical_delta)], [[0, 0, 0, 0] for _ in range(self._size.x)], axis=0)
        if self._ypad == "bottom" or self._ypad == "center":
            self._pixels = np.append(self._pixels, [[[0, 0, 0, 0] for _ in range(self._size.x)] for _ in range(vertical_delta)], axis=0)

    def _shrink_vertically(self, vertical_delta):
        if self._ypad == "center":
            vertical_delta = round(vertical_delta / 2)
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
    
    def _generate_image(self, tiles, top_left, bottom_right, output_folder, should_swizzle):
        pixels = []
        new_size = Vector2(bottom_right.x - top_left.x - 1, bottom_right.y - top_left.y - 1)
        width2 = math.log2(new_size.x)
        height2 = math.log2(new_size.y)
        if width2 - int(width2) > 0:
            new_size.x = int(pow(2, math.ceil(width2)))
        if height2 - int(height2) > 0:
            new_size.y = int(pow(2, math.ceil(height2)))
        for tile in tiles:
            tile.crop_to(new_size)
            pixels.extend(tile.get_pixels())
        output_path = output_folder.joinpath(self._name + ".qoi")
        rgba = np.array(pixels, dtype=np.uint8)
        if should_swizzle is True:
            rgba = swizzle(rgba.flatten(), new_size.x, new_size.y)
        qoi.write(output_path, rgba)

    def extract(self, image, output_folder, should_swizzle):
        print("  -> extracting {} {} from tilemap '{}' {} swizzle".format(
            self._total_tiles, 
            "image" if self._total_tiles == 1 else "tiles", 
            self._name, "with" if should_swizzle == True else "without"))
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
                    return self._generate_image(tilestrip_images, top_left, bottom_right, output_folder, should_swizzle)

class TextureFile:
    def __init__(self, json_data, input_folder):
        self._type = optional_key(json_data, "type", "single")
        self._files = require_key(json_data, "files") if self._type == "composite" else [require_key(json_data, "file")]
        self._paths = [pathlib.Path(input_folder).joinpath(file) for file in self._files]
        self._swizzle = optional_key(json_data, "swizzle", True)
        self._tilemaps = []
        if self._type == "single" or self._type == "composite":
            texture_data = require_key(json_data, "texture")
            tilemap = {
                "name": texture_data["name"],
                "offset": [0, 0],
                "tileSize": texture_data["size"],
                "sizeInTiles": [1, 1],
                "totalTiles": 1,
                "xpad": "right",
                "ypad": "bottom"
            }
            self._tilemaps.append(Tilemap(tilemap))
            self._is_texture = True
        elif self._type == "container":
            tilemaps = require_key(json_data, "tilemaps")
            for tilemap in tilemaps:
                self._tilemaps.append(Tilemap(tilemap)) 
            self._is_texture = False
        else:
            print("Unknown texture type '{}'", self._type)
            exit(-1)
    
    def extract_all(self, output_path):
        print("Extracting {} from file(s): {}".format(
            "tilemaps" if self._type == "container" else self._type + " texture", 
            self._files[0] if len(self._files) == 1 else "\n > " + "\n > ".join(self._files)))
        if self._type == "composite":
            image = np.copy(iio.imread(self._paths[0]))
            for i in range(1, len(self._paths)):
                composite_image = iio.imread(self._paths[i], mode="RGBA")
                if composite_image.shape != image.shape:
                    print("Incompatible image sizes:\n -> {} has size {} while {} has size {}".format(
                        self._files[0], str(image.shape), self._files[i], str(composite_image.shape)))
                    exit(-1)
                for y in range(image.shape[0]):
                    for x in range(image.shape[1]):
                        pixel_color = image[y][x]
                        if pixel_color[3] == 0:
                            pixel_color = composite_image[y][x]
                            image[y][x] = pixel_color
        else:
            image = iio.imread(self._paths[0], mode="RGBA")
        output_folder = pathlib.Path(output_path)
        if not self._is_texture:
            relative_output = self._files[0].split(".")[0]
        else:
            relative_output = os.path.split(self._files[0])[0]
        output_folder = output_folder.joinpath(relative_output)
        if not output_folder.exists():
            output_folder.mkdir(parents=True, exist_ok=True)
        for tilemap in self._tilemaps:
            tilemap.extract(image, output_folder, self._swizzle)

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
        files = [require_key(texture_json, "file")] if check_key(texture_json, "file") is True else require_key(texture_json, "files")
        for file in files:
            texture_path = pathlib.Path(input_folder).joinpath(file)
            if not texture_path.exists():
                print("Error: file '{}' does not exist.".format(texture_path))
                exit(-1)
        texture_file = TextureFile(texture_json, input_folder)
        texture_file.extract_all(output_folder)
