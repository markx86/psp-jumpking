#!/usr/bin/python3

import os
import pathlib
import json
import argparse
import imageio.v3 as iio
import numpy as np
import math
import qoi

class Vector2:
    def __init__(self, x, y):
        self.x = x
        self.y = y

class Tile:
    def __init__(self, image, offset, size, vpad, hpad):
        self._pixels = []
        for y in range(size.y):
            self._pixels.append([])
            for x in range(size.x):
                self._pixels[y].append([])
                self._pixels[y][x] = image[offset.y + y][offset.x + x]
        self._size = Vector2(size.x, size.y)
        self._hpad = hpad
        self._vpad = vpad

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
        if self._hpad == "center":
            horizontal_delta = round(horizontal_delta / 2)
        if self._hpad == "left" or self._hpad == "center":
            self._pixels = np.insert(self._pixels, [0 for _ in range(horizontal_delta)], [0, 0, 0, 0], axis=1)
        if self._hpad == "right" or self._hpad == "center":
            self._pixels = np.append(self._pixels, [[[0, 0, 0, 0] for _ in range(horizontal_delta)] for _ in range(self._size.y)], axis=1)
    
    def _shrink_horizontally(self, horizontal_delta):
        if self._hpad == "center":
            horizontal_delta = round(horizontal_delta / 2)
        if self._hpad == "left" or self._hpad == "center":
            for _ in range(horizontal_delta):
                self._pixels = np.delete(self._pixels, 0, axis=1)
        if self._hpad == "right" or self._hpad == "center":
            for _ in range(horizontal_delta):
                self._pixels = np.delete(self._pixels, self._pixels.shape[1] - 1, axis=1)
    
    def _grow_vertically(self, vertical_delta):
        if self._vpad == "center":
            vertical_delta = round(vertical_delta / 2)
        if self._vpad == "top" or self._vpad == "center":
            self._pixels = np.insert(self._pixels, [0 for _ in range(vertical_delta)], [[0, 0, 0, 0] for _ in range(self._size.x)], axis=0)
        if self._vpad == "bottom" or self._vpad == "center":
            self._pixels = np.append(self._pixels, [[[0, 0, 0, 0] for _ in range(self._size.x)] for _ in range(vertical_delta)], axis=0)

    def _shrink_vertically(self, vertical_delta):
        if self._vpad == "center":
            vertical_delta = round(vertical_delta / 2)
        if self._vpad == "top" or self._vpad == "center":
            for _ in range(vertical_delta):
                self._pixels = np.delete(self._pixels, 0, axis=0)
        if self._vpad == "bottom" or self._vpad == "center":
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
        self._name = json_data["name"]
        self._offset = Vector2(json_data["offset"][0], json_data["offset"][1])
        self._tile_size = Vector2(json_data["tileSize"][0], json_data["tileSize"][1])
        self._size_in_tiles = Vector2(json_data["sizeInTiles"][0], json_data["sizeInTiles"][1])
        self._total_tiles = json_data["totalTiles"]
        self._vertical_padding = json_data["vpad"]
        self._horizontal_padding = json_data["hpad"]
    
    def _generate_image(self, tiles, top_left, bottom_right, output_folder):
    #def _generate_image(self, tiles, output_folder):
        pixels = []
        new_size = Vector2(bottom_right.x - top_left.x - 1, bottom_right.y - top_left.y - 1)
        width2 = math.log2(new_size.x)
        height2 = math.log2(new_size.y)
        if width2 - int(width2) > 0:
            new_size.x = int(pow(2, math.ceil(width2)))
        if height2 - int(height2) > 0:
            new_size.y = int(pow(2, math.ceil(height2)))
        for tile in tiles:
            #tile_pixels = tile.get_pixels()[top_left.y:bottom_right.y]
            #for y in range(len(tile_pixels)):
            #    tile_pixels[y] = tile_pixels[y][top_left.x:bottom_right.x]
            #pixels.extend(tile_pixels)
            tile.crop_to(new_size)
            pixels.extend(tile.get_pixels())
        #output_path = output_folder.joinpath(self._name + ".png")
        output_path = output_folder.joinpath(self._name + ".qoi")
        rgba = np.array(pixels, dtype=np.uint8)
        #iio.imwrite(output_path, rgba, extension=".png")
        qoi.write(output_path, rgba)

    def extract(self, image, output_folder):
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
                    return self._generate_image(tilestrip_images, top_left, bottom_right, output_folder)
                    #return self._generate_image(tilestrip_images, output_folder)

class TextureFile:
    def __init__(self, json_data, input_folder):
        self._file = json_data["file"]
        self._path = pathlib.Path(input_folder).joinpath(self._file)
        self._tilemaps = []
        if json_data.get("texture") is not None:
            texture_data = json_data["texture"]
            tilemap = { "name": texture_data["name"], "offset": [0, 0], "tileSize": texture_data["size"], "sizeInTiles": [1, 1], "totalTiles": 1, "vpad": "bottom", "hpad": "right" }
            self._tilemaps.append(Tilemap(tilemap))
            self._is_texture = True
        else:
            for tilemap in json_data["tilemaps"]:
                self._tilemaps.append(Tilemap(tilemap)) 
            self._is_texture = False
    
    def extract_all(self, output_path):
        image = iio.imread(self._path, mode="RGBA")
        output_folder = pathlib.Path(output_path)
        if not self._is_texture:
            relative_output = self._file.split(".")[0]
        else:
            relative_output = os.path.split(self._file)[0]
        output_folder = output_folder.joinpath(relative_output)
        if not output_folder.exists():
            output_folder.mkdir(parents=True, exist_ok=True)
        for tilemap in self._tilemaps:
            tilemap.extract(image, output_folder)

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

    for file in json_data:
        texture_path = pathlib.Path(input_folder).joinpath(file["file"])
        if not texture_path.exists():
            print("Error: file '{}' does not exist.".format(texture_path))
            exit(-1)
        texture_file = TextureFile(file, input_folder)
        texture_file.extract_all(output_folder)
