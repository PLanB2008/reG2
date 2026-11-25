# The MAP's and the minimap's pins in the style of the key printed with GTA2's city maps (a wrench for the garages, a
# cross for the hospitals, a handset for the phones, an i for the gangs' information phones, a hook for the cranes, an S
# under a cross for the church): our own drawings, 13x13 HUD pixels at 1x and 26x26 at 2x.
# usage: python3 -I key_pins.py <out dir>
import math
import os
import sys
from PIL import Image, ImageDraw

SS = 16  # supersamples per pixel at 2x
SIZE = 26 * SS
INK = (20, 16, 12, 255)
WHITE = (250, 248, 240, 255)


def canvas():
    return Image.new('RGBA', (SIZE, SIZE), (0, 0, 0, 0))


def disc(d, fill, rim=INK, inset=0.5):
    r = SIZE / 2 - inset * SS
    c = SIZE / 2
    d.ellipse((c - r, c - r, c + r, c + r), fill=rim)
    r -= 2 * SS
    d.ellipse((c - r, c - r, c + r, c + r), fill=fill)


def square(d, fill, rim=INK):
    d.rounded_rectangle((SS, SS, SIZE - SS, SIZE - SS), radius=4 * SS, fill=rim)
    d.rounded_rectangle((3 * SS, 3 * SS, SIZE - 3 * SS, SIZE - 3 * SS), radius=3 * SS, fill=fill)


def line(d, pts, width, fill):
    d.line([(x * SS, y * SS) for x, y in pts], fill=fill, width=int(width * SS), joint='curve')
    for x, y in (pts[0], pts[-1]):
        r = width * SS / 2
        d.ellipse((x * SS - r, y * SS - r, x * SS + r, y * SS + r), fill=fill)


def garage():
    im = canvas(); d = ImageDraw.Draw(im)
    orange = (242, 160, 32, 255)
    disc(d, orange)
    # A double-ended open wrench from the bottom left to the top right.
    line(d, [(8.5, 17.5), (17.5, 8.5)], 2.8, INK)
    for cx, cy in ((7.2, 18.8), (18.8, 7.2)):
        r = 3.6 * SS
        d.ellipse((cx * SS - r, cy * SS - r, cx * SS + r, cy * SS + r), fill=INK)
        # The jaw's opening, outward.
        ox, oy = (cx - 13) * 0.32, (cy - 13) * 0.32
        r = 1.8 * SS
        d.ellipse(((cx + ox) * SS - r, (cy + oy) * SS - r, (cx + ox) * SS + r, (cy + oy) * SS + r), fill=orange)
    return im


def hospital():
    im = canvas(); d = ImageDraw.Draw(im)
    square(d, (214, 34, 30, 255))
    d.rectangle((10.5 * SS, 6 * SS, 15.5 * SS, 20 * SS), fill=WHITE)
    d.rectangle((6 * SS, 10.5 * SS, 20 * SS, 15.5 * SS), fill=WHITE)
    return im


def handset(d, colour):
    # A receiver lying from the bottom left to the top right: a thick handle bowed toward the top left, and at each end a
    # wide cup turned toward the bottom right, the earpiece and the mouthpiece.
    cx, cy, r, w = 17.2, 17.2, 9.0, 3.4
    d.arc(((cx - r - w / 2) * SS, (cy - r - w / 2) * SS, (cx + r + w / 2) * SS, (cy + r + w / 2) * SS), start=180, end=270,
          fill=colour, width=int(w * SS))
    d.rounded_rectangle((14.0 * SS, 5.4 * SS, 22.2 * SS, 12.0 * SS), radius=2.0 * SS, fill=colour)
    d.rounded_rectangle((5.4 * SS, 14.0 * SS, 12.0 * SS, 22.2 * SS), radius=2.0 * SS, fill=colour)


def phone(colour):
    im = canvas(); d = ImageDraw.Draw(im)
    disc(d, WHITE, rim=colour)
    c = SIZE / 2; r = SIZE / 2 - 0.5 * SS
    d.ellipse((c - r, c - r, c + r, c + r), outline=INK, width=int(0.9 * SS))
    handset(d, colour)
    return im


def info():
    im = canvas(); d = ImageDraw.Draw(im)
    disc(d, (26, 150, 64, 255))
    r = 1.8 * SS
    d.ellipse((13 * SS - r, 7.5 * SS - r, 13 * SS + r, 7.5 * SS + r), fill=WHITE)
    d.rounded_rectangle((11.3 * SS, 11 * SS, 14.7 * SS, 20 * SS), radius=SS, fill=WHITE)
    return im


def crane():
    im = canvas(); d = ImageDraw.Draw(im)
    orange = (226, 92, 30, 255)
    disc(d, orange)
    # A lattice arm from the bottom right up to the top left, and the hook hanging from its tip.
    line(d, [(19.5, 20), (10, 6.5)], 4.2, INK)
    for t in (0.2, 0.42, 0.64):
        x, y = 19.5 + (10 - 19.5) * t, 20 + (6.5 - 20) * t
        line(d, [(x - 0.9, y + 0.6), (x + 0.9, y - 0.6)], 0.9, orange)
    line(d, [(9, 7), (7.5, 12)], 1.2, INK)
    d.arc((4.5 * SS, 10.5 * SS, 11 * SS, 17 * SS), start=-30, end=200, fill=INK, width=int(1.8 * SS))
    return im


def church():
    im = canvas(); d = ImageDraw.Draw(im)
    # The black disc a little low, the cross standing above it, the sign with the S hanging from the cross.
    c, r = SIZE / 2, SIZE / 2 - 1.8 * SS
    d.ellipse((c - r, c - r + 1.3 * SS, c + r, c + r + 1.3 * SS), fill=INK)
    gold = (236, 196, 60, 255)
    d.rectangle((12.1 * SS, 0.4 * SS, 13.9 * SS, 8 * SS), fill=gold)
    d.rectangle((9.8 * SS, 2.2 * SS, 16.2 * SS, 3.9 * SS), fill=gold)
    sign = (170, 226, 150, 255); ink = (24, 80, 24, 255)
    d.rectangle((7.5 * SS, 7.5 * SS, 18.5 * SS, 21.5 * SS), fill=sign)
    w = 2.0
    for y in (10.2, 14.5, 18.8):
        d.rectangle((9.8 * SS, (y - w / 2) * SS, 16.2 * SS, (y + w / 2) * SS), fill=ink)
    d.rectangle((9.8 * SS, 10.2 * SS, (9.8 + w) * SS, 14.5 * SS), fill=ink)
    d.rectangle(((16.2 - w) * SS, 14.5 * SS, 16.2 * SS, 18.8 * SS), fill=ink)
    return im


PINS = {
    'garage': garage,
    'restart': hospital,
    'crane': crane,
    'save': church,
    'info': info,
    'phone_green': lambda: phone((40, 160, 40, 255)),
    'phone_yellow': lambda: phone((226, 176, 0, 255)),
    'phone_red': lambda: phone((210, 30, 24, 255)),
}


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    for name, make in PINS.items():
        big = make()
        for scale, px in ((1, 13), (2, 26)):
            im = big.resize((px, px), Image.LANCZOS)
            # Hard edges, as the HUD's sprites have.
            a = im.getchannel('A').point(lambda v: 255 if v > 110 else 0)
            im.putalpha(a)
            im.save(os.path.join(out, '%s_%dx.png' % (name, scale)))


if __name__ == '__main__':
    main()
