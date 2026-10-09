"""
One-off textures that carry the story: decals, the town map, notes pinned to the job board,
posters, a calendar, the van livery, rain streaks - plus UI images and icons for Slate.
Run from the HorrorHeist folder:  python Tools/AssetGen/textures/gen_unique.py
"""
import math
import os
import sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

sys.path.insert(0, os.path.dirname(__file__))
import texlib as T  # noqa: E402

OUT = 'SourceArt/Textures/Unique'
DECALS = 'SourceArt/Textures/Decals'
UI = 'Content/Slate/Images'
FONTS = 'Content/Slate/Fonts'


def font(name, size):
    return ImageFont.truetype(os.path.join(FONTS, name), size)


TYPE = lambda s: font('SpecialElite-Regular.ttf', s)          # noqa: E731
HAND = lambda s: font('Caveat-Medium.ttf', s)                 # noqa: E731
SCRAWL = lambda s: font('ReenieBeanie-Regular.ttf', s)        # noqa: E731
DISPLAY = lambda s: font('Oswald-SemiBold.ttf', s)            # noqa: E731
DISPLAY_M = lambda s: font('Oswald-Medium.ttf', s)            # noqa: E731
BODY = lambda s: font('Barlow-Medium.ttf', s)                 # noqa: E731
BODY_R = lambda s: font('Barlow-Regular.ttf', s)              # noqa: E731
COND = lambda s: font('BarlowCondensed-SemiBold.ttf', s)      # noqa: E731


def np_to_img(arr, mode='RGB'):
    return Image.fromarray(np.clip(arr * 255, 0, 255).astype(np.uint8), mode)


def aged_paper(w, h, tone=(0.86, 0.82, 0.70), seed=1, stains=0.3):
    T.seed(seed)
    size = max(w, h)
    n = T.fbm(size, 4, 6)[:h, :w]
    fine = T.fbm(size, 128, 2)[:h, :w]
    st = T.smoothstep(1 - stains - 0.05, 1 - stains + 0.05, T.fbm(size, 3, 5))[:h, :w]
    yy, xx = np.mgrid[0:h, 0:w]
    edge = np.minimum(np.minimum(xx, w - 1 - xx), np.minimum(yy, h - 1 - yy)) / (0.08 * min(w, h))
    edge = np.clip(edge, 0, 1)
    base = np.array(tone)[None, None, :] * (0.9 + 0.12 * n[..., None] + 0.05 * fine[..., None])
    base = T.lerp(base, base * np.array([0.82, 0.72, 0.55]), st * 0.5)
    base = base * (0.75 + 0.25 * edge[..., None])
    return np_to_img(base)


def ink(draw, xy, text, fnt, fill=(30, 26, 24), anchor='la'):
    draw.text(xy, text, font=fnt, fill=fill, anchor=anchor)


# =======================================================================================
# Decals

def decals():
    T.seed(501)
    size = 512
    yy, xx = np.mgrid[0:size, 0:size] / size - 0.5
    r = np.sqrt(xx ** 2 + yy ** 2)

    # Oil stain
    shape = T.warp(1 - T.smoothstep(0.18, 0.42, r), 0.08, 4)
    inner = T.fbm(size, 8, 4)
    alpha = shape * (0.6 + 0.4 * inner)
    rgb = np.stack([np.full((size, size), 0.05), np.full((size, size), 0.045), np.full((size, size), 0.04)], -1)
    T.save_rgba(os.path.join(DECALS, 'T_Decal_OilStain.png'), np.concatenate([rgb, alpha[..., None]], -1))

    # Water stain with tide marks
    T.seed(502)
    shape = T.warp(1 - T.smoothstep(0.3, 0.45, r), 0.1, 3)
    rings = (np.sin(T.warp(r, 0.05, 6) * 70) * 0.5 + 0.5) ** 8
    alpha = np.clip(shape * 0.35 + rings * shape * 0.5, 0, 1)
    rgb = np.stack([np.full((size, size), 0.32), np.full((size, size), 0.26), np.full((size, size), 0.17)], -1)
    T.save_rgba(os.path.join(DECALS, 'T_Decal_WaterStain.png'), np.concatenate([rgb, alpha[..., None]], -1))

    # Grime streaks (run downwards from the top edge)
    T.seed(503)
    h, w = 1024, 512
    cols = np.random.default_rng(3).random(w)
    cols = T.blur(np.tile(cols, (h, 1)), 2)
    fall = np.linspace(1, 0, h)[:, None] ** 1.5
    alpha = np.clip((cols - 0.35) * 1.6, 0, 1) * fall * 0.75
    alpha *= T.fbm(1024, 8, 4)[:h, :w] * 0.8 + 0.4
    rgb = np.stack([np.full((h, w), 0.12), np.full((h, w), 0.11), np.full((h, w), 0.08)], -1)
    T.save_rgba(os.path.join(DECALS, 'T_Decal_Grime.png'), np.concatenate([rgb, np.clip(alpha, 0, 1)[..., None]], -1))

    # Muddy boot prints (walking path, left/right)
    img = Image.new('RGBA', (1024, 256), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    for i in range(6):
        x = 60 + i * 160
        y = 80 if i % 2 == 0 else 150
        d.rounded_rectangle([x, y - 26, x + 70, y + 26], radius=24, fill=(48, 36, 24, 200))
        d.rounded_rectangle([x + 82, y - 20, x + 112, y + 20], radius=14, fill=(48, 36, 24, 190))
        for k in range(5):
            d.line([x + 8 + k * 12, y - 22, x + 8 + k * 12, y + 22], fill=(30, 22, 15, 120), width=3)
    img = img.filter(ImageFilter.GaussianBlur(1.5))
    noise = np_to_img(T.fbm(1024, 32, 4)[:256, :1024], 'L')
    a = np.array(img).astype(float)
    a[..., 3] *= (np.array(noise) / 255.0) * 0.8 + 0.3
    Image.fromarray(np.clip(a, 0, 255).astype(np.uint8), 'RGBA').save(os.path.join(DECALS, 'T_Decal_Footprints.png'))

    # Hairline cracks
    T.seed(504)
    edges, _ = T.worley(512, 5, 'f2')
    line = 1 - T.smoothstep(0.0, 0.012, edges)
    keep = T.value_noise(512, 10) < 0.5
    line = T.warp(line * keep, 0.02, 6) * (1 - T.smoothstep(0.3, 0.5, r))
    rgb = np.zeros((512, 512, 3)) + 0.08
    T.save_rgba(os.path.join(DECALS, 'T_Decal_Cracks.png'), np.concatenate([rgb, line[..., None]], -1))


# =======================================================================================
# Town map

def town_map():
    w, h = 2048, 1536
    img = aged_paper(w, h, (0.87, 0.84, 0.74), seed=11, stains=0.22)
    d = ImageDraw.Draw(img)
    rnd = np.random.default_rng(12)
    ink_c = (54, 50, 46)

    # River (Mill Brook) across the town.
    pts = []
    for i in range(0, 41):
        x = i / 40 * w
        y = h * 0.62 + math.sin(i / 40 * 6.0) * 120 + math.sin(i / 40 * 17) * 25
        pts.append((x, y))
    d.line(pts, fill=(120, 140, 150), width=46, joint='curve')
    d.line(pts, fill=(150, 168, 175), width=30, joint='curve')

    # Street grid (old town) + a couple of curved roads.
    streets = []
    for gx in range(6):
        x = 260 + gx * 260 + rnd.integers(-20, 20)
        streets.append([(x, 120), (x + rnd.integers(-40, 40), h * 0.55)])
    for gy in range(4):
        y = 200 + gy * 190 + rnd.integers(-15, 15)
        streets.append([(140, y), (w - 200, y + rnd.integers(-30, 30))])
    for s in streets:
        d.line(s, fill=(240, 236, 224), width=26)
        d.line(s, fill=ink_c, width=2)
    curve = [(200 + i * 45, h * 0.7 + 260 * math.sin(i / 36 * math.pi) * 0.6) for i in range(37)]
    d.line(curve, fill=(240, 236, 224), width=28, joint='curve')
    d.line(curve, fill=ink_c, width=2, joint='curve')

    # Blocks of houses.
    for _ in range(260):
        x = rnd.integers(150, w - 220)
        y = rnd.integers(130, h - 160)
        if abs(y - (h * 0.62 + math.sin(x / w * 6.0) * 120)) < 70:
            continue
        s = rnd.integers(14, 30)
        d.rectangle([x, y, x + s, y + int(s * 0.7)], outline=ink_c, width=2)

    title = DISPLAY(96)
    d.text((w // 2, 40), 'HOLLOWMERE', font=title, fill=(40, 36, 32), anchor='ma')
    d.text((w // 2, 150), 'TOWN SURVEY  ·  NOT TO SCALE', font=TYPE(28), fill=(70, 64, 58), anchor='ma')
    labels = [
        ('Ashgrove Ln', 330, 395, -2), ('Marrow St', 1240, 585, 0), ('Church Rd', 860, 240, 0),
        ('Mill Brook', 1500, h * 0.62 + 40, 0), ('Old Quarry Rd', 520, 1180, 12), ('Whitlock Farm ->', 1650, 1300, 0),
        ('Museum', 1080, 330, 0), ('St. Agnes', 1560, 220, 0), ('Laundromat', 640, 760, 0),
    ]
    for text, x, y, rot in labels:
        layer = Image.new('RGBA', (500, 80), (0, 0, 0, 0))
        ImageDraw.Draw(layer).text((250, 40), text, font=TYPE(30), fill=(52, 46, 40, 255), anchor='mm')
        layer = layer.rotate(rot, expand=True, resample=Image.BICUBIC)
        img.paste(layer, (int(x - layer.width / 2), int(y - layer.height / 2)), layer)

    # Crew annotations: red circles, crosses and handwriting.
    red = (168, 30, 26)
    circles = [(360, 330, 'VANCE - 14', 'no alarm. dog?'), (1260, 520, 'MARROW HOUSE', 'the painting!!'),
               (1580, 160, 'RECTORY', 'never at night'), (1700, 1240, 'WHITLOCK', 'NO.')]
    for x, y, label, note in circles:
        d.ellipse([x - 70, y - 50, x + 70, y + 50], outline=red, width=6)
        d.text((x + 80, y - 40), label, font=HAND(46), fill=red)
        d.text((x + 86, y + 6), note, font=SCRAWL(46), fill=red)
    d.line([(640, 760), (700, 700)], fill=red, width=4)
    d.text((710, 650), 'us', font=SCRAWL(56), fill=red)
    d.text((140, h - 120), 'stay off Marrow St after 11 - patrol car', font=HAND(42), fill=(28, 40, 110))

    img = img.filter(ImageFilter.GaussianBlur(0.6))
    img.save(os.path.join(OUT, 'T_Map_Hollowmere.jpg'), quality=90)


# =======================================================================================
# Notes atlas (4x2 notes, 512 each)

NOTES = [
    ('index', ['14 ASHGROVE LN', 'Vance family', '- 2 adults, maybe kid', '- back door sticks', '- they never leave?'], TYPE, 30),
    ('sticky', ['DONT', 'GO IN THE', 'BASEMENT'], SCRAWL, 74),
    ('paper', ['Brannigan says:', 'in by 1, out by 3.', 'no lights. no names.', 'no matter what you hear.'], HAND, 44),
    ('index', ['THE FENCE', 'pays 40% of value', 'cash only', 'ask for "Dutch"'], TYPE, 32),
    ('sticky', ['the hallway', 'was longer', 'the 2nd time'], HAND, 52),
    ('paper', ['Inventory:', '- crowbar', '- 2 torches', '- glass cutter', '- bags (big)'], HAND, 44),
    ('index', ['MARROW HOUSE', 'family gone since 1974', 'still lights on upstairs?', 'museum painting', '"Lantern Bearer"'], TYPE, 28),
    ('sticky', ['who moved', 'the van', 'keys??'], SCRAWL, 76),
]


def notes_atlas():
    atlas = Image.new('RGB', (2048, 1024), (0, 0, 0))
    for i, (kind, lines, fnt, size) in enumerate(NOTES):
        if kind == 'sticky':
            tile = aged_paper(512, 512, (0.93, 0.84, 0.42), seed=40 + i, stains=0.1)
            colour = (40, 36, 30)
        elif kind == 'index':
            tile = aged_paper(512, 512, (0.92, 0.9, 0.84), seed=40 + i, stains=0.15)
            dd = ImageDraw.Draw(tile)
            for k in range(1, 9):
                dd.line([(0, 80 + k * 52), (512, 80 + k * 52)], fill=(140, 165, 190), width=2)
            dd.line([(0, 80), (512, 80)], fill=(200, 90, 90), width=3)
            colour = (34, 32, 34)
        else:
            tile = aged_paper(512, 512, (0.86, 0.84, 0.78), seed=40 + i, stains=0.25)
            colour = (30, 34, 70)
        dd = ImageDraw.Draw(tile)
        y = 40 if kind != 'index' else 30
        for line in lines:
            dd.text((36, y), line, font=fnt(size), fill=colour)
            y += int(size * 1.25)
        tile = tile.rotate(np.random.default_rng(i).uniform(-1.5, 1.5), resample=Image.BICUBIC, fillcolor=tile.getpixel((5, 5)))
        atlas.paste(tile, ((i % 4) * 512, (i // 4) * 512))
    atlas.save(os.path.join(OUT, 'T_Notes_Atlas.jpg'), quality=90)


# =======================================================================================
# Posters, calendar, newspaper, signs

def missing_poster():
    w, h = 768, 1024
    img = aged_paper(w, h, (0.88, 0.86, 0.80), seed=61, stains=0.3)
    d = ImageDraw.Draw(img)
    d.text((w // 2, 40), 'MISSING', font=DISPLAY(150), fill=(24, 22, 22), anchor='ma')
    d.text((w // 2, 215), 'HAVE YOU SEEN THIS GIRL?', font=COND(44), fill=(30, 28, 28), anchor='ma')
    # Halftone "photo": a faint, unreadable portrait silhouette.
    T.seed(62)
    ph = np.ones((360, 300)) * 0.75
    yy, xx = np.mgrid[0:360, 0:300]
    head = ((xx - 150) / 70) ** 2 + ((yy - 140) / 88) ** 2 < 1
    shoulders = ((xx - 150) / 140) ** 2 + ((yy - 380) / 140) ** 2 < 1
    ph[head | shoulders] = 0.25
    ph = np.array(Image.fromarray((ph * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(6))) / 255.0
    dots = Image.new('L', (300, 360), 235)
    dd = ImageDraw.Draw(dots)
    for y in range(0, 360, 8):
        for x in range(0, 300, 8):
            r = (1 - ph[y, x]) * 4.6
            dd.ellipse([x + 4 - r, y + 4 - r, x + 4 + r, y + 4 + r], fill=30)
    img.paste(dots.convert('RGB'), (w // 2 - 150, 290))
    d.rectangle([w // 2 - 152, 288, w // 2 + 152, 652], outline=(30, 28, 28), width=3)
    lines = ['ELIZA MARROW, age 9', 'Last seen: Marrow St., Hollowmere', 'October 31st', 'Brown hair. Yellow raincoat.',
             'Any information: Hollowmere Police', 'Tel. 555-0118']
    y = 680
    for line in lines:
        d.text((w // 2, y), line, font=TYPE(32), fill=(28, 26, 26), anchor='ma')
        y += 48
    # Someone wrote on it.
    d.text((70, 950), 'still here', font=SCRAWL(58), fill=(150, 26, 22))
    img.save(os.path.join(OUT, 'T_Poster_Missing.jpg'), quality=90)


def museum_poster():
    w, h = 768, 1024
    T.seed(63)
    img = Image.new('RGB', (w, h), (26, 34, 40))
    d = ImageDraw.Draw(img)
    # Stylised "painting": a figure holding a lantern in a dark doorway.
    d.rectangle([110, 160, 658, 700], fill=(16, 18, 20), outline=(176, 146, 92), width=10)
    d.rectangle([300, 260, 470, 690], fill=(8, 9, 10))
    glow = Image.new('L', (w, h), 0)
    gd = ImageDraw.Draw(glow)
    gd.ellipse([330, 380, 440, 490], fill=255)
    glow = glow.filter(ImageFilter.GaussianBlur(40))
    img = Image.composite(Image.new('RGB', (w, h), (230, 170, 80)), img, glow.point(lambda v: int(v * 0.8)))
    d = ImageDraw.Draw(img)
    d.ellipse([370, 420, 400, 450], fill=(255, 220, 150))
    d.polygon([(355, 470), (415, 470), (440, 690), (330, 690)], fill=(12, 12, 14))
    d.ellipse([360, 330, 410, 380], fill=(12, 12, 14))
    d.text((w // 2, 40), 'HOLLOWMERE', font=DISPLAY_M(60), fill=(214, 188, 130), anchor='ma')
    d.text((w // 2, 108), 'HISTORICAL MUSEUM', font=COND(34), fill=(200, 196, 184), anchor='ma')
    d.text((w // 2, 735), 'THE LANTERN BEARER', font=DISPLAY(66), fill=(232, 222, 196), anchor='ma')
    d.text((w // 2, 815), 'and other works by Josiah Marrow (1871-1932)', font=BODY_R(28), fill=(200, 196, 184), anchor='ma')
    d.text((w // 2, 870), 'Special exhibition  ·  East Gallery', font=BODY_R(26), fill=(160, 156, 146), anchor='ma')
    d.text((w // 2, 940), 'Open daily 10 - 4  ·  Closed after dark', font=COND(28), fill=(214, 188, 130), anchor='ma')
    img = img.filter(ImageFilter.GaussianBlur(0.5))
    paper = np.array(aged_paper(w, h, (1, 1, 1), seed=64, stains=0.35)).astype(float) / 255
    out_ = np.array(img).astype(float) / 255 * (0.55 + 0.45 * paper)
    np_to_img(out_).save(os.path.join(OUT, 'T_Poster_Museum.jpg'), quality=90)


def calendar():
    w, h = 768, 1024
    img = aged_paper(w, h, (0.92, 0.91, 0.87), seed=65, stains=0.15)
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, w, 360], fill=(54, 72, 66))
    d.text((w // 2, 120), 'MARLOWE\'S', font=DISPLAY(70), fill=(232, 226, 208), anchor='ma')
    d.text((w // 2, 210), 'COIN LAUNDRY  ·  WASH & FOLD', font=COND(32), fill=(214, 206, 180), anchor='ma')
    d.text((w // 2, 400), 'OCTOBER', font=DISPLAY(64), fill=(40, 40, 40), anchor='ma')
    days = ['S', 'M', 'T', 'W', 'T', 'F', 'S']
    cell = (w - 80) / 7
    for i, dname in enumerate(days):
        d.text((40 + i * cell + cell / 2, 500), dname, font=COND(30), fill=(90, 90, 90), anchor='ma')
    day = 1
    for row in range(5):
        for col in range(7):
            if row == 0 and col < 2:
                continue
            if day > 31:
                break
            x = 40 + col * cell
            y = 550 + row * 88
            d.rectangle([x, y, x + cell, y + 88], outline=(160, 160, 156), width=1)
            d.text((x + 8, y + 6), str(day), font=BODY(26), fill=(50, 50, 50))
            if day < 24:
                d.line([(x + 10, y + 12), (x + cell - 10, y + 76)], fill=(150, 30, 26), width=4)
                d.line([(x + cell - 10, y + 12), (x + 10, y + 76)], fill=(150, 30, 26), width=4)
            if day == 31:
                d.ellipse([x + 4, y + 4, x + cell - 4, y + 84], outline=(150, 30, 26), width=5)
                d.text((x + cell / 2, y + 40), 'JOB', font=SCRAWL(40), fill=(150, 30, 26), anchor='ma')
            day += 1
    img.save(os.path.join(OUT, 'T_Calendar.jpg'), quality=90)


def newspaper():
    w, h = 1024, 1024
    img = aged_paper(w, h, (0.86, 0.84, 0.78), seed=66, stains=0.3)
    d = ImageDraw.Draw(img)
    d.text((w // 2, 30), 'The Hollowmere Gazette', font=font('Oswald-SemiBold.ttf', 78), fill=(20, 20, 20), anchor='ma')
    d.line([(40, 140), (w - 40, 140)], fill=(20, 20, 20), width=4)
    d.text((w // 2, 160), 'THIRD BREAK-IN ON ASHGROVE LANE', font=DISPLAY(58), fill=(18, 18, 18), anchor='ma')
    d.text((w // 2, 240), 'Residents report "strange sounds" - police find no one inside', font=TYPE(30), fill=(40, 40, 40), anchor='ma')
    rnd = np.random.default_rng(67)
    # Body copy as grey "text lines"; the two right columns flow around the photo.
    for col in range(3):
        x0 = 40 + col * 320
        y = 310
        while y < h - 40:
            if col > 0 and 318 < y < 700:
                y += 24
                continue
            lw = rnd.integers(200, 290)
            d.line([(x0, y), (x0 + lw, y)], fill=(90, 88, 84), width=6)
            y += 24
    # Halftone photo of a gabled house with one lit upstairs window.
    pw, ph_ = 560, 320
    photo = np.full((ph_, pw), 0.72)
    yy, xx = np.mgrid[0:ph_, 0:pw]
    sky = np.clip(1 - yy / ph_, 0, 1) * 0.15
    photo += sky
    house = (xx > 150) & (xx < 410) & (yy > 140)
    roof = (yy > 60) & (yy <= 140) & (np.abs(xx - 280) < (yy - 60) * 1.7)
    trees = (T.fbm(560, 6, 4)[:ph_, :pw] > 0.55) & (yy > 120) & ((xx < 140) | (xx > 430))
    photo[house | roof | trees] = 0.18
    photo[(xx > 300) & (xx < 340) & (yy > 170) & (yy < 215)] = 0.95
    photo[(xx > 190) & (xx < 230) & (yy > 170) & (yy < 215)] = 0.3
    photo[yy > 290] = 0.4
    photo = np.array(Image.fromarray((np.clip(photo, 0, 1) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(3))) / 255.0
    dots = Image.new('L', (pw, ph_), 220)
    dd = ImageDraw.Draw(dots)
    for y in range(0, ph_, 6):
        for x in range(0, pw, 6):
            r = (1 - photo[y, x]) * 3.6
            dd.ellipse([x + 3 - r, y + 3 - r, x + 3 + r, y + 3 + r], fill=25)
    img.paste(dots.convert('RGB'), (380, 330))
    d.rectangle([378, 328, 942, 652], outline=(30, 30, 30), width=2)
    d.text((660, 664), 'The Marrow house, photographed from the road.', font=TYPE(22), fill=(50, 50, 50), anchor='ma')
    img.save(os.path.join(OUT, 'T_Newspaper.jpg'), quality=88)


def signs():
    img = Image.new('RGB', (1024, 512), (24, 22, 20))
    d = ImageDraw.Draw(img)
    d.rectangle([20, 20, 1004, 492], outline=(200, 168, 96), width=8)
    d.text((512, 70), 'THE FENCE', font=DISPLAY(150), fill=(226, 214, 186), anchor='ma')
    d.text((512, 290), 'CASH ONLY  ·  NO RECEIPTS  ·  NO QUESTIONS', font=COND(52), fill=(200, 168, 96), anchor='ma')
    d.text((512, 380), 'ring twice, wait', font=HAND(60), fill=(180, 60, 50), anchor='ma')
    paper = np.array(aged_paper(1024, 512, (1, 1, 1), seed=68)).astype(float) / 255
    np_to_img(np.array(img).astype(float) / 255 * (0.6 + 0.4 * paper)).save(os.path.join(OUT, 'T_Sign_Fence.jpg'), quality=90)

    # Van livery (RGBA, white areas transparent)
    img = Image.new('RGBA', (2048, 512), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    green = (34, 70, 56, 255)
    d.text((1024, 40), 'BRANNIGAN & SONS', font=DISPLAY(190), fill=green, anchor='ma')
    d.text((1024, 290), 'PLUMBING  ·  HEATING  ·  DRAINS', font=COND(84), fill=(150, 40, 32, 255), anchor='ma')
    d.text((1024, 400), 'Est. 1961  ·  Hollowmere  ·  555-0137', font=BODY(56), fill=green, anchor='ma')
    a = np.array(img).astype(float) / 255
    a[..., 3] *= np.clip(T.fbm(2048, 64, 4)[:512, :2048] * 0.5 + 0.7, 0, 1)
    T.save_rgba(os.path.join(OUT, 'T_Van_Livery.png'), a)


# =======================================================================================
# Screens, rain, lens

def screens_and_rain():
    T.seed(70)
    noise = np.random.default_rng(71).random((512, 512))
    lines = (np.sin(np.arange(512) * np.pi / 2)[:, None] * 0.1 + 0.9)
    T.save_gray(os.path.join(OUT, 'T_TV_Static.png'), noise * lines)

    # Falling rain streaks (tiling vertically), RGBA white.
    h, w = 1024, 512
    streak = np.zeros((h, w))
    rnd = np.random.default_rng(72)
    for _ in range(420):
        x = rnd.integers(0, w)
        y = rnd.integers(0, h)
        length = rnd.integers(40, 160)
        a = rnd.uniform(0.15, 0.6)
        for k in range(length):
            streak[(y + k) % h, x] += a * (1 - k / length)
    streak = np.array(Image.fromarray(np.clip(streak * 255, 0, 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(0.8))) / 255.0
    rgba = np.stack([np.ones((h, w)), np.ones((h, w)), np.ones((h, w)), np.clip(streak * 1.6, 0, 1)], -1)
    T.save_rgba(os.path.join(OUT, 'T_Rain_Streaks.png'), rgba)

    # Droplets on glass: height field of round drops + running trails, as a normal map.
    T.seed(73)
    S = 512
    hgt = np.zeros((S, S))
    yy, xx = np.mgrid[0:S, 0:S]
    for _ in range(260):
        cx, cy = rnd.integers(0, S), rnd.integers(0, S)
        r = rnd.uniform(2, 9)
        d2 = ((xx - cx + S // 2) % S - S // 2) ** 2 + ((yy - cy + S // 2) % S - S // 2) ** 2
        hgt = np.maximum(hgt, np.sqrt(np.clip(1 - d2 / (r * r), 0, 1)) * r / 9)
    for _ in range(14):
        cx = rnd.integers(0, S)
        for y in range(S):
            x = int(cx + 3 * math.sin(y / 30)) % S
            hgt[y, max(0, x - 1):x + 2] = np.maximum(hgt[y, max(0, x - 1):x + 2], 0.25)
    n = T.normal_from_height(hgt, 6.0)
    T.save_rgb(os.path.join(OUT, 'T_RainDrops_N.png'), n)

    # Lens dirt for bloom.
    T.seed(74)
    S = 1024
    dirt = T.smoothstep(0.62, 0.9, T.fbm(S, 6, 6)) * 0.6
    spots = (np.random.default_rng(75).random((S, S)) > 0.9993).astype(float)
    spots = np.array(Image.fromarray((spots * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(5))) / 255.0 * 6
    lens = np.clip(dirt + spots, 0, 1)
    T.save_rgb(os.path.join(OUT, 'T_LensDirt.jpg'), np.stack([lens * 0.9, lens * 0.85, lens * 0.8], -1))


# =======================================================================================
# UI images & icons (Slate loads these from Content/Slate/Images at runtime)

def ui_images():
    os.makedirs(UI, exist_ok=True)
    S = 1024
    yy, xx = np.mgrid[0:S, 0:S] / S - 0.5
    r = np.sqrt((xx * 1.0) ** 2 + (yy * 1.25) ** 2)
    alpha = T.smoothstep(0.32, 0.78, r) ** 1.4
    T.save_rgba(os.path.join(UI, 'Vignette.png'), np.stack([np.zeros((S, S))] * 3 + [alpha * 0.95], -1))

    w = 512
    ramp = np.linspace(1, 0, w) ** 1.6
    left = np.tile(ramp, (8, 1)) * 0.94
    T.save_rgba(os.path.join(UI, 'GradientLeft.png'), np.stack([np.zeros((8, w))] * 3 + [left], -1))
    T.save_rgba(os.path.join(UI, 'GradientRight.png'), np.stack([np.zeros((8, w))] * 3 + [left[:, ::-1]], -1))
    bottom = np.tile(np.linspace(0, 1, w)[:, None] ** 1.6, (1, 8)) * 0.94
    T.save_rgba(os.path.join(UI, 'GradientBottom.png'), np.stack([np.zeros((w, 8))] * 3 + [bottom], -1))

    g = np.random.default_rng(80).random((256, 256))
    T.save_rgba(os.path.join(UI, 'Grain.png'), np.stack([g, g, g, np.full((256, 256), 0.06)], -1))

    T.seed(81)
    scratch = np.zeros((S, S))
    rnd = np.random.default_rng(82)
    for _ in range(60):
        x = rnd.integers(0, S)
        for y in range(rnd.integers(0, S // 2), S):
            scratch[y, (x + int(4 * math.sin(y / 50))) % S] = rnd.uniform(0.2, 0.6)
    T.save_rgba(os.path.join(UI, 'Scratches.png'), np.stack([np.ones((S, S))] * 3 + [scratch * 0.25], -1))


def icons():
    size = 48
    ss = 4
    S = size * ss
    W = (255, 255, 255, 255)

    def canvas():
        img = Image.new('RGBA', (S, S), (0, 0, 0, 0))
        return img, ImageDraw.Draw(img)

    def finish(img, name):
        img.resize((size, size), Image.LANCZOS).save(os.path.join(UI, f'Icon_{name}.png'))

    lw = int(4.5 * ss)

    img, d = canvas()  # Mic
    d.rounded_rectangle([S * 0.36, S * 0.12, S * 0.64, S * 0.58], radius=S * 0.14, fill=W)
    d.arc([S * 0.24, S * 0.3, S * 0.76, S * 0.72], 0, 180, fill=W, width=lw)
    d.line([S * 0.5, S * 0.72, S * 0.5, S * 0.86], fill=W, width=lw)
    d.line([S * 0.36, S * 0.86, S * 0.64, S * 0.86], fill=W, width=lw)
    finish(img, 'Mic')

    img, d = canvas()  # Lock
    d.rounded_rectangle([S * 0.22, S * 0.44, S * 0.78, S * 0.86], radius=S * 0.06, fill=W)
    d.arc([S * 0.32, S * 0.14, S * 0.68, S * 0.6], 180, 360, fill=W, width=lw)
    d.line([S * 0.32, S * 0.37, S * 0.32, S * 0.46], fill=W, width=lw)
    d.line([S * 0.68, S * 0.37, S * 0.68, S * 0.46], fill=W, width=lw)
    finish(img, 'Lock')

    img, d = canvas()  # Check
    d.line([(S * 0.18, S * 0.52), (S * 0.42, S * 0.75), (S * 0.84, S * 0.26)], fill=W, width=int(lw * 1.2), joint='curve')
    finish(img, 'Check')

    img, d = canvas()  # Crown
    d.polygon([(S * 0.14, S * 0.72), (S * 0.18, S * 0.3), (S * 0.36, S * 0.5), (S * 0.5, S * 0.22), (S * 0.64, S * 0.5), (S * 0.82, S * 0.3), (S * 0.86, S * 0.72)], fill=W)
    d.rectangle([S * 0.14, S * 0.76, S * 0.86, S * 0.84], fill=W)
    finish(img, 'Crown')

    img, d = canvas()  # ChevronLeft
    d.line([(S * 0.64, S * 0.16), (S * 0.32, S * 0.5), (S * 0.64, S * 0.84)], fill=W, width=lw, joint='curve')
    finish(img, 'ChevronLeft')
    img, d = canvas()  # ChevronRight
    d.line([(S * 0.36, S * 0.16), (S * 0.68, S * 0.5), (S * 0.36, S * 0.84)], fill=W, width=lw, joint='curve')
    finish(img, 'ChevronRight')

    img, d = canvas()  # Dot
    d.ellipse([S * 0.2, S * 0.2, S * 0.8, S * 0.8], fill=W)
    finish(img, 'Dot')

    img, d = canvas()  # Cash
    d.rounded_rectangle([S * 0.1, S * 0.28, S * 0.9, S * 0.72], radius=S * 0.05, outline=W, width=lw)
    d.ellipse([S * 0.4, S * 0.38, S * 0.6, S * 0.62], outline=W, width=lw)
    finish(img, 'Cash')

    img, d = canvas()  # Person
    d.ellipse([S * 0.34, S * 0.12, S * 0.66, S * 0.44], fill=W)
    d.pieslice([S * 0.16, S * 0.5, S * 0.84, S * 1.18], 180, 360, fill=W)
    finish(img, 'Person')

    img, d = canvas()  # Signal
    for i, hgt in enumerate((0.3, 0.5, 0.7)):
        d.rectangle([S * (0.2 + i * 0.22), S * (0.85 - hgt), S * (0.34 + i * 0.22), S * 0.85], fill=W)
    finish(img, 'Signal')

    img, d = canvas()  # Warning
    d.polygon([(S * 0.5, S * 0.1), (S * 0.92, S * 0.86), (S * 0.08, S * 0.86)], outline=W, width=lw)
    d.line([S * 0.5, S * 0.38, S * 0.5, S * 0.6], fill=W, width=lw)
    d.ellipse([S * 0.46, S * 0.68, S * 0.54, S * 0.76], fill=W)
    finish(img, 'Warning')

    img, d = canvas()  # Info
    d.ellipse([S * 0.1, S * 0.1, S * 0.9, S * 0.9], outline=W, width=lw)
    d.line([S * 0.5, S * 0.44, S * 0.5, S * 0.72], fill=W, width=lw)
    d.ellipse([S * 0.46, S * 0.26, S * 0.54, S * 0.34], fill=W)
    finish(img, 'Info')

    img, d = canvas()  # Close
    d.line([S * 0.22, S * 0.22, S * 0.78, S * 0.78], fill=W, width=lw)
    d.line([S * 0.78, S * 0.22, S * 0.22, S * 0.78], fill=W, width=lw)
    finish(img, 'Close')

    img, d = canvas()  # Rotate
    d.arc([S * 0.16, S * 0.16, S * 0.84, S * 0.84], 30, 320, fill=W, width=lw)
    d.polygon([(S * 0.86, S * 0.26), (S * 0.86, S * 0.5), (S * 0.64, S * 0.38)], fill=W)
    finish(img, 'Rotate')

    img, d = canvas()  # Zoom
    d.ellipse([S * 0.12, S * 0.12, S * 0.64, S * 0.64], outline=W, width=lw)
    d.line([S * 0.58, S * 0.58, S * 0.88, S * 0.88], fill=W, width=int(lw * 1.3))
    finish(img, 'Zoom')

    img, d = canvas()  # Star
    pts = []
    for i in range(10):
        ang = -math.pi / 2 + i * math.pi / 5
        rr = S * (0.42 if i % 2 == 0 else 0.18)
        pts.append((S * 0.5 + rr * math.cos(ang), S * 0.52 + rr * math.sin(ang)))
    d.polygon(pts, fill=W)
    finish(img, 'Star')

    img, d = canvas()  # Pin
    d.ellipse([S * 0.28, S * 0.1, S * 0.72, S * 0.54], fill=W)
    d.polygon([(S * 0.36, S * 0.44), (S * 0.64, S * 0.44), (S * 0.5, S * 0.9)], fill=W)
    finish(img, 'Pin')

    img, d = canvas()  # Van
    d.rounded_rectangle([S * 0.08, S * 0.3, S * 0.92, S * 0.72], radius=S * 0.06, fill=W)
    d.ellipse([S * 0.18, S * 0.62, S * 0.36, S * 0.8], fill=(0, 0, 0, 0), outline=W, width=lw)
    d.ellipse([S * 0.64, S * 0.62, S * 0.82, S * 0.8], fill=(0, 0, 0, 0), outline=W, width=lw)
    finish(img, 'Van')

    img, d = canvas()  # Gear
    for i in range(8):
        ang = i * math.pi / 4
        d.line([S * 0.5 + math.cos(ang) * S * 0.18, S * 0.5 + math.sin(ang) * S * 0.18,
                S * 0.5 + math.cos(ang) * S * 0.42, S * 0.5 + math.sin(ang) * S * 0.42], fill=W, width=int(lw * 1.4))
    d.ellipse([S * 0.24, S * 0.24, S * 0.76, S * 0.76], fill=W)
    d.ellipse([S * 0.4, S * 0.4, S * 0.6, S * 0.6], fill=(0, 0, 0, 0))
    finish(img, 'Gear')

    img, d = canvas()  # Door
    d.rectangle([S * 0.24, S * 0.1, S * 0.76, S * 0.9], outline=W, width=lw)
    d.ellipse([S * 0.6, S * 0.48, S * 0.68, S * 0.56], fill=W)
    finish(img, 'Door')


# =======================================================================================
# Chain-link, clock face

def chainlink():
    S = 256
    yy, xx = np.mgrid[0:S, 0:S] / S * 4
    a = np.abs(((xx + yy) % 1) - 0.5)
    b = np.abs(((xx - yy) % 1) - 0.5)
    wire = np.maximum(1 - T.smoothstep(0.03, 0.06, 0.5 - a), 1 - T.smoothstep(0.03, 0.06, 0.5 - b))
    shade = 0.55 + 0.25 * np.sin((xx + yy) * np.pi * 2) ** 2
    rgba = np.stack([shade * 0.62, shade * 0.63, shade * 0.62, wire], -1)
    T.save_rgba(os.path.join(OUT, 'T_ChainLink.png'), rgba)


def clockface():
    S = 512
    img = aged_paper(S, S, (0.88, 0.86, 0.78), seed=90, stains=0.2)
    d = ImageDraw.Draw(img)
    c = S / 2
    d.ellipse([8, 8, S - 8, S - 8], outline=(30, 30, 30), width=10)
    for i in range(60):
        ang = i / 60 * 2 * math.pi
        r0 = 0.42 * S if i % 5 else 0.38 * S
        d.line([(c + math.sin(ang) * r0, c - math.cos(ang) * r0),
                (c + math.sin(ang) * 0.45 * S, c - math.cos(ang) * 0.45 * S)], fill=(30, 30, 30), width=3 if i % 5 else 7)
    for h in range(1, 13):
        ang = h / 12 * 2 * math.pi
        d.text((c + math.sin(ang) * 0.31 * S, c - math.cos(ang) * 0.31 * S), str(h), font=DISPLAY_M(44),
               fill=(30, 30, 30), anchor='mm')
    d.text((c, c + 70), 'HOLLOWMERE SAVINGS', font=COND(22), fill=(110, 40, 30), anchor='mm')
    # Stopped at 3:17.
    for frac, length, width in ((3 / 12 + 17 / 720, 0.22, 12), (17 / 60, 0.36, 7)):
        ang = frac * 2 * math.pi
        d.line([(c, c), (c + math.sin(ang) * length * S, c - math.cos(ang) * length * S)], fill=(20, 20, 20), width=width)
    d.ellipse([c - 12, c - 12, c + 12, c + 12], fill=(20, 20, 20))
    img.save(os.path.join(OUT, 'T_ClockFace.jpg'), quality=90)


# =======================================================================================
# Night photographs (mission dossiers + polaroids on the job board)

def _poly(draw, pts, fill):
    draw.polygon([(float(x), float(y)) for x, y in pts], fill=fill)


def night_photo(kind, w=1024, h=768, seed=0):
    T.seed(300 + seed)
    rnd = np.random.default_rng(300 + seed)
    size = max(w, h)
    yy = np.linspace(0, 1, h)[:, None]
    horizon = 0.62
    sky = T.lerp(np.array([0.035, 0.04, 0.06]), np.array([0.16, 0.17, 0.19]), np.clip(yy / horizon, 0, 1) ** 2)[..., :] * np.ones((h, w, 1))
    clouds = T.fbm(size, 3, 6)[:h, :w]
    sky = sky * (0.8 + 0.4 * clouds[..., None])
    img = np.array(sky)
    ground = np.clip((yy - horizon) / (1 - horizon), 0, 1)
    grass = T.fbm(size, 64, 4)[:h, :w]
    flash = np.clip((yy - horizon) / (1 - horizon), 0, 1) ** 1.6
    ground_col = np.array([0.03, 0.035, 0.03]) + flash[..., None] * np.array([0.18, 0.2, 0.17]) * (0.6 + 0.8 * grass[..., None])
    img = np.where((yy > horizon)[..., None] * np.ones((1, w, 1)) > 0, ground_col * np.ones((h, w, 1)), img)

    # Tree line.
    tree_n = T.fbm(size, 24, 5)[0, :w]
    tree_top = (horizon - 0.06 - 0.12 * tree_n)[None, :]
    trees = (yy > tree_top) & (yy < horizon + 0.01)
    img[trees] = np.array([0.015, 0.018, 0.02])

    layer = Image.new('L', (w, h), 0)          # house silhouette mask
    lights = Image.new('RGB', (w, h), (0, 0, 0))
    d = ImageDraw.Draw(layer)
    dl = ImageDraw.Draw(lights)
    hy = int(horizon * h)
    warm = (255, 170, 90)

    def window(x0, y0, x1, y1, lit, colour=warm):
        if lit:
            dl.rectangle([x0, y0, x1, y1], fill=colour)
        else:
            dl.rectangle([x0, y0, x1, y1], fill=(10, 10, 14))
        dl.line([(x0 + x1) / 2, y0, (x0 + x1) / 2, y1], fill=(0, 0, 0), width=3)
        dl.line([x0, (y0 + y1) / 2, x1, (y0 + y1) / 2], fill=(0, 0, 0), width=3)

    if kind == 'vance':
        _poly(d, [(300, hy + 40), (300, hy - 170), (520, hy - 300), (740, hy - 170), (740, hy + 40)], 255)
        d.rectangle([740, hy - 90, 900, hy + 40], fill=255)
        _poly(d, [(735, hy - 90), (820, hy - 150), (905, hy - 90)], 255)
        d.rectangle([640, hy - 270, 670, hy - 200], fill=255)
        window(350, hy - 130, 420, hy - 60, False)
        window(470, hy - 130, 540, hy - 60, True)
        window(600, hy - 130, 670, hy - 60, False)
        window(490, hy - 240, 550, hy - 190, False)
        dl.rectangle([560, hy - 40, 610, hy + 40], fill=(16, 14, 12))
        d.rectangle([170, hy + 20, 178, hy + 110], fill=255)
        d.rectangle([150, hy + 6, 200, hy + 30], fill=255)
    elif kind == 'marrow':
        _poly(d, [(260, hy + 40), (260, hy - 220), (420, hy - 360), (580, hy - 220), (580, hy + 40)], 255)
        d.rectangle([580, hy - 160, 820, hy + 40], fill=255)
        _poly(d, [(570, hy - 160), (700, hy - 250), (830, hy - 160)], 255)
        d.rectangle([180, hy - 300, 270, hy + 40], fill=255)
        _poly(d, [(170, hy - 300), (225, hy - 420), (280, hy - 300)], 255)
        for x in (300, 380, 460, 610, 700):
            window(x, hy - 120, x + 50, hy - 40, False)
        window(395, hy - 290, 445, hy - 240, True, (255, 200, 140))
        window(205, hy - 250, 245, hy - 200, False)
        # Dead tree.
        dt = [(900, hy + 40), (905, hy - 260)]
        d.line(dt, fill=255, width=16)
        for k in range(9):
            y0 = hy - 120 - k * 16
            d.line([(905, y0), (905 + (-1) ** k * rnd.integers(50, 120), y0 - rnd.integers(30, 80))], fill=255, width=6)
    elif kind == 'rectory':
        d.rectangle([180, hy - 200, 640, hy + 40], fill=255)
        _poly(d, [(170, hy - 200), (410, hy - 320), (650, hy - 200)], 255)
        d.rectangle([700, hy - 380, 800, hy + 40], fill=255)
        _poly(d, [(690, hy - 380), (750, hy - 560), (810, hy - 380)], 255)
        d.line([(750, hy - 560), (750, hy - 610)], fill=255, width=6)
        d.line([(730, hy - 590), (770, hy - 590)], fill=255, width=6)
        for x in (230, 330, 430, 530):
            dl.rectangle([x, hy - 140, x + 46, hy - 50], fill=(12, 12, 16))
            dl.ellipse([x, hy - 163, x + 46, hy - 117], fill=(12, 12, 16))
        dl.ellipse([735, hy - 330, 765, hy - 300], fill=(60, 40, 20))
        for x in range(0, w, 26):
            d.line([(x, hy + 30), (x, hy + 130)], fill=255, width=4)
            _poly(d, [(x - 6, hy + 34), (x, hy + 18), (x + 6, hy + 34)], 255)
        d.line([(0, hy + 50), (w, hy + 50)], fill=255, width=4)
        d.line([(0, hy + 110), (w, hy + 110)], fill=255, width=4)
    elif kind == 'whitlock':
        _poly(d, [(120, hy + 20), (120, hy - 120), (250, hy - 220), (380, hy - 120), (380, hy + 20)], 255)
        _poly(d, [(560, hy + 20), (560, hy - 170), (640, hy - 260), (800, hy - 260), (880, hy - 170), (880, hy + 20)], 255)
        d.line([(980, hy + 20), (980, hy - 260)], fill=255, width=8)
        for k in range(4):
            ang = k * math.pi / 2 + 0.3
            d.line([(980, hy - 260), (980 + math.cos(ang) * 90, hy - 260 + math.sin(ang) * 90)], fill=255, width=10)
        window(170, hy - 90, 220, hy - 40, False)
        window(280, hy - 90, 330, hy - 40, False)
        dl.rectangle([690, hy - 80, 750, hy + 20], fill=(150, 12, 8))
    elif kind == 'van':
        _poly(d, [(200, hy + 120), (200, hy - 140), (260, hy - 200), (760, hy - 200), (800, hy - 120), (840, hy - 40), (840, hy + 120)], 255)
        dl.rectangle([270, hy - 180, 360, hy - 110], fill=(12, 12, 16))
        d.ellipse([280, hy + 80, 380, hy + 180], fill=255)
        d.ellipse([660, hy + 80, 760, hy + 180], fill=255)
    elif kind == 'hallway':
        img[:] = np.array([0.02, 0.02, 0.022])
        d.rectangle([0, 0, w, h], fill=255)
        dl.rectangle([440, 200, 600, 640], fill=(22, 18, 14))
        dl.rectangle([470, 230, 570, 640], fill=(4, 4, 5))
        dl.line([(470, 640), (570, 640)], fill=(80, 60, 40), width=4)
    elif kind == 'window':
        d.rectangle([0, 0, w, h], fill=255)
        dl.rectangle([360, 180, 680, 600], fill=(30, 34, 40))
        dl.ellipse([480, 300, 560, 400], fill=(110, 112, 110))
        dl.rectangle([470, 390, 570, 600], fill=(80, 82, 80))
        dl.line([(520, 180), (520, 600)], fill=(10, 10, 10), width=10)
        dl.line([(360, 390), (680, 390)], fill=(10, 10, 10), width=10)
    elif kind == 'painting':
        img[:] = np.array([0.06, 0.05, 0.04])
        d.rectangle([0, 0, w, h], fill=0)
        dl.rectangle([300, 120, 724, 660], fill=(90, 70, 40))
        dl.rectangle([330, 150, 694, 630], fill=(14, 14, 16))
        dl.ellipse([495, 380, 535, 420], fill=(255, 210, 140))
        dl.polygon([(485, 430), (545, 430), (565, 620), (465, 620)], fill=(6, 6, 7))
        dl.ellipse([490, 320, 540, 370], fill=(6, 6, 7))

    mask = np.array(layer.filter(ImageFilter.GaussianBlur(1.2))).astype(float)[..., None] / 255
    house_col = np.array([0.03, 0.03, 0.035])
    img = T.lerp(img, house_col * (0.85 + 0.3 * T.fbm(size, 32, 3)[:h, :w, None]), mask)
    lit = np.array(lights).astype(float) / 255
    lit_mask = (lit.sum(-1, keepdims=True) > 0.001).astype(float)
    img = T.lerp(img, lit, lit_mask)
    glow = np.array(lights.filter(ImageFilter.GaussianBlur(28))).astype(float) / 255
    img = img + glow * 0.9

    if kind in ('vance', 'marrow', 'rectory'):
        lx = {'vance': 90, 'marrow': 980, 'rectory': 60}[kind]
        yv, xv = np.mgrid[0:h, 0:w]
        r = np.sqrt((xv - lx) ** 2 + (yv - (hy - 260)) ** 2)
        img += np.exp(-r / 140)[..., None] * np.array([1.0, 0.55, 0.2]) * 0.55
        img[(np.abs(xv - lx) < 4) & (yv > hy - 260) & (yv < hy + 60)] = 0.02

    # Film: fog, grade, vignette, grain, leak.
    fog = np.exp(-((yy - horizon) / 0.08) ** 2)[..., None] * np.array([0.10, 0.11, 0.12])
    img = img + fog * 0.5
    img = img * np.array([0.95, 1.0, 1.05]) + np.array([0.035, 0.04, 0.045])
    yv, xv = np.mgrid[0:h, 0:w]
    vig = 1 - 0.75 * (((xv / w - 0.5) * 1.1) ** 2 + ((yv / h - 0.5) * 1.2) ** 2) * 1.6
    img = img * np.clip(vig, 0.2, 1)[..., None]
    img = img + rnd.normal(0, 0.035, (h, w, 1)) + rnd.normal(0, 0.012, (h, w, 3))
    leak = np.clip(1 - xv / (w * 0.35), 0, 1) ** 3 * (0.5 + 0.5 * np.sin(yv / h * 3))
    img = img + leak[..., None] * np.array([0.35, 0.12, 0.03]) * (0.5 if seed % 2 else 0.0)
    out_ = Image.fromarray((np.clip(img, 0, 1) ** (1 / 1.15) * 255).astype(np.uint8))
    return out_.filter(ImageFilter.GaussianBlur(0.7))


MISSION_PHOTOS = [('vance', 'T_Mission_Vance', '14 Ashgrove'), ('marrow', 'T_Mission_Marrow', 'Marrow St.'),
                  ('rectory', 'T_Mission_Rectory', 'St. Agnes'), ('whitlock', 'T_Mission_Whitlock', 'Whitlock')]
EXTRA_PHOTOS = [('van', 'the van'), ('hallway', 'basement??'), ('window', 'who is that'), ('painting', 'the Lantern Bearer')]


def mission_photos():
    for i, (kind, name, _) in enumerate(MISSION_PHOTOS):
        night_photo(kind, seed=i).save(os.path.join(OUT, name + '.jpg'), quality=90)


def polaroids():
    atlas = Image.new('RGB', (2048, 1024), (0, 0, 0))
    entries = [(k, cap) for k, _, cap in MISSION_PHOTOS] + EXTRA_PHOTOS
    for i, (kind, caption) in enumerate(entries):
        tile = aged_paper(512, 512, (0.90, 0.89, 0.85), seed=200 + i, stains=0.12)
        photo = night_photo(kind, seed=10 + i).crop((92, 0, 932, 768)).resize((440, 400), Image.LANCZOS)
        tile.paste(photo, (36, 30))
        d = ImageDraw.Draw(tile)
        d.text((256, 448), caption, font=SCRAWL(52), fill=(30, 30, 40), anchor='mm')
        atlas.paste(tile, ((i % 4) * 512, (i // 4) * 512))
    atlas.save(os.path.join(OUT, 'T_Polaroids.jpg'), quality=90)


if __name__ == '__main__':
    os.chdir(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
    for folder in (OUT, DECALS, UI):
        os.makedirs(folder, exist_ok=True)
    only = set(sys.argv[1:])
    for fn in (decals, town_map, notes_atlas, missing_poster, museum_poster, calendar, newspaper, signs,
               screens_and_rain, ui_images, icons, chainlink, clockface, mission_photos, polaroids):
        if only and fn.__name__ not in only:
            continue
        print('->', fn.__name__, flush=True)
        fn()
