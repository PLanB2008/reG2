#!/usr/bin/env python3
# Builds prompts_colour.png (16 pixel cells in prompts.cpp's order) from greatdocbrown's Gamepad UI and, for Switch,
# Kenney's Input Prompts Pixel (CC0): prompts_atlas.py <Gamepad UI folder> <gdb-playstation-3.png> <Kenney folder>
import os
import sys
from PIL import Image

# Cells' top left corners in gdb-xbox-2.png: A B X Y, LB RB, View Menu, LT RT; then the sticks' L and R.
XBOX = [(16, 48), (16, 80), (16, 32), (16, 64), (336, 80), (336, 96), (16, 96), (16, 112), (336, 48), (336, 64)]
STICKS = [(216, 32), (280, 32)]
# In gdb-playstation-3.png: cross circle square triangle, L1 R1, Share Options, L2 R2.
PLAYSTATION = [(16, 192), (16, 176), (16, 208), (16, 160), (128, 112), (128, 128), (16, 240), (16, 256), (128, 80),
               (128, 96)]
# Tile numbers in Kenney's Tilemap/tilemap_packed.png.
NINTENDO = [13, 14, 15, 16, 627, 628, 684, 685, 625, 626]
# The dark keys (the second set, not pressed) in gdb-keyboard-2.png; Enter is two cells wide.
ENTER = (240, 176)
ESCAPE = (32, 128)
DELETE = (432, 256)
LETTER_ROWS = [(160, 'QWERTYUIOP'), (176, 'ASDFGHJKL'), (192, 'ZXCVBNM')]


def cell(sheet, corner, width=16):
    return sheet.crop((corner[0], corner[1], corner[0] + width, corner[1] + 16))


def tile(sheet, n):
    return sheet.crop((n % 34 * 16, n // 34 * 16, n % 34 * 16 + 16, n // 34 * 16 + 16))


def atlas(xbox, playstation, keyboard, kenney):
    letters = {}
    for y, keys in LETTER_ROWS:
        for i, k in enumerate(keys):
            letters[k] = (64 + i * 16, y)
    cells = [cell(xbox, c) for c in XBOX]
    cells += [cell(playstation, c) for c in PLAYSTATION]
    cells += [tile(kenney, n) for n in NINTENDO]
    cells += [cell(xbox, c) for c in STICKS]
    cells += [cell(keyboard, ENTER, 32), cell(keyboard, ESCAPE), cell(keyboard, DELETE)]
    cells += [cell(keyboard, letters[chr(ord('A') + i)]) for i in range(26)]
    out = Image.new('RGBA', (sum(c.width for c in cells), 16))
    x = 0
    for c in cells:
        out.paste(c, (x, 0))
        x += c.width
    return out


def load(path):
    return Image.open(path).convert('RGBA')


gdb = sys.argv[1]
picture = atlas(load(gdb + '/gdb-xbox-2.png'), load(sys.argv[2]), load(gdb + '/gdb-keyboard-2.png'),
                load(sys.argv[3] + '/Tilemap/tilemap_packed.png'))
picture.save(os.path.join(os.path.dirname(os.path.abspath(sys.argv[0])), 'prompts_colour.png'), optimize=True)
