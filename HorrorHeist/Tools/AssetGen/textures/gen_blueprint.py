"""
T_Blueprint_Vance: the crew's floor plan of 14 Ashgrove Lane for the hideout's planning table.
Drawn from the same room layout as Tools/AssetGen/blender/vance.py (house-local metres, front
door at the bottom), with the crew's handwritten notes.

  python3 Tools/AssetGen/textures/gen_blueprint.py
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
FONTS = os.path.join(PROJECT, 'Content', 'Slate', 'Fonts')
OUT = os.path.join(PROJECT, 'SourceArt', 'Textures', 'Unique', 'T_Blueprint_Vance.jpg')

W, H = 2048, 1380
PAPER = (22, 52, 104)
LINE = (214, 228, 245)


def font(name, size):
    return ImageFont.truetype(os.path.join(FONTS, name), size)


TYPE = lambda s: font('IBMPlexMono-Medium.ttf', s)        # noqa: E731
HAND = lambda s: font('Caveat-Medium.ttf', s)             # noqa: E731
TITLE = lambda s: font('Oswald-SemiBold.ttf', s)          # noqa: E731

# House plan (vance.py): x -5.5..5.5, y 0..9; partitions at x = -1.1 / 1.1, rooms per floor.
ROOMS_G = [((-5.25, 0.25, -1.16, 5.54), 'LIVING RM'), ((-5.25, 5.66, -1.16, 8.75), 'DINING'),
           ((1.16, 0.25, 5.25, 3.94), 'STUDY'), ((1.16, 4.06, 5.25, 8.75), 'KITCHEN'),
           ((-1.04, 0.25, 1.04, 8.75), 'HALL')]
ROOMS_U = [((-5.25, 0.25, -1.16, 5.54), 'MASTER BR'), ((-5.25, 5.66, -1.16, 8.75), 'GUEST'),
           ((1.16, 0.25, 5.25, 4.4), "KID'S RM"), ((1.16, 4.52, 5.25, 8.75), 'BATH'),
           ((-1.04, 0.25, 1.04, 8.75), 'HALL')]


def plan(d, ox, oy, scale, rooms, label, doors, windows, stair=True, basement_stair=False):
    def P(x, y):
        return ox + (x + 5.5) * scale, oy - y * scale

    # Exterior walls (double line).
    for inset, wdt in ((0.0, 5), (0.25, 3)):
        x0, y0 = P(-5.5 + inset, 9.0 - inset)
        x1, y1 = P(5.5 - inset, 0.0 + inset)
        d.rectangle((x0, y0, x1, y1), outline=LINE, width=wdt)
    for (rx0, ry0, rx1, ry1), name in rooms:
        a, b = P(rx0, ry1)
        c, e = P(rx1, ry0)
        d.rectangle((a, b, c, e), outline=LINE, width=2)
        cx, cy = (a + c) / 2, (b + e) / 2
        if name != 'HALL':
            d.text((cx, cy), name, font=TYPE(26), fill=LINE, anchor='mm')
            d.text((cx, cy + 30), f'{rx1 - rx0:.1f} x {ry1 - ry0:.1f} m', font=TYPE(18), fill=LINE, anchor='mm')
    if stair:
        x0, y0 = P(0.16, 5.9)
        x1, y1 = P(1.04, 1.9)
        d.rectangle((x0, y0, x1, y1), outline=LINE, width=2)
        n = 16
        for i in range(1, n):
            yy = y1 + (y0 - y1) * i / n
            d.line((x0, yy, x1, yy), fill=LINE, width=1)
        d.line(((x0 + x1) / 2, y1 - 8, (x0 + x1) / 2, y0 + 14), fill=LINE, width=2)
        d.polygon([((x0 + x1) / 2 - 8, y0 + 22), ((x0 + x1) / 2 + 8, y0 + 22), ((x0 + x1) / 2, y0 + 8)], fill=LINE)
        d.text((x1 + 8, (y0 + y1) / 2), 'UP' if not basement_stair else 'DN', font=TYPE(18), fill=LINE, anchor='lm')
    for (x, y, horizontal) in doors:
        px, py = P(x, y)
        r = 0.86 * scale
        if horizontal:
            d.arc((px - r, py - r, px + r, py + r), 270, 360, fill=LINE, width=2)
            d.line((px, py, px, py - r), fill=LINE, width=2)
        else:
            d.arc((px - r, py - r, px + r, py + r), 0, 90, fill=LINE, width=2)
            d.line((px, py, px + r, py), fill=LINE, width=2)
    for (x0, y0, x1, y1) in windows:
        a, b = P(x0, y0)
        c, e = P(x1, y1)
        d.rectangle((min(a, c), min(b, e), max(a, c), max(b, e)), fill=PAPER, outline=LINE, width=2)
    lx, ly = P(-5.5, -0.4)
    d.text((lx, ly + 18), label, font=TITLE(34), fill=LINE, anchor='la')


def main():
    rng = np.random.default_rng(14)
    img = Image.new('RGB', (W, H), PAPER)
    # Paper texture: fibres and a faint grid.
    noise = rng.normal(0, 6, (H, W, 1)).repeat(3, axis=2)
    arr = np.clip(np.asarray(img, dtype=np.float32) + noise, 0, 255).astype(np.uint8)
    img = Image.fromarray(arr)
    d = ImageDraw.Draw(img)
    for x in range(0, W, 32):
        d.line((x, 0, x, H), fill=(32, 64, 118), width=1)
    for y in range(0, H, 32):
        d.line((0, y, W, y), fill=(32, 64, 118), width=1)

    scale = 76
    doors_g = [(-0.95, 0.0, False), (3.5, 9.0, True), (1.1, 0.8, True), (1.1, 6.6, True), (0.2, 5.9, False)]
    win_g = [(-4.95, -0.08, -4.05, 0.33), (-2.55, -0.08, -1.65, 0.33), (2.6, -0.08, 3.8, 0.33),
             (5.17, 1.55, 5.58, 2.45), (-5.58, 0.8, -5.17, 1.6), (-5.58, 4.2, -5.17, 5.0), (-5.58, 6.15, -5.17, 7.05),
             (-3.65, 8.67, -2.75, 9.08), (1.55, 8.67, 2.45, 9.08), (5.17, 5.9, 5.58, 6.9)]
    plan(d, 110, 1020, scale, ROOMS_G, 'GROUND FLOOR', doors_g, win_g)
    doors_u = [(-1.1, 4.6, True), (-1.1, 6.8, True), (1.1, 1.0, True), (1.1, 6.4, True)]
    win_u = [(-4.95, -0.08, -4.05, 0.33), (-2.55, -0.08, -1.65, 0.33), (1.75, -0.08, 2.65, 0.33),
             (3.75, -0.08, 4.65, 0.33), (-5.58, 1.2, -5.17, 2.0), (-5.58, 4.0, -5.17, 4.8), (-5.58, 6.75, -5.17, 7.65),
             (5.17, 0.6, 5.58, 1.4), (5.17, 6.95, 5.58, 7.65), (-3.65, 8.67, -2.75, 9.08)]
    plan(d, 1080, 1020, scale, ROOMS_U, 'UPPER FLOOR', doors_u, win_u)

    # Title block.
    d.rectangle((1520, 1150, 2010, 1350), outline=LINE, width=3)
    d.text((1540, 1165), '14 ASHGROVE LANE', font=TITLE(40), fill=LINE)
    d.text((1540, 1222), 'RESIDENCE - VANCE', font=TYPE(24), fill=LINE)
    d.text((1540, 1256), 'BASEMENT: FULL, STAIR UNDER MAIN STAIR', font=TYPE(18), fill=LINE)
    d.text((1540, 1286), 'SCALE 1:100   SHEET 1 OF 1', font=TYPE(18), fill=LINE)
    d.text((1540, 1316), 'HOLLOWMERE BLDG. DEPT. 1962', font=TYPE(18), fill=LINE)

    # The crew's notes in red and white grease pencil.
    red = (226, 92, 76)
    notes = [((110, 70), 'back door sticks - crowbar = LOUD', red, 40),
             ((110, 120), 'spare key?  check the gnome by the porch', LINE, 38),
             ((1080, 70), 'COIN COLLECTION - master bedroom dresser', red, 42),
             ((1080, 125), "nobody's seen the Vances leave. ever.", LINE, 36),
             ((110, 1170), 'cash in the study desk', LINE, 38),
             ((110, 1220), 'van at the curb. everyone back in before we go.', LINE, 38),
             ((640, 1290), 'basement - stay out of it', red, 36)]
    for (x, y), text, col, size in notes:
        d.text((x, y), text, font=HAND(size), fill=col)
    # Circle the master bedroom and the study.
    for cx, cy, r in ((1080 + (-3.2 + 5.5) * scale, 1020 - 2.9 * scale, 190), (110 + (3.2 + 5.5) * scale, 1020 - 2.0 * scale, 150)):
        for k in range(3):
            d.ellipse((cx - r - k, cy - r * 0.7 - k, cx + r + k, cy + r * 0.7 + k), outline=red, width=2)
    # An X on the back door.
    bx, by = 110 + (3.9 + 5.5) * scale, 1020 - 9.0 * scale
    d.line((bx - 26, by - 26, bx + 26, by + 26), fill=red, width=6)
    d.line((bx - 26, by + 26, bx + 26, by - 26), fill=red, width=6)

    img = img.filter(ImageFilter.GaussianBlur(0.6))
    # Fold lines and wear.
    arr = np.asarray(img, dtype=np.float32)
    for x in (W // 3, 2 * W // 3):
        arr[:, x - 2:x + 2] *= 0.82
    arr[H // 2 - 2:H // 2 + 2, :] *= 0.85
    yy, xx = np.mgrid[0:H, 0:W]
    vign = 1.0 - 0.25 * (((xx - W / 2) / (W / 2)) ** 2 + ((yy - H / 2) / (H / 2)) ** 2)
    arr *= vign[..., None]
    Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8)).save(OUT, quality=90)
    print('wrote', OUT)


if __name__ == '__main__':
    main()
