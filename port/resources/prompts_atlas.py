#!/usr/bin/env python3
# Builds prompts_colour.png (16 pixel cells in prompts.cpp's order) from greatdocbrown's Gamepad UI (CC0):
# prompts_atlas.py <Gamepad UI folder> <gdb-playstation-3.png>
import os
import sys
from PIL import Image

# Cells' top left corners in gdb-xbox-2.png: A B X Y, LB RB, View Menu, LT RT; then the sticks' L and R.
XBOX = [(16, 48), (16, 80), (16, 32), (16, 64), (336, 80), (336, 96), (16, 96), (16, 112), (336, 48), (336, 64)]
STICKS = [(216, 32), (280, 32)]
# In gdb-playstation-3.png: cross circle square triangle, L1 R1, Share Options, L2 R2.
PLAYSTATION = [(16, 192), (16, 176), (16, 208), (16, 160), (128, 112), (128, 128), (16, 240), (16, 256), (128, 80),
               (128, 96)]
# The dark keys (the second set, not pressed) in gdb-keyboard-2.png; Enter and Backspace are two cells wide.
ENTER = (240, 176)
ESCAPE = (32, 128)
DELETE = (432, 256)
LEFT = (304, 208)
RIGHT = (336, 208)
BACKSPACE = (256, 144)
LETTER_ROWS = [(160, 'QWERTYUIOP'), (176, 'ASDFGHJKL'), (192, 'ZXCVBNM')]


def cell(sheet, corner, width=16):
    return sheet.crop((corner[0], corner[1], corner[0] + width, corner[1] + 16))


def atlas(xbox, playstation, keyboard):
    letters = {}
    for y, keys in LETTER_ROWS:
        for i, k in enumerate(keys):
            letters[k] = (64 + i * 16, y)
    cells = [cell(xbox, c) for c in XBOX]
    cells += [cell(playstation, c) for c in PLAYSTATION]
    cells += [cell(xbox, c) for c in STICKS]
    cells += [cell(keyboard, ENTER, 32), cell(keyboard, ESCAPE), cell(keyboard, DELETE)]
    cells += [cell(keyboard, LEFT), cell(keyboard, RIGHT), cell(keyboard, BACKSPACE, 32)]
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
picture = atlas(load(gdb + '/gdb-xbox-2.png'), load(sys.argv[2]), load(gdb + '/gdb-keyboard-2.png'))
picture.save(os.path.join(os.path.dirname(os.path.abspath(sys.argv[0])), 'prompts_colour.png'), optimize=True)
