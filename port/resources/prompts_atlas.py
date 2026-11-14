#!/usr/bin/env python3
# Builds prompts_colour.png and prompts_1bit.png, one row of 16 pixel high cells in port/src/prompts.cpp's
# order, from Kenney's Input Prompts Pixel packs (CC0): prompts_atlas.py <pack folder> <1-Bit pack folder>.
import sys
from PIL import Image

# Tile numbers in the packs' Tilemap/*_packed.png; a pair overlaid is one picture (PlayStation's symbols).
XBOX = [4, 5, 6, 7, 553, 554, 616, 617, 551, 552]
PLAYSTATION = [(567, 568), (563, 564), (565, 566), (561, 562), 631, 632, 767, 768, 629, 630]
NINTENDO = [13, 14, 15, 16, 627, 628, 684, 685, 625, 626]
STICKS = [416, 484]
ESCAPE = 17
DELETE = (191, 192)
LETTERS = [120, 159, 157, 122, 87, 123, 124, 125, 92, 126, 127, 128, 161, 160, 93, 94, 85, 88, 121, 89, 91,
           158, 86, 156, 90, 155]


def tile(sheet, n):
    return sheet.crop((n % 34 * 16, n // 34 * 16, n % 34 * 16 + 16, n // 34 * 16 + 16))


def overlaid(sheet, pair):
    picture = tile(sheet, pair[0])
    picture.alpha_composite(tile(sheet, pair[1]))
    return picture


def wide(sheet, left, right):
    picture = Image.new('RGBA', (32, 16))
    picture.paste(tile(sheet, left), (0, 0))
    picture.paste(tile(sheet, right), (16, 0))
    return picture


def arrow_rows(picture, top, face):
    rows = [y for y in range(top, 10) for x in range(3, 13) if picture.getpixel((x, y)) != face]
    return min(rows), max(rows)


def enter(sheet):
    # Backspace's key with the return arrow of the ISO Enter's lower half, centred as backspace's arrow.
    picture = wide(sheet, 66, 67)
    lower = tile(sheet, 134)
    face = picture.getpixel((5, 3))
    top, bottom = arrow_rows(picture, 3, face)
    arrow_top, arrow_bottom = arrow_rows(lower, 0, face)
    shift = (top + bottom) // 2 - (arrow_top + arrow_bottom) // 2
    for y in range(top, bottom + 1):
        for x in range(3, 13):
            picture.putpixel((x, y), face)
    for y in range(arrow_top, arrow_bottom + 1):
        for x in range(3, 13):
            if lower.getpixel((x, y)) != face:
                picture.putpixel((x, y + shift), lower.getpixel((x, y)))
    return picture


def atlas(sheet):
    cells = []
    for pad in (XBOX, PLAYSTATION, NINTENDO):
        cells += [overlaid(sheet, n) if isinstance(n, tuple) else tile(sheet, n) for n in pad]
    cells += [tile(sheet, n) for n in STICKS]
    cells += [enter(sheet), tile(sheet, ESCAPE), wide(sheet, *DELETE)]
    cells += [tile(sheet, n) for n in LETTERS]
    out = Image.new('RGBA', (sum(c.width for c in cells), 16))
    x = 0
    for c in cells:
        out.paste(c, (x, 0))
        x += c.width
    return out


colour = Image.open(sys.argv[1] + '/Tilemap/tilemap_packed.png').convert('RGBA')
atlas(colour).save(sys.argv[0].rsplit('/', 1)[0] + '/prompts_colour.png', optimize=True)
white = Image.open(sys.argv[2] + '/Tilemap/tilemap_white_packed.png').convert('RGBA')
atlas(white).save(sys.argv[0].rsplit('/', 1)[0] + '/prompts_1bit.png', optimize=True)
