"""
Decals for the haunting: handprints that appear on mirrors and doors, words scratched into the
plaster, claw marks, a chalk circle in the basement and a child's drawing on the fridge.
Run from the HorrorHeist folder:  python Tools/AssetGen/textures/gen_haunt_images.py
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

sys.path.insert(0, os.path.dirname(__file__))
import texlib as T  # noqa: E402

DECALS = 'SourceArt/Textures/Decals'
FONTS = 'Content/Slate/Fonts'


def font(name, size):
    return ImageFont.truetype(os.path.join(FONTS, name), size)


def rgba_from(alpha, rgb):
    h, w = alpha.shape
    color = np.broadcast_to(np.array(rgb, dtype=float)[None, None, :], (h, w, 3))
    return np.concatenate([color, np.clip(alpha, 0, 1)[..., None]], -1)


def grit(h, w, period=24, lo=0.55):
    n = T.fbm(max(h, w), period, 4)[:h, :w]
    return lo + (1 - lo) * n


def handprints():
    """Two smeared handprints, sliding down a little (dark, dried)."""
    T.seed(801)
    size = 1024
    img = Image.new('L', (size, size), 0)
    d = ImageDraw.Draw(img)
    rng = np.random.default_rng(801)

    def hand(cx, cy, angle, scale, mirror):
        pts = []
        # Palm, fingers (index..little) and thumb as rounded strokes in hand space (pixels at scale 1).
        palm = [(-55, -10), (55, -10), (60, 90), (-50, 95)]
        fingers = [((-42, -15), (-50, -150)), ((-12, -20), (-14, -175)), ((18, -18), (22, -165)), ((46, -10), (60, -125))]
        thumb = ((-55, 40), (-120, -20))
        if mirror:
            palm = [(-x, y) for x, y in palm]
            fingers = [((-a[0], a[1]), (-b[0], b[1])) for a, b in fingers]
            thumb = ((-thumb[0][0], thumb[0][1]), (-thumb[1][0], thumb[1][1]))
        ca, sa = math.cos(angle), math.sin(angle)

        def tr(p):
            x, y = p[0] * scale, p[1] * scale
            return (cx + x * ca - y * sa, cy + x * sa + y * ca)
        d.polygon([tr(p) for p in palm], fill=215)
        d.ellipse([tr((-58, 40))[0] - 50 * scale, tr((-58, 40))[1] - 50 * scale,
                   tr((-58, 40))[0] + 50 * scale, tr((-58, 40))[1] + 50 * scale], fill=215)
        for a, b in fingers + [thumb]:
            d.line([tr(a), tr(b)], fill=220, width=int(34 * scale))
            e = tr(b)
            d.ellipse([e[0] - 17 * scale, e[1] - 17 * scale, e[0] + 17 * scale, e[1] + 17 * scale], fill=220)
        # Smear: the hand slid down.
        for k in range(12):
            off = (k + 1) * 9 * scale
            sx = rng.uniform(-50, 50) * scale
            d.line([tr((sx, 80)), (tr((sx, 80))[0], tr((sx, 80))[1] + off * 3)], fill=int(120 - k * 8), width=int(10 * scale))
        pts.append((cx, cy))
        return pts

    hand(380, 420, -0.12, 1.35, False)
    hand(660, 470, 0.18, 1.25, True)
    a = np.array(img.filter(ImageFilter.GaussianBlur(3))).astype(float) / 255.0
    # Skin texture: the print is broken up by ridges and grit.
    ridges = (np.sin(T.warp(np.mgrid[0:size, 0:size][0] / size, 0.03, 8) * 900) * 0.5 + 0.5) ** 0.5
    a = a * grit(size, size, 40, 0.35) * (0.88 + 0.12 * ridges)
    rgb = (0.16, 0.06, 0.05)
    T.save_rgba(os.path.join(DECALS, 'T_Decal_Handprints.png'), rgba_from(np.clip(a * 1.15, 0, 0.95), rgb))


def scrawl(text, name, size=(1024, 512), font_px=300, seed=802, color=(0.22, 0.05, 0.04)):
    """Words written with a finger in something dark - running at the bottom."""
    T.seed(seed)
    w, h = size
    img = Image.new('L', (w, h), 0)
    d = ImageDraw.Draw(img)
    fnt = font('ReenieBeanie-Regular.ttf', font_px)
    box = d.textbbox((0, 0), text, font=fnt)
    tw, th = box[2] - box[0], box[3] - box[1]
    scale = min(1.0, (w * 0.9) / tw)
    if scale < 1.0:
        fnt = font('ReenieBeanie-Regular.ttf', int(font_px * scale))
        box = d.textbbox((0, 0), text, font=fnt)
        tw, th = box[2] - box[0], box[3] - box[1]
    x0, y0 = (w - tw) / 2 - box[0], (h - th) / 2 - box[1] - h * 0.06
    # Stroke twice, slightly offset: a finger, not a pen.
    for dx, dy, val in ((0, 0, 230), (3, 2, 170), (-2, 3, 140)):
        d.text((x0 + dx, y0 + dy), text, font=fnt, fill=val, stroke_width=4, stroke_fill=val)
    a = np.array(img).astype(float) / 255.0
    # Drips from the bottom of the strokes.
    rng = np.random.default_rng(seed)
    cols = np.where(a.max(axis=0) > 0.5)[0]
    for x in rng.choice(cols, size=min(len(cols), 26), replace=False):
        ys = np.where(a[:, x] > 0.5)[0]
        if len(ys) == 0:
            continue
        y = ys.max()
        length = int(rng.uniform(15, 120))
        width = int(rng.uniform(2, 5))
        for yy in range(y, min(h - 1, y + length)):
            fade = 1 - (yy - y) / length
            a[yy, max(0, x - width // 2):x + width // 2 + 1] = np.maximum(a[yy, max(0, x - width // 2):x + width // 2 + 1], 0.8 * fade)
        a[min(h - 1, y + length - 3):min(h, y + length + 3), max(0, x - width):x + width + 1] = 0.7
    a = T.gblur(a, 1.2) * grit(h, w, 30, 0.45)
    T.save_rgba(os.path.join(DECALS, f'T_Decal_{name}.png'), rgba_from(np.clip(a * 1.2, 0, 0.95), color))


def scratches():
    """Four parallel gouges, dragged downward."""
    T.seed(803)
    size = 512
    img = Image.new('L', (size, size), 0)
    d = ImageDraw.Draw(img)
    rng = np.random.default_rng(803)
    for k in range(4):
        x0 = 150 + k * 55 + rng.uniform(-6, 6)
        pts = [(x0 + rng.uniform(-4, 4) + i * 1.5, 60 + i * 38) for i in range(11)]
        for i in range(len(pts) - 1):
            wdt = int(10 - abs(i - 4) * 1.3) if i < 9 else 3
            d.line([pts[i], pts[i + 1]], fill=255, width=max(2, wdt))
    a = np.array(img.filter(ImageFilter.GaussianBlur(1.0))).astype(float) / 255.0
    a *= grit(size, size, 20, 0.6)
    rgb = np.zeros((size, size, 3)) + np.array([0.11, 0.09, 0.07])
    T.save_rgba(os.path.join(DECALS, 'T_Decal_Scratches.png'), np.concatenate([rgb, a[..., None] * 0.9], -1))


def chalk_circle():
    """A chalk circle with a five-pointed star and marks, scuffed (on the basement floor)."""
    T.seed(804)
    size = 1024
    img = Image.new('L', (size, size), 0)
    d = ImageDraw.Draw(img)
    c = size / 2
    for r, wdt in ((430, 10), (395, 6)):
        d.ellipse([c - r, c - r, c + r, c + r], outline=230, width=wdt)
    pts = [(c + 395 * math.cos(-math.pi / 2 + k * 4 * math.pi / 5), c + 395 * math.sin(-math.pi / 2 + k * 4 * math.pi / 5)) for k in range(6)]
    d.line(pts, fill=220, width=7)
    rng = np.random.default_rng(804)
    for k in range(13):  # marks between the rings
        ang = k * 2 * math.pi / 13
        x, y = c + 412 * math.cos(ang), c + 412 * math.sin(ang)
        for _ in range(3):
            dx, dy = rng.uniform(-12, 12, 2)
            d.line([(x - dx, y - dy), (x + dx, y + dy)], fill=200, width=4)
    a = np.array(img.filter(ImageFilter.GaussianBlur(1.6))).astype(float) / 255.0
    scuff = T.smoothstep(0.35, 0.6, T.fbm(size, 6, 5))
    a *= grit(size, size, 64, 0.4) * (0.35 + 0.65 * scuff)
    rgb = np.zeros((size, size, 3)) + np.array([0.82, 0.81, 0.78])
    T.save_rgba(os.path.join(DECALS, 'T_Decal_ChalkCircle.png'), np.concatenate([rgb, a[..., None] * 0.85], -1))


def child_drawing():
    """Crayon on paper: a house, mum, dad and a little one - and the tall black shape behind them."""
    T.seed(805)
    w, h = 768, 1024
    paper = np.ones((h, w, 3)) * np.array([0.93, 0.91, 0.86])
    paper *= (0.94 + 0.06 * T.fbm(1024, 64, 3)[:h, :w])[..., None]
    img = Image.fromarray((paper * 255).astype(np.uint8), 'RGB')
    d = ImageDraw.Draw(img)
    rng = np.random.default_rng(805)

    def crayon(points, color, width=9, passes=3):
        for _ in range(passes):
            jit = [(x + rng.uniform(-3, 3), y + rng.uniform(-3, 3)) for x, y in points]
            d.line(jit, fill=color, width=width, joint='curve')

    # Sun and grass
    d.ellipse([560, 70, 690, 200], outline=(235, 190, 40), width=10)
    for k in range(10):
        ang = k * math.pi / 5
        crayon([(625 + 80 * math.cos(ang), 135 + 80 * math.sin(ang)), (625 + 115 * math.cos(ang), 135 + 115 * math.sin(ang))], (235, 190, 40), 7, 2)
    crayon([(20, 900), (750, 890)], (70, 150, 60), 14, 4)
    # House
    crayon([(80, 880), (80, 600), (330, 600), (330, 880), (80, 880)], (180, 70, 50), 9)
    crayon([(60, 610), (205, 470), (350, 610)], (120, 60, 40), 9)
    crayon([(170, 880), (170, 760), (240, 760), (240, 880)], (90, 60, 40), 8)
    # Family: three figures (stick people with round heads)
    for x, s, col in ((420, 1.0, (60, 90, 200)), (520, 1.05, (200, 60, 120)), (610, 0.7, (240, 140, 40))):
        top = 880 - 260 * s
        d.ellipse([x - 28 * s, top, x + 28 * s, top + 56 * s], outline=col, width=7)
        crayon([(x, top + 56 * s), (x, 880 - 95 * s)], col, 8)
        crayon([(x - 45 * s, top + 100 * s), (x + 45 * s, top + 100 * s)], col, 7)
        crayon([(x, 880 - 95 * s), (x - 30 * s, 880)], col, 7)
        crayon([(x, 880 - 95 * s), (x + 30 * s, 880)], col, 7)
        d.arc([x - 15 * s, top + 22 * s, x + 15 * s, top + 44 * s], 20, 160, fill=col, width=4)
    # The tall one: scribbled solid black, long arms over the family, no face
    for _ in range(140):
        y = rng.uniform(250, 880)
        span = 26 if y > 330 else 22
        crayon([(705 - span + rng.uniform(-4, 4), y), (705 + span + rng.uniform(-4, 4), y + rng.uniform(-6, 6))], (18, 16, 16), 6, 1)
    d.ellipse([672, 200, 738, 280], fill=(18, 16, 16))
    crayon([(690, 340), (600, 330), (520, 360), (440, 400)], (18, 16, 16), 9, 4)
    crayon([(720, 340), (740, 420), (735, 520)], (18, 16, 16), 9, 4)
    # Name and a word in a child's hand
    hand = font('ReenieBeanie-Regular.ttf', 70)
    d.text((40, 80), 'ME AND MUM AND DAD', font=hand, fill=(60, 60, 160))
    d.text((40, 160), 'and HER', font=hand, fill=(20, 18, 18))
    a = np.ones((h, w))
    # Slightly torn bottom corner, fridge-magnet crease
    yy, xx = np.mgrid[0:h, 0:w]
    a[(xx + (h - yy)) < 60] = 0
    rgba = np.concatenate([np.array(img).astype(float) / 255.0, a[..., None]], -1)
    T.save_rgba(os.path.join(DECALS, 'T_Decal_ChildDrawing.png'), rgba)


JOBS = [handprints, lambda: scrawl('GET OUT', 'GetOut', seed=802),
        lambda: scrawl('SHE IS STILL HERE', 'StillHere', seed=806, font_px=240), scratches, chalk_circle, child_drawing]

if __name__ == '__main__':
    os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..'))
    for job in JOBS:
        job()
    print('done')
