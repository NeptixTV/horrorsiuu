"""
Images for the bigger Hollowmere (downtown, the bank, the supermarket, the playground and the
crime scenes): shop signs, the ATM screen, grocery shelf facings, aisle signs, posters and police
tape. All procedural, rendered with the project fonts like gen_world_images.py.

Run from the HorrorHeist folder:  python Tools/AssetGen/textures/gen_town_images.py [names...]
Writes SourceArt/Textures/Unique/*.jpg|png
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(__file__))
import gen_world_images as W  # noqa: E402
import texlib as T  # noqa: E402

OUT = W.OUT


def save_png(img, name):
    os.makedirs(OUT, exist_ok=True)
    img.save(os.path.join(OUT, name + '.png'), optimize=True)


# =======================================================================================
# Shop signs (2048 x 512, the same board as the Main Street shops)

def sign_bank():
    W._shop_sign('T_Sign_Bank', (0.07, 0.16, 0.12), (0.86, 0.72, 0.40), 'HOLLOWMERE SAVINGS & LOAN',
                 'EST. 1902  ·  YOUR FUTURE, SAFE', W.OSWALD, W.COND, 701, accent=(0.78, 0.64, 0.34), rust=0.2,
                 dirt=0.35, fade=0.15, title_tracking=6)


def sign_market():
    W._shop_sign('T_Sign_Market', (0.72, 0.08, 0.06), (0.97, 0.95, 0.90), 'PICK & SAVE',
                 'GROCERIES  ·  FILM  ·  FIRST AID', W.OSWALD, W.COND, 702, accent=(0.98, 0.86, 0.30), rust=0.15,
                 dirt=0.3, fade=0.2, title_tracking=18, title_h=0.56)


def sign_gazette():
    W._shop_sign('T_Sign_Gazette', (0.85, 0.82, 0.74), (0.08, 0.08, 0.09), 'THE HOLLOWMERE GAZETTE',
                 'NEWS SINCE 1889', W.TYPE, W.COND, 703, accent=(0.30, 0.10, 0.08), rust=0.3, dirt=0.5, fade=0.2,
                 title_tracking=4)


def sign_pharmacy():
    W._shop_sign('T_Sign_Pharmacy', (0.10, 0.30, 0.34), (0.92, 0.94, 0.92), "BELL'S PHARMACY",
                 'PRESCRIPTIONS  ·  SODA FOUNTAIN', W.OSWALD, W.COND, 704, accent=(0.70, 0.86, 0.84), rust=0.35,
                 dirt=0.45, title_tracking=8)


def sign_barber():
    W._shop_sign('T_Sign_Barber', (0.12, 0.14, 0.30), (0.95, 0.93, 0.88), "SAL'S BARBER SHOP",
                 'HAIRCUTS  ·  HOT SHAVES', W.OSWALD, W.COND, 705, accent=(0.78, 0.16, 0.12), rust=0.4, dirt=0.5,
                 title_tracking=6)


def sign_playground():
    W._shop_sign('T_Sign_Playground', (0.16, 0.30, 0.16), (0.95, 0.92, 0.80), 'WILLOW PARK',
                 'PLAYGROUND  ·  OPEN DAWN TO DUSK', W.OSWALD, W.COND, 706, accent=(0.95, 0.80, 0.30), rust=0.5,
                 dirt=0.55, fade=0.3, title_tracking=14)


# =======================================================================================
# The ATM screen (shown through the Screen master, faint scanlines)

def atm_screen():
    w, h = 512, 384
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float64)
    grad = 0.55 + 0.45 * (1 - yy / h)
    base = np.stack([0.05 * grad, 0.18 * grad, 0.42 * grad], -1)
    img = W.to_img(base)
    d = ImageDraw.Draw(img)
    gold = (230, 196, 110)
    white = (232, 238, 246)
    d.rectangle([0, 0, w, 52], fill=(10, 34, 26))
    W.text_centered(d, w / 2, 27, 'HOLLOWMERE SAVINGS & LOAN', W.fit_font(W.COND, 'HOLLOWMERE SAVINGS & LOAN', w - 60, 30, 40, 2),
                    gold, 2)
    W.text_centered(d, w / 2, 120, 'WELCOME', W.fit_font(W.OSWALD, 'WELCOME', w - 290, 64, 80, 6), white, 6)
    for k, msg in enumerate(('PLEASE INSERT', 'YOUR CARD')):
        W.text_centered(d, w / 2, 190 + k * 38, msg, W.fit_font(W.COND, 'PLEASE INSERT', w - 300, 32, 40, 3), white, 3)
    # Card slot arrow and the side key hints (keys beside the screen line up with these).
    d.polygon([(w / 2 - 22, 272), (w / 2 + 22, 272), (w / 2, 302)], fill=gold)
    for i, label in enumerate(('BALANCE', 'DEPOSIT', 'WITHDRAW', 'CANCEL')):
        y = 110 + (i % 2) * 150
        side = 0 if i < 2 else 1
        x0 = 8 if side == 0 else w - 120
        d.rounded_rectangle([x0, y - 16, x0 + 112, y + 16], radius=6, outline=(120, 160, 210), width=2)
        W.text_centered(d, x0 + 56, y, label, W.fit_font(W.COND, label, 96, 22, 24, 1), (180, 205, 235), 1)
    a = W.to_f(img)
    a *= (0.92 + 0.08 * (np.sin(yy * math.pi / 2) ** 2))[..., None]
    W.save_jpg(np.clip(a, 0, 1), 'T_ATM_Screen')


# =======================================================================================
# Grocery shelves: 8 horizontal bands (256 px each) of product facings, tileable along U.

BRANDS = {
    'cereal': ['OATY-O\'S', 'CORN STARS', 'BRAN FLAKES', 'HONEY PUFFS', 'CRUNCH', 'GRANOLA'],
    'can': ['SUNNY BEANS', 'PEAS', 'CORN', 'TOMATO', 'PEACHES', 'TUNA'],
    'box': ['MACARONI', 'RICE', 'CRACKERS', 'COOKIES', 'CAKE MIX', 'TEA'],
    'bottle': ['HOLLOW COLA', 'LEMON-LIME', 'ROOT BEER', 'GINGER', 'SPRING', 'ORANGE'],
    'jar': ['JAM', 'PICKLES', 'HONEY', 'PEANUT', 'SALSA', 'SAUCE'],
    'bag': ['CHIPS', 'PRETZELS', 'POPCORN', 'NUTS', 'CANDY', 'CORN CHIPS'],
    'jug': ['BLEACH', 'SOAP', 'DETERGENT', 'SHINE', 'FRESH', 'SCRUB'],
    'carton': ['MILK', 'JUICE', 'CREAM', 'MILK 2%', 'O.J.', 'BUTTERMILK'],
}
BANDS = ['cereal', 'can', 'box', 'bottle', 'jar', 'bag', 'jug', 'carton']
PALETTE = [(196, 42, 36), (232, 178, 40), (40, 96, 170), (52, 140, 72), (230, 120, 40), (120, 60, 140),
           (240, 236, 226), (30, 30, 34), (200, 90, 120), (90, 170, 200)]


def _shade_rect(arr, x0, y0, x1, y1, color, kind, rnd):
    """Paint one product into the float RGB array with simple lighting for its shape."""
    h, w = arr.shape[:2]
    x0, x1 = max(0, x0), min(w, x1)
    if x1 <= x0:
        return
    xs = np.linspace(-1, 1, x1 - x0)
    if kind in ('can', 'bottle', 'jar', 'jug'):
        light = 0.62 + 0.38 * np.sqrt(np.clip(1 - xs ** 2, 0, 1)) + 0.25 * np.exp(-((xs + 0.45) / 0.12) ** 2)
    elif kind == 'bag':
        light = 0.75 + 0.25 * np.cos(xs * 1.4) + 0.1 * np.sin(xs * 9 + rnd.uniform(0, 6))
    else:
        light = 0.92 - 0.12 * np.abs(xs) ** 3
    col = np.array(color, dtype=np.float64) / 255.0
    ys = np.linspace(0, 1, max(1, y1 - y0))
    vert = 1.0 - 0.18 * ys
    block = col[None, None, :] * light[None, :, None] * vert[:, None, None]
    arr[y0:y1, x0:x1] = np.clip(block, 0, 1)


def groceries():
    w, band = 2048, 256
    h = band * len(BANDS)
    rnd = np.random.default_rng(1902)
    arr = np.zeros((h, w, 3))
    arr[:] = np.array([0.06, 0.06, 0.065])
    labels = []                               # (kind, x0, y0, x1, y1, brand, color, accent)
    for b, kind in enumerate(BANDS):
        y_floor = (b + 1) * band - 10
        x = 0
        while x < w:
            if kind == 'cereal':
                pw, ph = rnd.integers(110, 150), rnd.integers(190, 232)
            elif kind == 'can':
                pw, ph = rnd.integers(62, 76), rnd.integers(100, 122)
            elif kind == 'box':
                pw, ph = rnd.integers(90, 130), rnd.integers(120, 170)
            elif kind == 'bottle':
                pw, ph = rnd.integers(56, 70), rnd.integers(190, 230)
            elif kind == 'jar':
                pw, ph = rnd.integers(70, 90), rnd.integers(110, 140)
            elif kind == 'bag':
                pw, ph = rnd.integers(120, 160), rnd.integers(170, 215)
            elif kind == 'jug':
                pw, ph = rnd.integers(90, 120), rnd.integers(180, 225)
            else:
                pw, ph = rnd.integers(80, 100), rnd.integers(170, 210)
            reps = int(rnd.integers(2, 5))
            brand = BRANDS[kind][int(rnd.integers(0, 6))]
            color = PALETTE[int(rnd.integers(0, len(PALETTE)))]
            accent = PALETTE[int(rnd.integers(0, len(PALETTE)))]
            for _ in range(reps):
                gap = int(rnd.integers(3, 9))
                if x + pw > w:
                    pw = w - x - gap
                    if pw < 30:
                        x = w
                        break
                x0, x1 = int(x + gap), int(x + gap + pw)
                y0 = int(y_floor - ph)
                _shade_rect(arr, x0, y0, x1, int(y_floor), color, kind, rnd)
                labels.append((kind, x0, y0, x1, int(y_floor), brand, color, accent))
                x = x1
            if rnd.random() < 0.08:
                x += int(rnd.integers(40, 120))          # a gap where something sold out
    img = W.to_img(arr)
    d = ImageDraw.Draw(img)
    for kind, x0, y0, x1, y1, brand, color, accent in labels:
        pw, ph = x1 - x0, y1 - y0
        ac = tuple(int(c) for c in accent)
        # Lettering contrasts with whatever it is printed on (the label panel or the pack itself).
        under = accent if kind in ('cereal', 'box', 'bottle') else color
        dark = sum(under) < 330
        ink = (240, 238, 230) if dark else (30, 26, 24)
        if kind == 'bottle':
            # Neck and cap: shoulders narrow towards the top.
            neck = int(pw * 0.36)
            d.rectangle([x0, y0, x1, y0 + int(ph * 0.3)], fill=(16, 16, 18))
            d.polygon([(x0, y0 + int(ph * 0.34)), (x0 + (pw - neck) // 2, y0 + int(ph * 0.14)),
                       (x1 - (pw - neck) // 2, y0 + int(ph * 0.14)), (x1, y0 + int(ph * 0.34))],
                      fill=tuple(int(c * 0.8) for c in color))
            d.rectangle([x0 + (pw - neck) // 2, y0, x1 - (pw - neck) // 2, y0 + int(ph * 0.15)],
                        fill=tuple(int(c * 0.8) for c in color))
            d.rectangle([x0 + (pw - neck) // 2 - 2, y0, x1 - (pw - neck) // 2 + 2, y0 + 10], fill=ac)
            d.rectangle([x0, y0 + int(ph * 0.45), x1, y0 + int(ph * 0.72)], fill=ac)
        elif kind == 'jug':
            d.rectangle([x1 - int(pw * 0.35), y0, x1, y0 + int(ph * 0.25)], fill=(14, 14, 16))
            d.rectangle([x0 + int(pw * 0.5), y0, x0 + int(pw * 0.75), y0 + 14], fill=ac)
            d.rectangle([x0 + 6, y0 + int(ph * 0.35), x1 - 6, y0 + int(ph * 0.8)], fill=(236, 234, 228))
        elif kind == 'jar':
            d.rectangle([x0 + 4, y0, x1 - 4, y0 + 16], fill=(200, 196, 186))
            d.rectangle([x0, y0 + int(ph * 0.35), x1, y0 + int(ph * 0.78)], fill=(236, 230, 214))
        elif kind == 'can':
            d.rectangle([x0, y0, x1, y0 + 6], fill=(170, 170, 172))
            d.rectangle([x0, y1 - 6, x1, y1], fill=(150, 150, 152))
            d.rectangle([x0, y0 + int(ph * 0.3), x1, y0 + int(ph * 0.66)], fill=(236, 232, 220))
        elif kind == 'bag':
            d.polygon([(x0, y0 + 12), (x0 + pw // 4, y0), (x1 - pw // 4, y0), (x1, y0 + 12)],
                      fill=tuple(int(c * 0.7) for c in color))
            d.ellipse([x0 + pw * 0.15, y0 + ph * 0.25, x1 - pw * 0.15, y0 + ph * 0.7], fill=ac)
        elif kind == 'carton':
            d.polygon([(x0, y0 + 26), ((x0 + x1) // 2, y0), (x1, y0 + 26)], fill=(232, 230, 224))
            d.rectangle([x0, y0 + 26, x1, y0 + int(ph * 0.4)], fill=(236, 234, 228))
        else:
            d.rectangle([x0 + 6, y0 + 8, x1 - 6, y0 + int(ph * 0.45)], fill=ac)
        size_box = (pw - 10, max(10, min(40, int(ph * 0.16))))
        f = W.fit_font(W.COND, brand, size_box[0], size_box[1], 44)
        if kind in ('can', 'jar', 'jug', 'carton'):
            cy = y0 + int(ph * (0.48 if kind == 'can' else 0.56 if kind != 'carton' else 0.3))
            W.text_centered(d, (x0 + x1) / 2, cy, brand, f, (40, 34, 30))
        elif kind == 'bottle':
            W.text_centered(d, (x0 + x1) / 2, y0 + int(ph * 0.585), brand, f, ink)
        else:
            W.text_centered(d, (x0 + x1) / 2, y0 + int(ph * 0.24), brand, f, ink)
        # Price tag on the shelf edge under every product group start.
        d.rectangle([x0, y1 + 2, x0 + 34, y1 + 9], fill=(236, 230, 120))
    img = img.filter(ImageFilter.GaussianBlur(0.6))
    a = W.to_f(img)
    T.seed(1903)
    grain = T.fbm(2048, 64, 3)[:h, :w]
    a *= (0.94 + 0.08 * grain)[..., None]
    W.save_jpg(np.clip(a, 0, 1), 'T_Groceries', quality=88)


def aisle_signs():
    """Four hanging aisle signs, 1024 x 128 each (rows top to bottom)."""
    rows = [('1', 'CEREAL  ·  COFFEE  ·  BAKING'), ('2', 'CANNED GOODS  ·  SOUP  ·  PASTA'),
            ('3', 'SNACKS  ·  SODA  ·  CANDY'), ('4', 'HOUSEHOLD  ·  FIRST AID  ·  FILM')]
    w, rh = 1024, 128
    img = Image.new('RGB', (w, rh * len(rows)), (240, 240, 236))
    d = ImageDraw.Draw(img)
    for i, (num, text) in enumerate(rows):
        y0 = i * rh
        d.rectangle([0, y0, w, y0 + rh - 1], fill=(26, 70, 140))
        d.rectangle([6, y0 + 6, w - 7, y0 + rh - 7], outline=(236, 236, 232), width=4)
        d.ellipse([22, y0 + 18, 22 + 92, y0 + 110], fill=(236, 200, 60))
        W.text_centered(d, 68, y0 + 64, num, W.fit_font(W.OSWALD, num, 60, 70, 80), (26, 40, 80))
        W.text_centered(d, (130 + w) / 2, y0 + 64, text, W.fit_font(W.COND, text, w - 180, 64, 80, 3),
                        (240, 242, 238), 3)
    img = img.filter(ImageFilter.GaussianBlur(0.5))
    W.save_jpg(W.to_f(img), 'T_Aisle_Signs')


# =======================================================================================
# Posters

def poster_bank():
    w, h = 512, 768
    img = Image.new('RGB', (w, h), (232, 226, 206))
    d = ImageDraw.Draw(img)
    green = (20, 60, 44)
    gold = (180, 140, 60)
    d.rectangle([18, 18, w - 19, h - 19], outline=green, width=8)
    W.text_centered(d, w / 2, 96, 'SAVE FOR A', W.fit_font(W.OSWALD, 'SAVE FOR A', w - 120, 70, 90, 4), green, 4)
    W.text_centered(d, w / 2, 178, 'RAINY NIGHT', W.fit_font(W.OSWALD, 'RAINY NIGHT', w - 80, 90, 110, 4), green, 4)
    # An umbrella in the rain, a coin stack sheltered under it.
    cx, cy = w / 2, 360
    rnd = np.random.default_rng(5)
    for _ in range(60):
        x, y = rnd.uniform(40, w - 40), rnd.uniform(230, 520)
        if abs(x - cx) < 160 and y > cy - 110:
            continue
        d.line([(x, y), (x - 6, y + 22)], fill=(90, 110, 130), width=3)
    d.pieslice([cx - 150, cy - 120, cx + 150, cy + 120], 180, 360, fill=(150, 40, 34))
    for k in range(4):
        x = cx - 150 + k * 75
        d.pieslice([x, cy - 24, x + 75, cy + 24], 0, 180, fill=(232, 226, 206))
    d.line([cx, cy - 120, cx, cy + 120], fill=(40, 34, 30), width=8)
    d.arc([cx - 36, cy + 96, cx + 4, cy + 140], 0, 180, fill=(40, 34, 30), width=8)
    for k in range(5):
        y = cy + 140 - k * 16
        d.ellipse([cx + 40, y - 10, cx + 130, y + 10], fill=(200, 160, 70), outline=(120, 90, 30), width=3)
    for line_y, text in ((590, '2% INTEREST EVERY NIGHT'), (640, 'NEVER TOUCHED BY BAIL'),
                         (690, 'HOLLOWMERE SAVINGS & LOAN')):
        W.text_centered(d, w / 2, line_y, text, W.fit_font(W.COND, text, w - 90, 40, 46, 2),
                        gold if line_y == 690 else green, 2)
    img = img.filter(ImageFilter.GaussianBlur(0.5))
    a = W.to_f(img)
    T.seed(711)
    n = T.fbm(768, 8, 4)[:h, :w]
    a = a * (0.9 + 0.1 * n[..., None]) * np.array([1.0, 0.98, 0.93])
    W.save_jpg(np.clip(a, 0, 1), 'T_Poster_Bank')


def poster_market():
    w, h = 512, 768
    img = Image.new('RGB', (w, h), (246, 236, 200))
    d = ImageDraw.Draw(img)
    red = (190, 30, 26)
    d.rectangle([0, 0, w, 150], fill=red)
    W.text_centered(d, w / 2, 76, 'THIS WEEK', W.fit_font(W.OSWALD, 'THIS WEEK', w - 80, 100, 120, 6), (250, 240, 210), 6)
    items = [('ROCK SALT, 5 LB', '$2.49'), ('FLASH FILM, 12 EXP', '$4.99'), ('FIRST AID KIT', '$6.50'),
             ('HOLLOW COLA 6-PK', '$1.89'), ('CANDLES (DOZEN)', '$3.25'), ('OATY-O\'S', '$1.19')]
    for i, (name, price) in enumerate(items):
        y = 210 + i * 86
        W.text_centered(d, 170, y, name, W.fit_font(W.COND, name, 280, 40, 44, 1), (40, 34, 30), 1)
        d.ellipse([340, y - 36, 480, y + 36], fill=(250, 214, 60), outline=red, width=4)
        W.text_centered(d, 410, y, price, W.fit_font(W.OSWALD, price, 120, 44, 50), red)
    W.text_centered(d, w / 2, h - 40, 'PICK & SAVE  ·  OPEN 8 TO 8', W.fit_font(W.COND, 'PICK & SAVE  ·  OPEN 8 TO 8', w - 60, 34, 40, 2), red, 2)
    img = img.filter(ImageFilter.GaussianBlur(0.5))
    W.save_jpg(W.to_f(img), 'T_Poster_Market')


# =======================================================================================
# Police tape: "POLICE LINE  DO NOT CROSS", tiles along U (1 tile = 2 m of tape)

def police_tape():
    w, h = 1024, 64
    img = Image.new('RGB', (w, h), (232, 196, 24))
    d = ImageDraw.Draw(img)
    text = 'POLICE LINE   DO NOT CROSS   '
    f = W.fit_font(W.COND, text, w / 2 - 20, h - 18, 60, 2)
    for k in range(2):
        W.text_centered(d, w / 4 + k * w / 2, h / 2, text.strip(), f, (16, 14, 12), 2)
    d.rectangle([0, 0, w, 4], fill=(20, 18, 14))
    d.rectangle([0, h - 5, w, h], fill=(20, 18, 14))
    W.save_jpg(W.to_f(img), 'T_Police_Tape')


ALL = [sign_bank, sign_market, sign_gazette, sign_pharmacy, sign_barber, sign_playground, atm_screen, groceries,
       aisle_signs, poster_bank, poster_market, police_tape]

if __name__ == '__main__':
    os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..'))
    only = set(sys.argv[1:])
    for fn in ALL:
        if only and fn.__name__ not in only:
            continue
        print('->', fn.__name__, flush=True)
        fn()
