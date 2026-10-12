#!/usr/bin/env python3
# syobon-action-ps5 - home-screen art from the game's sprites.
# Copyright (C) 2026 Andrei Kvapil
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compose the home-screen art from the game's own sprites.

    python3 art/compose.py OUTPUT_DIR

Writes icon0.png (512x512), background-source.png and
launch-background-source.png (3840x2160). Everything is laid out at the
game's pixel scale and enlarged without smoothing.
"""
import os
import sys

from PIL import Image

RES = os.path.join(os.path.dirname(__file__), "..", "assets", "res")
SKY = (160, 180, 250)
KEY = (153, 255, 255)
GREEN = (0, 230, 0)


def sheet(name):
    return Image.open(os.path.join(RES, name)).convert("RGB")


def sprite(image, x, y, w, h):
    """A piece of a sheet, its colour key made transparent."""
    piece = image.crop((x, y, x + w, y + h)).convert("RGBA")
    piece.putdata([(r, g, b, 0) if (r, g, b) == KEY else (r, g, b, 255)
                   for r, g, b, _ in piece.get_flattened_data()])
    return piece


player = sheet("player.PNG")
blocks = sheet("brock.PNG")
scenery = sheet("haikei.PNG")
logo = sprite(sheet("syobon3.PNG"), 0, 0, 378, 56)

cat = sprite(player, 0, 0, 30, 36)
ground_top = sprite(blocks, 33 * 5, 0, 30, 30)
ground = sprite(blocks, 33 * 6, 0, 30, 30)
brick = sprite(blocks, 33 * 1, 0, 30, 30)
question = sprite(blocks, 33 * 2, 0, 30, 30)
hill = sprite(scenery, 0, 0, 150, 90)
grass = sprite(scenery, 151, 0, 65, 29)
cloud = sprite(scenery, 151, 113, 51, 29)
face_cloud = sprite(scenery, 151, 31, 70, 40)


def lay_ground(canvas, top):
    for x in range(0, canvas.width, 29):
        canvas.alpha_composite(ground_top, (x, top))
        canvas.alpha_composite(ground, (x, top + 29))


def pipe(canvas, x, top, width, height):
    """A pipe as the game draws one: a green box with a black outline."""
    canvas.paste(GREEN + (255,), (x, top, x + width, top + height))
    for px in range(x, x + width):
        canvas.putpixel((px, top), (0, 0, 0, 255))
    for py in range(top, top + height):
        canvas.putpixel((x, py), (0, 0, 0, 255))
        canvas.putpixel((x + width - 1, py), (0, 0, 0, 255))


def enlarge(canvas, size):
    return canvas.resize(size, Image.NEAREST).convert("RGB")


def selection_background():
    # The console draws the title and Play on the left: the scene keeps right
    canvas = Image.new("RGBA", (480, 270), SKY + (255,))
    floor = 270 - 58
    canvas.alpha_composite(cloud, (150, 40))
    canvas.alpha_composite(face_cloud, (330, 22))
    canvas.alpha_composite(hill, (175, floor - 90))
    canvas.alpha_composite(grass, (110, floor - 29))
    pipe(canvas, 400, floor - 60, 50, 60)
    for i, block in enumerate((brick, question, brick)):
        canvas.alpha_composite(block, (290 + 29 * i, floor - 120))
    canvas.alpha_composite(cat, (330, floor - 36))
    lay_ground(canvas, floor)
    return enlarge(canvas, (3840, 2160))


def launch_background():
    # The title screen, widened to 16:9
    canvas = Image.new("RGBA", (480, 270), SKY + (255,))
    floor = 270 - 58
    canvas.alpha_composite(logo, (240 - 378 // 2, 40))
    canvas.alpha_composite(cloud, (40, 120))
    canvas.alpha_composite(hill, (300, floor - 90))
    canvas.alpha_composite(grass, (170, floor - 29))
    canvas.alpha_composite(cat, (70, floor - 36))
    lay_ground(canvas, floor)
    return enlarge(canvas, (3840, 2160))


def icon():
    canvas = Image.new("RGBA", (64, 64), SKY + (255,))
    for x in range(-2, 64, 29):
        canvas.alpha_composite(ground_top, (x, 52))
    canvas.alpha_composite(cat, (17, 16))
    return enlarge(canvas, (512, 512))


out = sys.argv[1]
icon().save(os.path.join(out, "icon0.png"))
selection_background().save(os.path.join(out, "background-source.png"))
launch_background().save(os.path.join(out, "launch-background-source.png"))
