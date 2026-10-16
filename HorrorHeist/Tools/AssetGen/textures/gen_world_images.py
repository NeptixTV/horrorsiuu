"""
One-off images for the town and the houses: oil paintings (one of them is lore), an old family
photo, shop / street / traffic signs rendered with the project fonts, brass house numbers, a dim
shop interior seen through a window, and town decals (leaves, puddle, mud). All procedural.

Run from the HorrorHeist folder:  python Tools/AssetGen/textures/gen_world_images.py [names...]
Writes SourceArt/Textures/Unique/*.jpg|png and SourceArt/Textures/Decals/*.png
"""
import math
import os
import sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageFile

sys.path.insert(0, os.path.dirname(__file__))
import texlib as T  # noqa: E402

OUT = 'SourceArt/Textures/Unique'
DECALS = 'SourceArt/Textures/Decals'
FONTS = 'Content/Slate/Fonts'
ImageFile.MAXBLOCK = 1 << 24

OSWALD = 'Oswald-SemiBold.ttf'
COND = 'BarlowCondensed-SemiBold.ttf'
TYPE = 'SpecialElite-Regular.ttf'
BODY = 'Barlow-Medium.ttf'


def font(name, size):
    return ImageFont.truetype(os.path.join(FONTS, name), int(size))


def fit_font(name, text, max_w, max_h, start=400, tracking=0):
    """Largest font size (<= start) whose rendered text fits in max_w x max_h."""
    size = start
    while size > 8:
        f = font(name, size)
        x0, y0, x1, y1 = f.getbbox(text)
        w = x1 - x0 + tracking * (len(text) - 1)
        if w <= max_w and (y1 - y0) <= max_h:
            return f
        size -= 2
    return font(name, 8)


def text_centered(draw, cx, cy, text, fnt, fill, tracking=0):
    """Draw text centred on (cx, cy) by its ink bbox; optional letter tracking in pixels."""
    if tracking == 0:
        x0, y0, x1, y1 = fnt.getbbox(text)
        draw.text((cx - (x0 + x1) / 2, cy - (y0 + y1) / 2), text, font=fnt, fill=fill)
        return
    widths = [fnt.getlength(c) for c in text]
    total = sum(widths) + tracking * (len(text) - 1)
    x0, y0, x1, y1 = fnt.getbbox(text)
    x = cx - total / 2
    for c, w in zip(text, widths):
        draw.text((x, cy - (y0 + y1) / 2), c, font=fnt, fill=fill)
        x += w + tracking


def noise(w, h, period, octaves=5, seed_value=None):
    """texlib fbm cropped to a w x h image (texlib noise is square)."""
    if seed_value is not None:
        T.seed(seed_value)
    size = max(w, h)
    return T.fbm(size, period, octaves)[:h, :w]


def to_f(img):
    return np.asarray(img.convert('RGB')).astype(np.float64) / 255.0


def to_img(a):
    return Image.fromarray(np.clip(np.round(a * 255), 0, 255).astype(np.uint8), 'RGB')


def save_jpg(a, name, quality=90):
    os.makedirs(OUT, exist_ok=True)
    to_img(a).save(os.path.join(OUT, name + '.jpg'), quality=quality, subsampling=0, optimize=True)


def edge_distance(w, h):
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float64)
    return np.minimum(np.minimum(xx, w - 1 - xx), np.minimum(yy, h - 1 - yy)), yy, xx


def run_down(mask, length, decay=0.97):
    """Streaks running down the image from a mask (rust or grime washed by rain)."""
    out = mask.copy()
    cur = mask.copy()
    for _ in range(length):
        cur = np.roll(cur, 1, axis=0) * decay
        cur[0] = 0
        out = np.maximum(out, cur)
    return out


def age_metal(a, seed_value, rust=0.5, dirt=0.5, fade=0.25, chips=0.4, holes=(), streaks=0.5, border_px=40):
    """Weather a flat painted-metal sign image (HxWx3 floats): faded/chalky paint, grime that
    gathers at the bottom, rust creeping in from the edges and bolt holes, chips, scratches."""
    h, w = a.shape[:2]
    rnd = np.random.default_rng(seed_value)
    d, yy, xx = edge_distance(w, h)
    big = noise(w, h, 4, 5, seed_value)
    mid = noise(w, h, 16, 4)
    fine = noise(w, h, 96, 3)
    # Fading: lighter, less saturated, uneven.
    grey = a.mean(-1, keepdims=True)
    f = fade * (0.6 + 0.8 * big)[..., None]
    a = T.lerp(a, grey * 1.05 + 0.04, f)
    # Rust from the edges and around the bolt holes.
    hole_d = np.full((h, w), 1e9)
    for (hx, hy) in holes:
        hole_d = np.minimum(hole_d, np.sqrt((xx - hx) ** 2 + (yy - hy) ** 2))
    near = np.maximum(1 - T.smoothstep(0, border_px, d), 1 - T.smoothstep(4, 34, hole_d))
    rmask = T.smoothstep(0.55, 0.65, mid * 0.6 + fine * 0.25 + near * 0.55 * rust + big * 0.2 - 0.18) * min(1, rust * 1.6)
    rmask = rmask * T.smoothstep(0.05, 0.5, near + 0.12 * rust)
    chip = T.smoothstep(0.74, 0.78, noise(w, h, 48, 4) * 0.7 + mid * 0.3) * chips
    rcol = T.lerp(T.color([0.36, 0.17, 0.07]), T.color([0.20, 0.09, 0.04]), fine)
    a = T.lerp(a, T.color([0.42, 0.41, 0.40]) * (0.8 + 0.3 * fine[..., None]), chip * (1 - rmask))
    a = T.lerp(a, rcol, np.clip(rmask + chip * 0.4, 0, 1))
    run = run_down(np.clip(rmask * 0.8 + chip * 0.3, 0, 1), int(h * 0.35), 0.985) * streaks
    run = run * (0.4 + 0.6 * noise(w, h, 64, 2))
    a = T.lerp(a, a * T.color([0.62, 0.42, 0.28]), np.clip(run - rmask, 0, 1) * 0.6)
    # Grime: general, heavier towards the bottom and the edges; rain-washed vertical streaks.
    g = dirt * (0.35 * big + 0.35 * (yy / h) ** 2 + 0.3 * (1 - T.smoothstep(0, border_px * 1.5, d)))
    cols = np.random.default_rng(seed_value + 1).random(w)
    cols = T.gblur(np.tile(cols, (8, 1)), 0.1, 3)[0]
    vstreak = np.clip((cols - 0.55) * 2.5, 0, 1)[None, :] * (1 - yy / h) * dirt * 0.5
    a = a * (1 - 0.55 * np.clip(g + vstreak * 0.6, 0, 1))[..., None]
    a = a * T.color([1.0, 0.97, 0.9]) ** (dirt * 1.5)
    # Fine scratches.
    sc = Image.new('L', (w, h), 0)
    sd = ImageDraw.Draw(sc)
    for _ in range(int(w * h / 9000)):
        x, y = rnd.uniform(0, w), rnd.uniform(0, h)
        ang = rnd.uniform(0, math.pi)
        ln = rnd.uniform(10, 80)
        sd.line([(x, y), (x + math.cos(ang) * ln, y + math.sin(ang) * ln)], fill=int(rnd.uniform(60, 200)), width=1)
    sc = np.asarray(sc).astype(np.float64) / 255.0
    a = T.lerp(a, np.minimum(a * 1.25 + 0.05, 1), sc[..., None] * 0.35)
    return np.clip(a, 0, 1)


def bolt(draw, x, y, r, fill=(70, 66, 60), rim=(30, 28, 26)):
    draw.ellipse([x - r, y - r, x + r, y + r], fill=fill, outline=rim, width=max(1, int(r * 0.25)))
    draw.line([x - r * 0.6, y, x + r * 0.6, y], fill=rim, width=max(1, int(r * 0.2)))


# =======================================================================================
# Shop signs (2048 x 512)

def _shop_sign(name, bg, fg, title, sub, title_font, sub_font, seed_value, accent=None, rust=0.5, dirt=0.5,
               fade=0.25, title_tracking=0, sub_tracking=6, frame=True, title_h=0.5):
    w, h = 2048, 512
    img = Image.new('RGB', (w, h), tuple(int(c * 255) for c in bg))
    d = ImageDraw.Draw(img)
    fg8 = tuple(int(c * 255) for c in fg)
    ac8 = tuple(int(c * 255) for c in (accent or fg))
    if frame:
        d.rounded_rectangle([26, 26, w - 27, h - 27], radius=18, outline=ac8, width=9)
        d.rounded_rectangle([46, 46, w - 47, h - 47], radius=10, outline=ac8, width=3)
    ft = fit_font(title_font, title, w - 260, h * title_h, 360, title_tracking)
    text_centered(d, w / 2, h * 0.40, title, ft, fg8, title_tracking)
    fs = fit_font(sub_font, sub, w - 700, h * 0.15, 120, sub_tracking)
    text_centered(d, w / 2, h * 0.78, sub, fs, ac8, sub_tracking)
    holes = [(70, 70), (w - 70, 70), (70, h - 70), (w - 70, h - 70)]
    for (x, y) in holes:
        bolt(d, x, y, 13)
    img = img.filter(ImageFilter.GaussianBlur(0.7))
    a = age_metal(to_f(img), seed_value, rust=rust, dirt=dirt, fade=fade, holes=holes)
    save_jpg(a, name)


def sign_brannigan():
    _shop_sign('T_Sign_Brannigan', (0.10, 0.22, 0.16), (0.86, 0.81, 0.66), 'BRANNIGAN & SONS',
               'AUTO REPAIR  ·  EST. 1961', OSWALD, COND, 601, accent=(0.80, 0.70, 0.46), rust=0.6, title_tracking=8)


def sign_hardware():
    _shop_sign('T_Sign_Hardware', (0.36, 0.08, 0.06), (0.88, 0.84, 0.72), "COBB'S HARDWARE",
               'LOCKS  ·  KEYS  ·  TOOLS', OSWALD, COND, 602, accent=(0.86, 0.74, 0.42), rust=0.45, dirt=0.55,
               title_tracking=4)


def sign_clothing():
    _shop_sign('T_Sign_Clothing', (0.80, 0.77, 0.68), (0.12, 0.16, 0.28), 'THREADS & CO.',
               'CLOTHING  ·  ALTERATIONS', TYPE, COND, 604, accent=(0.55, 0.42, 0.20), rust=0.3, dirt=0.6,
               fade=0.15, title_tracking=10, title_h=0.48)


def sign_diner():
    w, h = 2048, 512
    img = Image.new('RGB', (w, h), (214, 204, 182))
    d = ImageDraw.Draw(img)
    red = (150, 30, 26)
    d.rounded_rectangle([20, 20, w - 21, h - 21], radius=60, outline=(90, 92, 94), width=16)
    d.rounded_rectangle([44, 44, w - 45, h - 45], radius=44, outline=red, width=6)
    ft = fit_font(OSWALD, 'MAPLE DINER', w - 760, h * 0.5, 360, 14)
    text_centered(d, w / 2, h * 0.38, 'MAPLE DINER', ft, red, 14)
    band = [w * 0.33, h * 0.64, w * 0.67, h * 0.86]
    d.rounded_rectangle(band, radius=18, fill=(26, 24, 22))
    fs = fit_font(COND, 'OPEN LATE', band[2] - band[0] - 80, (band[3] - band[1]) * 0.7, 120, 12)
    text_centered(d, w / 2, (band[1] + band[3]) / 2, 'OPEN LATE', fs, (226, 196, 120), 12)
    # A maple leaf either side of the name.
    for cx in (w * 0.095, w * 0.905):
        pts = []
        for k in range(40):
            t = k / 40 * 2 * math.pi
            r = 70 * (0.55 + 0.45 * abs(math.cos(2.5 * t)) ** 0.8) * (1 - 0.08 * math.cos(16 * t))
            pts.append((cx + math.sin(t) * r, h * 0.4 - math.cos(t) * r))
        d.polygon(pts, fill=red)
        d.line([(cx, h * 0.4), (cx, h * 0.4 + 95)], fill=red, width=8)
    holes = [(80, h / 2), (w - 80, h / 2)]
    img = img.filter(ImageFilter.GaussianBlur(0.7))
    a = age_metal(to_f(img), 605, rust=0.45, dirt=0.7, fade=0.2, holes=holes)
    save_jpg(a, 'T_Sign_Diner')


def sign_laundromat():
    """Marlowe's Laundromat (the crew's hideout is in its basement): faded painted board, teal
    ground, cream script-like lettering with a red neon-tube-style outline painted around it."""
    w, h = 2048, 512
    img = Image.new('RGB', (w, h), (36, 84, 84))
    d = ImageDraw.Draw(img)
    cream = (232, 222, 196)
    red = (186, 52, 44)
    d.rounded_rectangle([22, 22, w - 23, h - 23], radius=26, outline=cream, width=10)
    d.rounded_rectangle([44, 44, w - 45, h - 45], radius=16, outline=red, width=4)
    title = "MARLOWE'S LAUNDROMAT"
    ft = fit_font(OSWALD, title, w - 300, h * 0.46, 330, 10)
    # Painted "neon" glow outline: red halo behind the cream letters.
    glow = Image.new('L', (w, h), 0)
    text_centered(ImageDraw.Draw(glow), w / 2, h * 0.40, title, ft, 255, 10)
    halo = glow.filter(ImageFilter.MaxFilter(9)).filter(ImageFilter.GaussianBlur(3))
    img = Image.composite(Image.new('RGB', (w, h), red), img, halo.point(lambda v: int(v * 0.85)))
    d = ImageDraw.Draw(img)
    text_centered(d, w / 2, h * 0.40, title, ft, cream, 10)
    sub = 'COIN-OP  ·  OPEN 6 AM - 10 PM'
    band = [w * 0.22, h * 0.66, w * 0.78, h * 0.87]
    fs = fit_font(COND, sub, band[2] - band[0] - 90, (band[3] - band[1]) * 0.7, 110, 8)
    d.rounded_rectangle(band, radius=14, fill=(22, 50, 52), outline=cream, width=3)
    text_centered(d, w / 2, (band[1] + band[3]) / 2, sub, fs, cream, 8)
    for cx in (w * 0.11, w * 0.89):                 # little painted bubbles
        for (ox, oy, r) in ((0, 0, 30), (-38, 30, 16), (30, 38, 12), (-12, -44, 10)):
            d.ellipse([cx + ox - r, h * 0.76 + oy - r, cx + ox + r, h * 0.76 + oy + r], outline=cream, width=4)
    holes = [(70, 70), (w - 70, 70), (70, h - 70), (w - 70, h - 70)]
    for (x, y) in holes:
        bolt(d, x, y, 12)
    img = img.filter(ImageFilter.GaussianBlur(0.7))
    a = age_metal(to_f(img), 606, rust=0.35, dirt=0.6, fade=0.4, chips=0.5, holes=holes)
    # Wooden board under the paint: faint grain where the paint has worn thin.
    T.seed(607)
    grain = T.grain_field(2048, 160, 6, 3)[:h, :w]
    a = a * (0.94 + 0.08 * grain[..., None])
    save_jpg(a, 'T_Sign_Laundromat')


def sign_museum():
    """Engraved limestone plaque: incised letters with shadowed upper-left walls, lichen, soot."""
    w, h = 2048, 512
    T.seed(603)
    base = noise(w, h, 6, 6)
    fine = noise(w, h, 128, 3)
    pd, _, pid = T.cells(w, 420)
    pits = ((1 - T.smoothstep(0.05, 0.25, pd)) * (np.random.default_rng(6031).random(420 * 420)[pid] < 0.3))[:h, :w]
    stone = T.lerp(T.color([0.50, 0.48, 0.44]), T.color([0.66, 0.64, 0.59]), base * 0.7 + fine * 0.3)
    stone = stone * (1 - 0.18 * pits[..., None])
    mask = Image.new('L', (w, h), 0)
    d = ImageDraw.Draw(mask)
    ft = fit_font(OSWALD, 'HOLLOWMERE', w - 420, h * 0.42, 330, 40)
    text_centered(d, w / 2, h * 0.36, 'HOLLOWMERE', ft, 255, 40)
    fs = fit_font(COND, 'HISTORICAL MUSEUM', w - 760, h * 0.16, 140, 22)
    text_centered(d, w / 2, h * 0.76, 'HISTORICAL MUSEUM', fs, 255, 22)
    d.line([(w * 0.2, h * 0.6), (w * 0.8, h * 0.6)], fill=255, width=5)
    for x in (w * 0.17, w * 0.83):
        d.polygon([(x, h * 0.6 - 12), (x + 12, h * 0.6), (x, h * 0.6 + 12), (x - 12, h * 0.6)], fill=255)
    m = np.asarray(mask.filter(ImageFilter.GaussianBlur(1.0))).astype(np.float64) / 255.0
    # Incised V-cut: depth from distance inside the glyph; light from the upper left.
    depth = T.gblur(np.pad(m, 0), 3.0) * m
    gy, gx = np.gradient(depth)
    lightv = np.clip(0.45 - (gx + gy) * 30, 0, 1)            # light from the upper left: incised walls
    inside = T.lerp(T.color([0.14, 0.13, 0.12]), T.color([0.62, 0.60, 0.56]), lightv)
    a = T.lerp(stone, inside, m[..., None])
    # Border: a raised chamfered frame.
    dd, yy, xx = edge_distance(w, h)
    frame = T.smoothstep(18, 22, dd) * (1 - T.smoothstep(40, 44, dd))
    a = a * (1 - 0.25 * (1 - T.smoothstep(0, 18, dd)))[..., None]
    a = T.lerp(a, a * 0.78, (T.smoothstep(40, 44, dd) * (1 - T.smoothstep(44, 52, dd)))[..., None])
    a = T.lerp(a, a * 1.08, frame[..., None])
    lichen = T.smoothstep(0.64, 0.72, noise(w, h, 5, 5) + 0.2 * (1 - T.smoothstep(0, 120, dd)))
    a = T.lerp(a, T.color([0.60, 0.62, 0.52]) * (0.8 + 0.4 * fine[..., None]), lichen[..., None] * 0.6)
    soot = run_down(T.smoothstep(0.62, 0.75, noise(w, h, 40, 3)) * (1 - T.smoothstep(20, 60, dd)), int(h * 0.5), 0.985)
    soot = T.gblur(np.pad(soot, ((0, 0), (0, 0))), 2.0)
    a = a * (1 - 0.25 * soot * (0.5 + 0.5 * noise(w, h, 48, 2)))[..., None]
    a = a * (1 - 0.3 * (yy / h) ** 2 * noise(w, h, 8, 3))[..., None]
    save_jpg(np.clip(a, 0, 1), 'T_Sign_Museum')


# =======================================================================================
# Street and traffic signs

def sign_parking():
    w = h = 512
    img = Image.new('RGB', (w, h), (225, 225, 222))
    d = ImageDraw.Draw(img)
    blue = (22, 70, 140)
    d.rounded_rectangle([6, 6, w - 7, h - 7], radius=36, fill=blue)
    d.rounded_rectangle([22, 22, w - 23, h - 23], radius=26, outline=(236, 236, 232), width=10)
    fp = fit_font(BODY, 'P', w * 0.6, h * 0.56, 420)
    text_centered(d, w / 2, h * 0.40, 'P', fp, (240, 240, 236))
    fs = fit_font(COND, 'PUBLIC PARKING', w - 110, h * 0.12, 80, 3)
    text_centered(d, w / 2, h * 0.83, 'PUBLIC PARKING', fs, (240, 240, 236), 3)
    holes = [(w / 2, 44), (w / 2, h - 44)]
    for x, y in holes:
        bolt(d, x, y, 9, fill=(150, 150, 150), rim=(60, 60, 60))
    img = img.filter(ImageFilter.GaussianBlur(0.6))
    a = age_metal(to_f(img), 610, rust=0.25, dirt=0.4, fade=0.3, chips=0.25, holes=holes, border_px=24)
    save_jpg(a, 'T_Sign_Parking')


STREET_NAMES = ['MAIN ST', 'ASHGROVE LN', 'MARROW ST', 'CHURCH RD', 'ELM AVE', 'PINE ST', 'MILL RD', 'ORCHARD LN']


def sign_street_names():
    """Atlas of street-name blades: 8 rows of 1024 x 128, top to bottom in STREET_NAMES order."""
    w, h = 1024, 128 * len(STREET_NAMES)
    atlas = np.zeros((h, w, 3))
    names = STREET_NAMES
    for i, n in enumerate(names):
        img = Image.new('RGB', (w, 128), (40, 40, 40))
        d = ImageDraw.Draw(img)
        d.rounded_rectangle([0, 0, w - 1, 127], radius=14, fill=(14, 92, 56))
        d.rounded_rectangle([7, 7, w - 8, 120], radius=10, outline=(236, 238, 234), width=5)
        f = fit_font(COND, n, w - 120, 82, 110, 5)
        text_centered(d, w / 2, 64, n, f, (240, 242, 238), 5)
        for x in (40, w - 40):
            bolt(d, x, 64, 7, fill=(170, 170, 168), rim=(60, 60, 60))
        img = img.filter(ImageFilter.GaussianBlur(0.5))
        a = age_metal(to_f(img), 620 + i, rust=0.25, dirt=0.4, fade=0.25, chips=0.2,
                      holes=[(40, 64), (w - 40, 64)], border_px=14, streaks=0.3)
        atlas[i * 128:(i + 1) * 128] = a
    save_jpg(atlas, 'T_Sign_StreetNames')


def sign_stop():
    w = h = 512
    img = Image.new('RGB', (w, h), (52, 52, 54))
    d = ImageDraw.Draw(img)
    c = w / 2
    pts = [(c + math.cos(math.pi / 8 + k * math.pi / 4) * 0.5 / math.cos(math.pi / 8) * w,
            c + math.sin(math.pi / 8 + k * math.pi / 4) * 0.5 / math.cos(math.pi / 8) * h) for k in range(8)]
    d.polygon(pts, fill=(236, 236, 232))
    inner = [(c + (x - c) * 0.93, c + (y - c) * 0.93) for x, y in pts]
    d.polygon(inner, fill=(170, 22, 24))
    f = fit_font(COND, 'STOP', w * 0.74, h * 0.36, 260, 6)
    text_centered(d, c, c, 'STOP', f, (240, 240, 236), 6)
    holes = [(c, 40), (c, h - 40)]
    for x, y in holes:
        bolt(d, x, y, 9, fill=(160, 160, 158), rim=(60, 60, 60))
    img = img.filter(ImageFilter.GaussianBlur(0.6))
    a = age_metal(to_f(img), 630, rust=0.3, dirt=0.4, fade=0.35, chips=0.3, holes=holes, border_px=30)
    save_jpg(a, 'T_Sign_Stop')


def sign_speed():
    w, h = 512, 640
    img = Image.new('RGB', (w, h), (232, 232, 228))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([18, 18, w - 19, h - 19], radius=28, outline=(20, 20, 20), width=12)
    f1 = fit_font(COND, 'SPEED', w * 0.7, h * 0.15, 140, 4)
    text_centered(d, w / 2, h * 0.19, 'SPEED', f1, (18, 18, 18), 4)
    text_centered(d, w / 2, h * 0.37, 'LIMIT', f1, (18, 18, 18), 4)
    f2 = fit_font(COND, '25', w * 0.66, h * 0.38, 400, 6)
    text_centered(d, w / 2, h * 0.70, '25', f2, (18, 18, 18), 6)
    holes = [(w / 2, 46), (w / 2, h - 46)]
    for x, y in holes:
        bolt(d, x, y, 9, fill=(160, 160, 158), rim=(60, 60, 60))
    img = img.filter(ImageFilter.GaussianBlur(0.6))
    a = age_metal(to_f(img), 640, rust=0.3, dirt=0.5, fade=0.15, chips=0.3, holes=holes, border_px=26)
    save_jpg(a, 'T_Sign_Speed')


def house_numbers():
    """1024 x 256 RGBA: four 256 px cells with aged brass numerals (14, 9, 3, 27) on transparency."""
    w, h = 1024, 256
    ss = 2
    mask = Image.new('L', (w * ss, h * ss), 0)
    d = ImageDraw.Draw(mask)
    for i, n in enumerate(['14', '9', '3', '27']):
        f = fit_font(OSWALD, n, 226 * ss, 226 * ss, 600, 10 * ss)
        text_centered(d, (i * 256 + 128) * ss, 128 * ss, n, f, 255, 10 * ss)
    mask = mask.resize((w, h), Image.LANCZOS)
    m = np.asarray(mask).astype(np.float64) / 255.0
    T.seed(650)
    # Bevelled relief from the glyph's inner distance; light from the upper left.
    hgt = T.gblur(m, 2.5) * m
    gy, gx = np.gradient(hgt)
    shade = np.clip(0.55 - (gx + gy) * 9, 0, 1)
    tone = noise(w, h, 24, 4)
    brass = T.lerp(T.color([0.36, 0.26, 0.11]), T.color([0.80, 0.64, 0.34]), shade)
    brass = brass * (0.85 + 0.25 * tone[..., None])
    tarnish = T.smoothstep(0.5, 0.75, noise(w, h, 8, 4)) * 0.6
    brass = T.lerp(brass, brass * T.color([0.55, 0.48, 0.36]), tarnish[..., None])
    verd = T.smoothstep(0.66, 0.74, noise(w, h, 16, 4)) * (1 - T.smoothstep(0.05, 0.25, hgt / (hgt.max() + 1e-9)))
    brass = T.lerp(brass, T.color([0.30, 0.45, 0.38]), verd[..., None] * 0.7)
    yy, xx = np.mgrid[0:h, 0:w]
    for i in range(4):
        for sy in (60, 196):
            dd = np.sqrt((xx - (i * 256 + 128)) ** 2 + (yy - sy) ** 2)
            screw = (1 - T.smoothstep(4, 6, dd)) * m
            brass = T.lerp(brass, T.color([0.28, 0.27, 0.25]), screw[..., None])
    # Dilate colour under the transparent area so filtering does not pull in black fringes.
    wgt = T.gblur(m, 6) + 1e-6
    fill = np.stack([T.gblur(brass[..., c] * m, 6) for c in range(3)], -1) / wgt[..., None]
    rgb = T.lerp(fill, brass, m[..., None])
    alpha = np.clip((m - 0.5) * 1.6 + 0.5, 0, 1)
    os.makedirs(OUT, exist_ok=True)
    T.save_rgba(os.path.join(OUT, 'T_House_Numbers.png'), np.concatenate([rgb, alpha[..., None]], -1))


# =======================================================================================
# Paintings & photo

def blur_img(a, r):
    """Gaussian blur of a float image (reflect-padded, so nothing wraps across the borders)."""
    p = int(math.ceil(r * 3)) + 1
    pad = ((p, p), (p, p)) + ((0, 0),) * (a.ndim - 2)
    b = T.gblur(np.pad(a, pad, mode='reflect'), r)
    return b[p:-p, p:-p]


def gauss_blob(w, h, cx, cy, sx, sy, ang=0.0):
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float64)
    dx, dy = xx - cx, yy - cy
    ca, sa = math.cos(ang), math.sin(ang)
    u = (dx * ca + dy * sa) / sx
    v = (-dx * sa + dy * ca) / sy
    return np.exp(-(u * u + v * v) / 2)


def mask_from(draw_fn, w, h, ss=2, blur=0.0):
    m = Image.new('L', (w * ss, h * ss), 0)
    draw_fn(ImageDraw.Draw(m), ss)
    m = m.resize((w, h), Image.LANCZOS)
    if blur > 0:
        m = m.filter(ImageFilter.GaussianBlur(blur))
    return np.asarray(m).astype(np.float64) / 255.0


def shaded_ellipses(a, items, light=(-0.55, -0.45, 0.70), soft=1.5):
    """Composite lit ellipsoids (painter's order) onto `a`: items = (cx, cy, rx, ry, angle, dark_rgb,
    light_rgb). Each blob gets Lambert shading from its own ellipsoid normal, so overlapping
    fingers, puffs and heads separate naturally."""
    h, w = a.shape[:2]
    L = np.array(light, dtype=np.float64)
    L /= np.linalg.norm(L)
    for (cx, cy, rx, ry, ang, dk, lt) in items:
        r = int(max(rx, ry)) + 4
        x0, x1 = max(0, int(cx - r)), min(w, int(cx + r))
        y0, y1 = max(0, int(cy - r)), min(h, int(cy + r))
        if x1 <= x0 or y1 <= y0:
            continue
        yy, xx = np.mgrid[y0:y1, x0:x1].astype(np.float64)
        ca, sa = math.cos(ang), math.sin(ang)
        u = ((xx - cx) * ca + (yy - cy) * sa) / rx
        v = (-(xx - cx) * sa + (yy - cy) * ca) / ry
        q = u * u + v * v
        nz = np.sqrt(np.clip(1 - q, 0, 1))
        nx = u * ca - v * sa
        ny = u * sa + v * ca
        lam = np.clip(nx * L[0] + ny * L[1] + nz * L[2], 0, 1)
        col = T.lerp(T.color(dk), T.color(lt), lam ** 0.9)
        m = np.clip((1 - q) * max(rx, ry) / soft, 0, 1)
        a[y0:y1, x0:x1] = T.lerp(a[y0:y1, x0:x1], col, m[..., None])
    return a


def painterly(ref, seed_value, passes=((18, 13, 2.4), (9, 7, 2.2), (4, 3.5, 2.0)), detail=None, flow_bias=None,
              jitter=0.025):
    """Re-paint `ref` with opaque brush strokes, coarse to fine. Strokes follow the image's
    edges (perpendicular to the luminance gradient); flat areas use `flow_bias` (radians) or a
    slowly varying random direction. `detail` (0..1) restricts the finest pass."""
    h, w = ref.shape[:2]
    rnd = np.random.default_rng(seed_value)
    lum = blur_img(ref.mean(-1), 3)
    gy, gx = np.gradient(lum)
    mag = np.sqrt(gx * gx + gy * gy)
    flow = noise(w, h, 3, 3, seed_value) * 2 * np.pi
    canvas = to_img(blur_img(ref, 8))
    d = ImageDraw.Draw(canvas)
    for pi_, (width, spacing, lf) in enumerate(passes):
        ys, xs = np.mgrid[0:h:spacing, 0:w:spacing]
        ys = (ys + rnd.uniform(0, spacing, ys.shape)).astype(int).clip(0, h - 1).ravel()
        xs = (xs + rnd.uniform(0, spacing, xs.shape)).astype(int).clip(0, w - 1).ravel()
        order = rnd.permutation(len(xs))
        for i in order:
            x, y = xs[i], ys[i]
            if pi_ == len(passes) - 1 and detail is not None and detail[y, x] < rnd.random():
                continue
            if pi_ == 1 and detail is not None and detail[y, x] < 0.02 and rnd.random() < 0.35:
                continue
            c = ref[y, x] * (1 + rnd.normal(0, jitter)) + rnd.normal(0, jitter * 0.4, 3)
            if mag[y, x] > 0.004:
                ang = math.atan2(gy[y, x], gx[y, x]) + math.pi / 2
            elif flow_bias is not None:
                ang = flow_bias + rnd.normal(0, 0.25)
            else:
                ang = flow[y, x]
            ang += rnd.normal(0, 0.18)
            ln = width * lf * rnd.uniform(0.6, 1.4)
            dx, dy = math.cos(ang) * ln / 2, math.sin(ang) * ln / 2
            col = tuple(int(np.clip(v, 0, 1) * 255) for v in c)
            d.line([(x - dx, y - dy), (x + dx, y + dy)], fill=col, width=max(1, int(width * rnd.uniform(0.75, 1.1))))
    return to_f(canvas)


def craquelure(w, h, seed_value, strength=0.5):
    """Fine crack network of old varnish/paint: two scales of cell borders, slightly cupped."""
    T.seed(seed_value)
    size = max(w, h)
    f1, f2, ids = T.cells(size, 34, warp_y=(T.fbm(size, 8, 3) - 0.5) * 30, warp_x=(T.fbm(size, 8, 3) - 0.5) * 30)
    g1, g2, _ = T.cells(size, 90)
    big = 1 - T.smoothstep(0.0, 0.025, f2 - f1)
    small = (1 - T.smoothstep(0.0, 0.03, g2 - g1)) * 0.55
    cup = (np.random.default_rng(seed_value).random(34 * 34)[ids] - 0.5) * 0.05
    lines = np.clip(big + small, 0, 1)[:h, :w]
    return lines * strength, cup[:h, :w]


def age_painting(a, seed_value, varnish=0.7, crack=0.5, vignette=0.6, losses=0.3):
    h, w = a.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float64)
    # Darkened, yellowed varnish: warm multiply, lifted murky blacks, compressed highlights.
    vcol = T.lerp(T.color([1, 1, 1]), T.color([0.86, 0.74, 0.50]), varnish)
    a = a * vcol + T.color([0.025, 0.02, 0.008]) * varnish
    a = a / (1 + a * 0.35 * varnish)
    blotch = noise(w, h, 3, 4, seed_value + 1)
    a = a * (0.88 + 0.16 * blotch[..., None])
    lines, cup = craquelure(w, h, seed_value + 2, crack)
    a = a * (1 + cup[..., None]) * (1 - 0.55 * lines[..., None])
    # Canvas weave.
    weave = (np.sin(xx * np.pi / 2.2) * np.sin(yy * np.pi / 2.2)) * 0.5 + 0.5
    a = a * (0.97 + 0.04 * weave[..., None])
    # Paint losses (ground shows through) and dust specks.
    lossm = T.smoothstep(0.83, 0.86, noise(w, h, 40, 3, seed_value + 3)) * losses
    a = T.lerp(a, T.color([0.52, 0.46, 0.34]), lossm[..., None])
    rnd = np.random.default_rng(seed_value + 4)
    dust = (rnd.random((h, w)) > 0.99985).astype(float)
    dust = blur_img(dust, 0.8) * 4
    a = a + np.clip(dust, 0, 0.15)[..., None] * 0.4
    vig = 1 - vignette * np.clip(((xx / w - 0.5) * 1.6) ** 2 + ((yy / h - 0.5) * 1.5) ** 2, 0, 1) ** 1.2
    a = a * vig[..., None]
    return np.clip(a, 0, 1)


def painting_lore():
    """Victorian portrait: a pale woman in a black high-collared dress, hands folded; murky
    brown-green ground. Her face is indistinct - the eyes are only dark smudges."""
    w, h = 1024, 1280
    T.seed(701)
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float64)
    bgn = noise(w, h, 3, 6, 701)
    halo = np.exp(-(((xx - 440) / 420) ** 2 + ((yy - 420) / 520) ** 2))
    bg = T.lerp(T.color([0.07, 0.075, 0.05]), T.color([0.38, 0.35, 0.22]), np.clip(halo, 0, 1))
    bg = bg * (0.72 + 0.56 * bgn[..., None])
    bg = T.lerp(bg, bg * T.color([0.80, 1.0, 0.78]), noise(w, h, 5, 4)[..., None] * 0.5)
    cx = 512
    hy = 405
    frx, fry = 100, 132
    L = (-0.55, -0.45, 0.70)

    def dress_fn(d, s):
        P = lambda pts: [(x * s, y * s) for x, y in pts]  # noqa: E731
        d.polygon(P([(cx - 66, 590), (cx + 66, 590), (cx + 215, 660), (cx + 300, 760), (cx + 330, 1280),
                     (cx - 330, 1280), (cx - 300, 760), (cx - 215, 660)]), fill=255)

    dress = mask_from(dress_fn, w, h, blur=1.0)
    a = bg.copy()
    folds = (np.sin((xx - cx) / 34 + noise(w, h, 4, 3) * 4) * 0.5 + 0.5) * T.smoothstep(820, 960, yy)
    rim = np.exp(-((xx - (cx - 300 + (yy - 760) * 0.06)) / 22) ** 2) * T.smoothstep(700, 800, yy)
    dress_col = (T.color([0.045, 0.043, 0.048]) + (folds ** 5)[..., None] * T.color([0.09, 0.09, 0.10])
                 + rim[..., None] * T.color([0.07, 0.065, 0.06]))
    a = T.lerp(a, dress_col, dress[..., None])
    # Leg-of-mutton sleeve puffs and the arms (shaded ellipsoids of black silk).
    silk_dk, silk_lt = (0.02, 0.02, 0.024), (0.20, 0.20, 0.22)
    a = shaded_ellipses(a, [(cx - 222, 760, 108, 115, 0.2, silk_dk, silk_lt), (cx + 222, 760, 108, 115, -0.2, silk_dk, (0.13, 0.13, 0.15)),
                            (cx - 225, 960, 66, 170, 0.32, silk_dk, (0.10, 0.10, 0.11)), (cx + 228, 965, 66, 170, -0.32, silk_dk, (0.08, 0.08, 0.09))], L)
    # Hands folded at the waist: the left hand below, the right laid over it (fingers as ellipsoids).
    sk_dk, sk_lt = (0.36, 0.29, 0.24), (0.84, 0.78, 0.68)
    hands = [(cx - 30, 1108, 78, 44, -0.15, sk_dk, sk_lt)]                      # left hand (underneath)
    for k in range(4):                                                           # its fingers, to the right
        hands.append((cx + 72 + k * 5, 1100 + k * 17, 54, 13, 0.45, sk_dk, sk_lt))
    hands.append((cx + 40, 1122, 80, 46, 0.12, sk_dk, sk_lt))                    # right hand laid over it
    for k in range(4):                                                           # its fingers, draping left
        hands.append((cx - 62 - k * 5, 1112 + k * 17, 56, 13.5, -0.5, sk_dk, sk_lt))
    hands.append((cx + 2, 1092, 36, 12, -0.35, sk_dk, sk_lt))                    # thumb
    cuffs = [(cx - 118, 1104, 22, 46, 0.35, (0.26, 0.25, 0.23), (0.58, 0.55, 0.50)),
             (cx + 128, 1118, 22, 46, -0.35, (0.26, 0.25, 0.23), (0.52, 0.50, 0.45))]
    a = shaded_ellipses(a, cuffs + hands, L, soft=2.0)
    # High collar with a lace edge and a cameo brooch.
    collar = mask_from(lambda d, s: d.polygon([((cx - 56) * s, 492 * s), ((cx + 56) * s, 492 * s), ((cx + 70) * s, 612 * s),
                                               ((cx - 70) * s, 612 * s)], fill=255), w, h, blur=0.8)
    ccol = T.color([0.03, 0.03, 0.035]) + T.color([0.08, 0.08, 0.085]) * np.clip(1 - np.abs(xx - cx + 25) / 60, 0, 1)[..., None]
    a = T.lerp(a, ccol, collar[..., None])
    lace = np.exp(-((yy - 497) / 3.5) ** 2) * (np.abs(xx - cx) < 58) * (0.5 + 0.5 * np.sin(xx * 0.9) ** 2)
    a = T.lerp(a, T.color([0.55, 0.52, 0.47]), (lace * collar)[..., None] * 0.6)
    a = shaded_ellipses(a, [(cx, 600, 21, 27, 0, (0.30, 0.22, 0.10), (0.72, 0.58, 0.30)),
                            (cx, 600, 15, 20, 0, (0.45, 0.42, 0.38), (0.82, 0.77, 0.68))], L)
    # Hair: smooth, centre-parted, pulled back over the ears.
    hair_dk, hair_lt = (0.03, 0.022, 0.018), (0.20, 0.15, 0.11)
    a = shaded_ellipses(a, [(cx - frx + 6, hy + 8, 34, 62, 0.15, hair_dk, hair_lt), (cx + frx - 6, hy + 8, 34, 62, -0.15, hair_dk, (0.12, 0.09, 0.07)),
                            (cx, hy - 34, frx + 12, fry - 4, 0, hair_dk, hair_lt)], L)
    # Face (below a curved hairline): pale, smooth, with only smudges where the features are.
    fx = (xx - cx) / frx
    fy = (yy - hy) / fry
    fd = fx * fx + fy * fy
    hairline = hy - 78 + ((xx - cx) / 70) ** 2 * 48 + 7 * np.exp(-((xx - cx) / 6) ** 2)
    face = (1 - T.smoothstep(0.92, 1.0, fd)) * T.smoothstep(-3, 3, yy - hairline)
    face = blur_img(face, 1.2)
    Lv = np.array(L) / np.linalg.norm(L)
    nz = np.sqrt(np.clip(1 - fd, 0, 1))
    lam = np.clip(fx * Lv[0] + fy * Lv[1] + nz * Lv[2], 0, 1)
    skin = T.lerp(T.color([0.34, 0.28, 0.24]), T.color([0.88, 0.83, 0.74]), lam ** 0.75)
    eyes = np.maximum(gauss_blob(w, h, cx - 42, hy + 2, 23, 14, 0.08), gauss_blob(w, h, cx + 43, hy + 8, 24, 15, -0.1))
    eyes = np.clip(eyes * 1.3, 0, 1)
    brow = (gauss_blob(w, h, cx - 42, hy - 26, 30, 6) + gauss_blob(w, h, cx + 43, hy - 22, 30, 6)) * 0.25
    nose = gauss_blob(w, h, cx + 12, hy + 52, 6, 28) * 0.3 + gauss_blob(w, h, cx + 3, hy + 82, 15, 5) * 0.4
    mouth = gauss_blob(w, h, cx + 3, hy + 112, 26, 4.0) * 0.55
    chin = gauss_blob(w, h, cx + 30, hy + 150, 70, 16) * 0.35
    skin = skin * (1 - 0.94 * eyes)[..., None] * (1 - brow - nose - chin).clip(0, 1)[..., None]
    skin = T.lerp(skin, T.color([0.32, 0.15, 0.14]), mouth[..., None])
    skin = blur_img(skin, 2.2)
    a = T.lerp(a, skin, face[..., None])
    detail = np.clip(blur_img(np.clip(face + (np.abs(yy - 1110) < 80) * (np.abs(xx - cx) < 190) + (np.abs(yy - 560) < 60) * (np.abs(xx - cx) < 80), 0, 1), 12) * 2.5, 0, 1)
    painted = painterly(a, 702, detail=detail)
    painted = T.lerp(painted, blur_img(a, 1.2), (detail * 0.4)[..., None])
    out = age_painting(painted, 703, varnish=0.8, crack=0.6, vignette=0.6, losses=0.3)
    save_jpg(out, 'T_Painting_Lore')


def painting_landscape():
    """Dull 19th-century style landscape: fields, a tree line, one big tree, overcast sky."""
    w, h = 1024, 768
    T.seed(711)
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float64)
    hor = h * 0.56
    sky = T.lerp(T.color([0.40, 0.42, 0.43]), T.color([0.70, 0.69, 0.63]), np.clip(yy / hor, 0, 1) ** 1.5)
    clouds = noise(w, h, 3, 6, 711)
    clouds2 = noise(w, h, 6, 4)
    billow = T.smoothstep(0.45, 0.75, clouds * 0.7 + clouds2 * 0.3) * np.clip(1.2 - yy / hor, 0, 1)
    sky = sky * (0.86 + 0.25 * clouds[..., None]) + T.color([0.12, 0.12, 0.11]) * billow[..., None]
    sky = T.lerp(sky, sky * T.color([0.80, 0.82, 0.86]), T.smoothstep(0.5, 0.3, clouds)[..., None] * 0.5)
    a = sky
    hills_y = hor - 40 - 50 * noise(w, h, 2, 3)[0][None, :] - 20 * np.sin(xx / w * 5)
    a = T.lerp(a, T.color([0.42, 0.45, 0.45]), (yy > hills_y)[..., None] * 0.85)
    tree_y = hor - 10 - 45 * noise(w, h, 12, 4)[1][None, :]
    trees = (yy > tree_y).astype(float)
    tcol = T.color([0.15, 0.19, 0.11]) * (0.8 + 0.4 * noise(w, h, 24, 3)[..., None])
    a = T.lerp(a, tcol, trees[..., None])
    field_t = np.clip((yy - hor) / (h - hor), 0, 1)
    field = T.lerp(T.color([0.50, 0.47, 0.30]), T.color([0.30, 0.31, 0.17]), field_t[..., None] ** 0.8)
    furrow = (np.sin((xx - w * 0.6) / (8 + 60 * field_t) * 1.0 + field_t * 3) * 0.5 + 0.5) * field_t
    field = field * (0.9 + 0.12 * furrow[..., None]) * (0.85 + 0.3 * noise(w, h, 16, 3)[..., None])
    a = np.where((yy > hor)[..., None], field, a)
    # A path.
    pc = w * 0.62 + (h - yy) * 0.6 * np.sin((h - yy) / 160)
    pw = 6 + 60 * field_t
    path = (np.abs(xx - pc) < pw) * (yy > hor + 4)
    a = T.lerp(a, T.color([0.52, 0.45, 0.33]) * (0.85 + 0.3 * noise(w, h, 32, 2)[..., None]), path[..., None] * 0.85)
    # Big tree on the left: a noise-edged crown built from soft blobs, and a trunk.
    crown = np.zeros((h, w))
    rnd = np.random.default_rng(712)
    for _ in range(30):
        x = 205 + rnd.normal(0, 70)
        y = 320 + rnd.normal(0, 55)
        crown += gauss_blob(w, h, x, y, rnd.uniform(35, 60), rnd.uniform(30, 50))
    crown = crown + (noise(w, h, 24, 4) - 0.5) * 1.2
    cm = T.smoothstep(0.75, 0.95, crown)
    trunk = mask_from(lambda d, s: d.polygon([(188 * s, 640 * s), (228 * s, 640 * s), (218 * s, 380 * s), (200 * s, 380 * s)], fill=255), w, h, blur=1)
    a = T.lerp(a, T.color([0.10, 0.08, 0.06]), trunk[..., None])
    tshade = np.clip(0.3 + 0.9 * np.exp(-(((xx - 150) / 150) ** 2 + ((yy - 250) / 120) ** 2)), 0, 1)
    leaves = T.lerp(T.color([0.08, 0.09, 0.05]), T.color([0.32, 0.34, 0.19]), tshade) * (0.75 + 0.5 * noise(w, h, 48, 3)[..., None])
    a = T.lerp(a, leaves, cm[..., None])
    for (tx, ty, tr) in ((560, hor - 8, 18), (610, hor - 12, 24), (880, hor - 6, 16)):
        a = T.lerp(a, T.color([0.13, 0.16, 0.09]), np.clip(gauss_blob(w, h, tx, ty - tr, tr, tr * 1.3) * 2 - 0.6, 0, 1)[..., None])
    # A far farmhouse.
    a[int(hor) - 22:int(hor) - 4, 760:800] = T.color([0.70, 0.66, 0.58])
    for k in range(14):
        a[int(hor) - 36 + k, 756 + k:804 - k] = T.color([0.40, 0.20, 0.15])
    painted = painterly(a, 713, passes=((16, 12, 2.6), (8, 6, 2.4), (4, 4, 2.0)), flow_bias=0.0)
    out = age_painting(painted, 714, varnish=0.7, crack=0.4, vignette=0.55, losses=0.15)
    save_jpg(out, 'T_Painting_Landscape')


def photo_family():
    """Faded 1970s colour print: a family of four on the lawn in front of their house. Soft,
    warm, ordinary - except for a pale shape in the attic window and one shadow too many."""
    W, H = 768, 1024
    bx, by, bb = 34, 34, 74
    w, h = W - 2 * bx, H - by - bb
    T.seed(721)
    rnd = np.random.default_rng(721)
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float64)
    hor = h * 0.6
    L = (-0.6, -0.35, 0.72)
    sky = T.lerp(T.color([0.52, 0.64, 0.74]), T.color([0.82, 0.84, 0.80]), np.clip(yy / hor, 0, 1) ** 0.8)
    cl = noise(w, h, 3, 5, 721)
    sky = sky + T.color([0.10, 0.10, 0.09]) * T.smoothstep(0.5, 0.75, cl)[..., None]
    a = sky * np.ones((h, w, 1))
    lawn_t = np.clip((yy - hor) / (h - hor), 0, 1)
    lawn = T.lerp(T.color([0.46, 0.52, 0.28]), T.color([0.30, 0.40, 0.18]), lawn_t[..., None])
    lawn = lawn * (0.82 + 0.3 * noise(w, h, 48, 3)[..., None]) * (0.9 + 0.15 * noise(w, h, 6, 3)[..., None])
    a = np.where((yy > hor)[..., None], lawn, a)
    # Trees either side: soft blobs with lit tops.
    for (tx, ty, tr) in ((30, hor - 170, 130), (40, hor - 60, 90), (660, hor - 190, 150), (640, hor - 60, 100)):
        tb = np.clip(gauss_blob(w, h, tx, ty, tr, tr * 1.15) * 2.2 - 0.5 + (noise(w, h, 20, 4) - 0.5) * 0.9, 0, 1)
        lit = np.clip(0.35 + 0.8 * gauss_blob(w, h, tx - tr * 0.3, ty - tr * 0.4, tr * 0.8, tr * 0.7), 0, 1)
        tc = T.lerp(T.color([0.08, 0.13, 0.07]), T.color([0.36, 0.44, 0.20]), lit) * (0.75 + 0.5 * noise(w, h, 64, 3)[..., None])
        a = T.lerp(a, tc, tb[..., None])
    # The house: lit gable front, shaded side wing, roof, porch, windows with curtains.
    x0, x1, gy0 = 140, 500, hor - 240
    front = (xx > x0) & (xx < x1) & (yy > gy0) & (yy < hor + 12)
    gable = (yy <= gy0) & (yy > gy0 - 150 + np.abs(xx - (x0 + x1) / 2) * (150 / ((x1 - x0) / 2))) & (xx > x0) & (xx < x1)
    wing = (xx >= x1) & (xx < 640) & (yy > hor - 150) & (yy < hor + 12)
    boards = 0.93 + 0.07 * (np.sin(yy * np.pi / 5.5) * 0.5 + 0.5)
    a = np.where((front | gable)[..., None], T.color([0.86, 0.80, 0.62]) * boards[..., None], a)
    a = np.where(wing[..., None], T.color([0.62, 0.57, 0.45]) * boards[..., None], a)
    eave = (yy > gy0 - 160 + np.abs(xx - (x0 + x1) / 2) * (160 / ((x1 - x0) / 2 + 14))) & (yy < gy0 - 140 + np.abs(xx - (x0 + x1) / 2) * (150 / ((x1 - x0) / 2))) & (xx > x0 - 14) & (xx < x1 + 14)
    a = np.where(eave[..., None], T.color([0.28, 0.24, 0.21]), a)
    wing_roof = (xx >= x1) & (xx < 652) & (yy > hor - 178) & (yy <= hor - 150)
    a = np.where(wing_roof[..., None], T.color([0.30, 0.26, 0.23]) * (0.9 + 0.1 * np.sin(yy)[..., None]), a)
    a = a * (1 - 0.3 * ((yy > gy0) & (yy < gy0 + 14) & front))[..., None]
    a = a * (1 - 0.35 * ((yy > hor - 150) & (yy < hor - 138) & wing))[..., None]
    a = a * (1 - 0.25 * ((yy > hor - 6) & (front | wing)))[..., None]
    wins = [(180, gy0 + 40, 240, gy0 + 120), (400, gy0 + 40, 460, gy0 + 120), (296, gy0 - 95, 344, gy0 - 35),
            (180, hor - 100, 240, hor - 25), (535, hor - 115, 585, hor - 45)]
    for (wx0, wy0, wx1, wy1) in wins:
        win = (xx > wx0) & (xx < wx1) & (yy > wy0) & (yy < wy1)
        glass = T.color([0.18, 0.20, 0.22]) + T.color([0.25, 0.27, 0.28]) * np.clip((wx1 - xx) / (wx1 - wx0) - 0.4, 0, 1)[..., None]
        curt = (np.abs(xx - wx0 - 6) < 7) | (np.abs(xx - wx1 + 6) < 7)
        glass = np.where(curt[..., None], T.color([0.80, 0.74, 0.62]), glass)
        a = np.where(win[..., None], glass, a)
        trim = (np.abs(xx - (wx0 + wx1) / 2) < 1.5) | (np.abs(yy - (wy0 + wy1) / 2) < 1.5) | (np.abs(xx - wx0) < 3) | (np.abs(xx - wx1) < 3) | (np.abs(yy - wy0) < 3) | (np.abs(yy - wy1) < 3)
        a = np.where((trim & (xx > wx0 - 3) & (xx < wx1 + 3) & (yy > wy0 - 3) & (yy < wy1 + 3))[..., None], T.color([0.92, 0.90, 0.84]), a)
    ghost = gauss_blob(w, h, 320, gy0 - 62, 8, 12)
    a = T.lerp(a, T.color([0.66, 0.66, 0.62]), np.clip(ghost * 0.7, 0, 1)[..., None])
    porch = (xx > 270) & (xx < 380) & (yy > hor - 125) & (yy < hor + 12)
    a = np.where(porch[..., None], T.color([0.30, 0.26, 0.22]), a)
    door = (xx > 300) & (xx < 350) & (yy > hor - 105) & (yy < hor + 6)
    a = np.where(door[..., None], T.color([0.42, 0.20, 0.13]) * (1 - 0.3 * ((xx - 300) / 50))[..., None], a)
    a = np.where(((xx > 262) & (xx < 388) & (yy > hor - 132) & (yy < hor - 122))[..., None], T.color([0.90, 0.88, 0.82]), a)
    for k in range(3):
        step = (xx > 280 - k * 6) & (xx < 370 + k * 6) & (yy > hor + 6 + k * 8) & (yy < hor + 14 + k * 8)
        a = np.where(step[..., None], T.color([0.62, 0.60, 0.56]) * (0.9 + 0.05 * k), a)
    # The family (father, mother, son, daughter), lit from the left, and their shadows - plus one.
    feet = h * 0.9
    people = [(212, 340, (0.40, 0.30, 0.21), (0.56, 0.50, 0.40), 'm'),
              (300, 305, (0.80, 0.55, 0.24), (0.80, 0.55, 0.24), 'f'),
              (382, 222, (0.24, 0.46, 0.46), (0.17, 0.19, 0.32), 'm'),
              (448, 190, (0.72, 0.24, 0.22), (0.72, 0.24, 0.22), 'f')]
    shadows = np.zeros((h, w))
    for (px, ht) in [(p_[0], p_[1]) for p_ in people] + [(522, 196)]:
        shadows = np.maximum(shadows, gauss_blob(w, h, px + 46, feet + 4, ht * 0.3, 10, 0.1))
    a = a * (1 - 0.5 * np.clip(shadows * 1.3, 0, 1))[..., None]
    for (px, ht, top, bottom, kind) in people:
        u = ht / 100.0
        dk = lambda c, f=0.45: tuple(v * f for v in c)  # noqa: E731
        items = []
        if kind == 'm':
            items += [(px - 7 * u, feet - 22 * u, 6.5 * u, 23 * u, 0.0, dk(bottom), bottom),
                      (px + 7 * u, feet - 22 * u, 6.5 * u, 23 * u, 0.0, dk(bottom), bottom)]
        else:
            items += [(px - 5 * u, feet - 9 * u, 3.5 * u, 10 * u, 0.0, (0.40, 0.30, 0.24), (0.86, 0.68, 0.55)),
                      (px + 5 * u, feet - 9 * u, 3.5 * u, 10 * u, 0.0, (0.40, 0.30, 0.24), (0.86, 0.68, 0.55)),
                      (px, feet - 33 * u, 13.5 * u, 22 * u, 0.0, dk(bottom), bottom)]
        items += [(px - 16 * u, feet - 62 * u, 5 * u, 24 * u, 0.06, dk(top), top),
                  (px + 16 * u, feet - 62 * u, 5 * u, 24 * u, -0.06, dk(top), top),
                  (px, feet - 64 * u, 14 * u, 25 * u, 0.0, dk(top), top),
                  (px - 17 * u, feet - 40 * u, 4 * u, 5 * u, 0, (0.40, 0.30, 0.24), (0.86, 0.68, 0.55)),
                  (px + 17 * u, feet - 40 * u, 4 * u, 5 * u, 0, (0.40, 0.30, 0.24), (0.86, 0.68, 0.55)),
                  (px, feet - 92 * u, 7.2 * u, 8.6 * u, 0, (0.42, 0.30, 0.24), (0.90, 0.72, 0.58)),
                  (px - 0.5 * u, feet - 96.5 * u, 7.6 * u, 5.0 * u, 0, (0.10, 0.07, 0.05), (0.42, 0.30, 0.18))]
        a = shaded_ellipses(a, items, L, soft=1.2)
    # Lens softness, then film: faded dyes, warm cast, lifted blacks, grain, vignette, leak.
    a = blur_img(a, 2.6)
    grey = a.mean(-1, keepdims=True)
    a = T.lerp(a, grey, 0.22)
    a = a * T.color([1.08, 0.96, 0.78]) + T.color([0.10, 0.07, 0.04])
    a = 0.06 + a * 0.86
    vig = 1 - 0.35 * np.clip(((xx / w - 0.5) * 1.5) ** 2 + ((yy / h - 0.5) * 1.3) ** 2, 0, 1)
    a = a * vig[..., None]
    leak = np.clip(1 - xx / (w * 0.3), 0, 1) ** 2.5 * (0.6 + 0.4 * np.sin(yy / h * 4))
    a = a + leak[..., None] * T.color([0.30, 0.12, 0.02])
    a = a + rnd.normal(0, 0.022, (h, w, 1)) + rnd.normal(0, 0.008, (h, w, 3))
    a = blur_img(np.clip(a, 0, 1), 0.6)
    # Print with a slightly yellowed white border, handled edges and a crease.
    paper = to_f(Image.new('RGB', (W, H), (232, 228, 214)))
    pn = noise(W, H, 4, 5, 722)
    paper = paper * (0.93 + 0.07 * pn[..., None])
    paper[by:by + h, bx:bx + w] = a
    PY, PX = np.mgrid[0:H, 0:W].astype(np.float64)
    crease = np.exp(-((PX * 0.32 + PY - H * 0.62) / 1.6) ** 2)
    paper = paper * (1 - 0.18 * crease[..., None]) + crease[..., None] * 0.06
    stain = T.smoothstep(0.66, 0.72, noise(W, H, 3, 5)) * 0.25
    paper = T.lerp(paper, paper * T.color([0.85, 0.75, 0.55]), stain[..., None])
    ed = np.minimum(np.minimum(PX, W - 1 - PX), np.minimum(PY, H - 1 - PY))
    paper = paper * (0.82 + 0.18 * T.smoothstep(0, 10, ed))[..., None]
    save_jpg(np.clip(paper, 0, 1), 'T_Photo_Family')


PICTURES = [painting_lore, painting_landscape, photo_family]


# =======================================================================================
# Shop interior backdrop

def shop_interior():
    """What you see through a dusty shop window at night: shelves of goods, a counter and a
    cash register in the warm light of one pendant lamp, soft and dark."""
    w, h = 1024, 512
    rnd = np.random.default_rng(801)
    T.seed(801)
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float64)
    wall = T.color([0.16, 0.12, 0.085]) * (0.85 + 0.25 * noise(w, h, 6, 4)[..., None])
    wall = wall * (0.92 + 0.08 * (np.sin(xx / 22) > 0.9)[..., None])
    img = to_img(wall)
    d = ImageDraw.Draw(img)
    pal = [(150, 40, 32), (40, 70, 120), (180, 150, 60), (70, 100, 60), (200, 190, 170), (120, 80, 50),
           (90, 90, 95), (160, 110, 60), (60, 50, 45), (170, 60, 50)]
    shelves = [100, 185, 270, 355]
    for sy in shelves:
        x = 30
        while x < w - 40:
            if rnd.random() < 0.08:
                x += rnd.integers(20, 60)
                continue
            iw = int(rnd.integers(12, 46))
            ih = int(rnd.integers(22, 70))
            c = pal[rnd.integers(len(pal))]
            c = tuple(int(v * rnd.uniform(0.7, 1.1)) for v in c)
            if rnd.random() < 0.3:                      # cans / jars
                d.rounded_rectangle([x, sy - ih, x + iw, sy], radius=min(iw // 3, 8), fill=c)
                d.rectangle([x, sy - ih + 4, x + iw, sy - ih + 9], fill=tuple(min(255, v + 50) for v in c))
            else:                                       # boxes with a label band
                d.rectangle([x, sy - ih, x + iw, sy], fill=c)
                d.rectangle([x + 2, sy - ih * 0.6, x + iw - 2, sy - ih * 0.35], fill=tuple(min(255, v + 60) for v in c))
            x += iw + int(rnd.integers(1, 6))
        d.rectangle([0, sy, w, sy + 9], fill=(70, 48, 30))
        d.rectangle([0, sy + 9, w, sy + 13], fill=(30, 20, 14))
    for ux in range(0, w + 1, 256):
        d.rectangle([ux - 6, 40, ux + 6, 380], fill=(56, 38, 24))
    # Counter (foreground right) with a cash register, and a floor.
    d.rectangle([0, 380, w, h], fill=(40, 30, 24))
    d.polygon([(560, 330), (w, 330), (w, h), (540, h)], fill=(64, 40, 24))
    d.rectangle([548, 318, w, 334], fill=(120, 86, 56))
    d.rectangle([800, 250, 920, 320], fill=(36, 34, 32))
    d.polygon([(800, 250), (920, 250), (900, 222), (820, 222)], fill=(60, 56, 50))
    for k in range(4):
        for j in range(3):
            d.rectangle([815 + k * 24, 268 + j * 15, 830 + k * 24, 278 + j * 15], fill=(150, 140, 120))
    d.rectangle([600, 360, 1000, 366], fill=(90, 60, 38))
    # A rack of hanging coats (left foreground silhouettes).
    for k in range(6):
        x = 70 + k * 34
        d.polygon([(x, 250), (x + 28, 250), (x + 34, 420), (x - 6, 420)], fill=(30 + k * 6, 26 + k * 3, 30))
    d.line([(50, 248), (290, 248)], fill=(110, 100, 90), width=4)
    a = to_f(img)
    # Lighting: one warm pendant lamp and a cold spill from the street on the left.
    lamp = (420, 40)
    dist = np.sqrt((xx - lamp[0]) ** 2 + ((yy - lamp[1]) * 1.3) ** 2)
    glow = np.exp(-dist / 230)
    a = a * (0.2 + 1.5 * glow[..., None] * T.color([1.0, 0.78, 0.5]))
    a = a + np.exp(-dist / 25)[..., None] * T.color([1.0, 0.85, 0.6]) * 0.9
    a = a + np.exp(-(xx / 220)) [..., None] * T.color([0.02, 0.03, 0.05])
    a = blur_img(np.clip(a, 0, 1.5), 3.0)
    # Dusty glass: haze catching the lamp light, grime, dried rain spots, vignette.
    dust = noise(w, h, 4, 6) * 0.7 + noise(w, h, 64, 3) * 0.3
    a = a * (0.75 + 0.25 * dust[..., None]) + (dust * glow)[..., None] * T.color([0.10, 0.08, 0.05])
    spots = blur_img((rnd.random((h, w)) > 0.9985).astype(float), 1.5) * 8
    a = a + np.clip(spots, 0, 1)[..., None] * 0.04
    streak = run_down(T.smoothstep(0.7, 0.8, noise(w, h, 24, 3)) * (yy < 60), 300, 0.992)
    a = a * (1 - 0.25 * streak[..., None])
    vig = 1 - 0.6 * np.clip(((xx / w - 0.45) * 1.4) ** 2 + ((yy / h - 0.4) * 1.6) ** 2, 0, 1)
    a = a * vig[..., None]
    save_jpg(np.clip(a, 0, 1), 'T_Shop_Interior')


# =======================================================================================
# Decals (1024 x 1024 RGBA, alpha = coverage)

def _decal_fade(S, margin=48):
    d, _, _ = edge_distance(S, S)
    return T.smoothstep(0, margin, d)


def _save_decal(name, rgb, alpha):
    os.makedirs(DECALS, exist_ok=True)
    T.save_rgba(os.path.join(DECALS, name + '.png'), np.concatenate([np.clip(rgb, 0, 1), np.clip(alpha, 0, 1)[..., None]], -1))


def _leaf_outline(kind, L, rnd):
    """Leaf outline in local coords (u along the leaf from the stem, v across), length L."""
    pts = []
    if kind == 'maple':
        for k in range(90):
            t = k / 90 * 2 * math.pi
            r = L * 0.5 * (0.42 + 0.58 * abs(math.cos(2.5 * t)) ** 0.7) * (1 + 0.07 * math.sin(22 * t))
            if abs(math.cos(t / 2)) < 0.12:
                r *= 0.5
            pts.append((L * 0.5 + math.cos(t) * r, math.sin(t) * r))
    elif kind == 'oak':
        n = 40
        W = L * 0.32
        for k in range(n + 1):
            u = k / n
            wv = W * math.sin(math.pi * u) ** 0.7 * (0.72 + 0.28 * abs(math.sin(u * 4.5 * math.pi)))
            pts.append((u * L, wv))
        for k in range(n, -1, -1):
            u = k / n
            wv = W * math.sin(math.pi * u) ** 0.7 * (0.72 + 0.28 * abs(math.sin(u * 4.5 * math.pi + 0.6)))
            pts.append((u * L, -wv))
    else:
        n = 40
        W = L * rnd.uniform(0.26, 0.36)
        for k in range(n + 1):
            u = k / n
            pts.append((u * L, W * math.sin(math.pi * u) ** 0.7 * (1 - 0.3 * u) * (1 + 0.05 * math.sin(u * 50))))
        for k in range(n, -1, -1):
            u = k / n
            pts.append((u * L, -W * math.sin(math.pi * u) ** 0.7 * (1 - 0.3 * u) * (1 + 0.05 * math.sin(u * 50 + 1))))
    return pts


def decal_leaves():
    """Scattered wet autumn leaves (maple, oak, elm), sparse clusters, transparent background."""
    S = 1024
    rnd = np.random.default_rng(901)
    T.seed(901)
    rgb = np.zeros((S, S, 3))
    alpha = np.zeros((S, S))
    tex = noise(S, S, 64, 3)
    blot = noise(S, S, 12, 3)
    pal = [(0.36, 0.20, 0.09), (0.58, 0.29, 0.08), (0.66, 0.48, 0.14), (0.46, 0.14, 0.07), (0.24, 0.14, 0.07),
           (0.52, 0.38, 0.16)]
    clusters = [rnd.uniform(220, 804, 2) for _ in range(5)]
    for i in range(85):
        if rnd.random() < 0.75:
            c = clusters[rnd.integers(len(clusters))]
            cx, cy = c + rnd.normal(0, 85, 2)
        else:
            cx, cy = rnd.uniform(120, 904, 2)
        kind = rnd.choice(['maple', 'oak', 'elm'], p=[0.4, 0.25, 0.35])
        L = rnd.uniform(48, 100) if kind != 'elm' else rnd.uniform(40, 80)
        ang = rnd.uniform(0, 2 * math.pi)
        col = np.array(pal[rnd.integers(len(pal))]) * rnd.uniform(0.7, 1.0) * 0.82    # wet: darker
        pts = _leaf_outline(kind, L, rnd)
        ss = 2
        R_ = int(L * 1.2) + 6
        m = Image.new('L', (2 * R_ * ss, 2 * R_ * ss), 0)
        dm = ImageDraw.Draw(m)
        ca, sa = math.cos(ang), math.sin(ang)
        off = -L * 0.5
        P = [((R_ + (u + off) * ca - v * sa) * ss, (R_ + (u + off) * sa + v * ca) * ss) for u, v in pts]
        dm.polygon(P, fill=255)
        dm.line([((R_ + (off - L * 0.18) * ca) * ss, (R_ + (off - L * 0.18) * sa) * ss), ((R_ + off * ca) * ss, (R_ + off * sa) * ss)],
                fill=255, width=2 * ss)
        m = np.asarray(m.resize((2 * R_, 2 * R_), Image.LANCZOS)).astype(np.float64) / 255.0
        py, px = np.mgrid[0:2 * R_, 0:2 * R_].astype(np.float64)
        lu = (px - R_) * ca + (py - R_) * sa - off            # along the leaf from the stem
        lv = -(px - R_) * sa + (py - R_) * ca
        rib = np.exp(-(lv / 1.2) ** 2) * (lu > 0) * (lu < L * 0.95)
        if kind == 'maple':
            th = np.arctan2(lv, lu - L * 0.5)
            veins = np.exp(-(((np.abs(th) % (2 * math.pi / 5)) - 0.0) * np.hypot(lu - L * 0.5, lv) / 1.4) ** 2) * 0.6
        else:
            veins = (np.abs(np.sin((lu - np.abs(lv) * 1.3) / L * 26)) < 0.12) * 0.35 * (np.abs(lv) > 2)
        x0, y0 = int(cx - R_), int(cy - R_)
        x1, y1 = x0 + 2 * R_, y0 + 2 * R_
        if x0 < 0 or y0 < 0 or x1 > S or y1 > S:
            continue
        local = col * (0.82 + 0.3 * tex[y0:y1, x0:x1, None]) * (1 + 0.25 * (rib + veins)[..., None])
        spots = T.smoothstep(0.62, 0.7, blot[y0:y1, x0:x1]) * rnd.uniform(0, 1)
        local = T.lerp(local, local * 0.45, spots[..., None])
        edge = np.clip(blur_img(m, 2.0) * -1 + m, 0, 1) * 2.5
        local = local * (1 - 0.35 * edge[..., None])
        rgb[y0:y1, x0:x1] = T.lerp(rgb[y0:y1, x0:x1], local, m[..., None])
        alpha[y0:y1, x0:x1] = np.maximum(alpha[y0:y1, x0:x1], m * rnd.uniform(0.88, 1.0))
    # Spread leaf colours under the transparent area (no dark fringes when filtered).
    wsum = blur_img(alpha, 8) + 1e-6
    fill = np.stack([blur_img(rgb[..., c] * alpha, 8) for c in range(3)], -1) / wsum[..., None]
    fill = np.where((wsum < 1e-3)[..., None], T.color([0.35, 0.22, 0.10]), fill)
    out = T.lerp(fill, rgb, np.clip(alpha * 1.2, 0, 1)[..., None])
    _save_decal('T_Decal_Leaves', out, alpha * _decal_fade(S))


def decal_puddle():
    """Irregular rain puddle: soft edge, damp rim, sediment band; ~60 % of the square."""
    S = 1024
    T.seed(911)
    yy, xx = np.mgrid[0:S, 0:S].astype(np.float64) / S - 0.5
    r = np.sqrt(xx ** 2 + (yy * 1.1) ** 2)
    field = 1 - r / 0.5 + (T.fbm(S, 3, 5) - 0.5) * 0.55 + (T.fbm(S, 10, 4) - 0.5) * 0.12
    field = field * _decal_fade(S, 90) - (1 - _decal_fade(S, 90))
    thr = np.quantile(field, 0.40)
    water = T.smoothstep(thr - 0.015, thr + 0.05, field)
    damp = T.smoothstep(thr - 0.09, thr, field) * (1 - water)
    deep = T.smoothstep(thr + 0.05, thr + 0.35, field)
    band = T.smoothstep(thr - 0.01, thr + 0.03, field) * (1 - T.smoothstep(thr + 0.03, thr + 0.09, field))
    n = T.fbm(S, 24, 3)
    rgb = T.lerp(T.color([0.20, 0.17, 0.14]), T.color([0.11, 0.10, 0.09]), deep[..., None])
    rgb = T.lerp(rgb, T.color([0.30, 0.24, 0.17]), band[..., None] * 0.6) * (0.9 + 0.2 * n[..., None])
    alpha = np.maximum(water * (0.86 + 0.08 * n), damp * 0.4)
    _save_decal('T_Decal_Puddle', rgb, alpha * _decal_fade(S, 24))


def decal_mud():
    """Mud patch crossed by a tyre track (tread smeared out of the patch) and boot prints."""
    S = 1024
    T.seed(921)
    Y, X = np.mgrid[0:S, 0:S].astype(np.float64)
    yy, xx = Y / S - 0.5, X / S - 0.5
    r = np.sqrt((xx * 1.2) ** 2 + yy ** 2)
    field = 1 - r / 0.42 + (T.fbm(S, 4, 5) - 0.5) * 0.7
    thr = np.quantile(field, 0.62)
    patch = T.smoothstep(thr - 0.02, thr + 0.06, field)
    splat = (1 - T.smoothstep(0.05, 0.35, T.cells(S, 60)[0])) * T.smoothstep(0.5, 0.7, T.fbm(S, 6, 3)) * T.smoothstep(thr - 0.35, thr - 0.05, field)
    # Tyre track: a gentle arc across the decal; staggered tread blocks; mud carried out of the
    # patch to the right, thinning out, with ragged edges.
    centre = S * 0.55 + (X - S / 2) ** 2 / 9000 + (T.fbm(S, 8, 2) - 0.5) * 6
    t_across = Y - centre
    tw = 48 + (T.fbm(S, 16, 2) - 0.5) * 8
    band = 1 - T.smoothstep(tw - 4, tw + 3, np.abs(t_across))
    lane = np.floor((t_across + 48) / 24)
    tread = ((X + lane * 9 + np.abs(t_across) * 0.5) % 30) < 13
    tread = blur_img(tread.astype(float), 1.0)
    carry = np.clip(1 - (X - S * 0.6) / (S * 0.34), 0, 1) ** 1.5 * (X > S * 0.5)
    carry = carry * T.smoothstep(0.35, 0.6, T.fbm(S, 24, 3) + carry * 0.3)
    track_alpha = band * np.clip(patch * 0.3 + carry, 0, 1)
    # Boot prints: forefoot + heel, soft lugs.
    lug_d = T.cells(S, 110)[0]
    lug = 1 - T.smoothstep(0.25, 0.45, lug_d)
    boots = np.zeros((S, S))
    for (bx, by, ang, k) in ((250, 290, -0.4, 0.9), (325, 200, -0.3, 0.6), (430, 770, 0.3, 0.8), (520, 840, 0.25, 0.5),
                             (190, 650, -1.2, 0.7)):
        ca, sa = math.cos(ang), math.sin(ang)
        u = ((X - bx) * ca + (Y - by) * sa)
        v = (-(X - bx) * sa + (Y - by) * ca)
        fore = (u / 28) ** 2 + ((v + 22) / 44) ** 2
        heel = (u / 23) ** 2 + ((v - 52) / 22) ** 2
        sole = np.maximum(1 - T.smoothstep(0.85, 1.0, fore), 1 - T.smoothstep(0.85, 1.0, heel))
        boots = np.maximum(boots, sole * k)
    lugs = lug * (boots > 0.3)
    n = T.fbm(S, 32, 3)
    wet = T.smoothstep(thr + 0.05, thr + 0.4, field)
    rgb = T.lerp(T.color([0.34, 0.26, 0.18]), T.color([0.17, 0.12, 0.08]), wet[..., None]) * (0.85 + 0.3 * n[..., None])
    rgb = T.lerp(rgb, rgb * 0.72, (band * tread * (0.4 + 0.6 * patch))[..., None] * 0.6)
    rgb = T.lerp(rgb, rgb * 1.15, (band * (1 - tread) * patch)[..., None] * 0.3)
    rgb = T.lerp(rgb, rgb * 0.7, (boots * (0.5 + 0.5 * lugs))[..., None] * 0.7)
    alpha = np.clip(np.maximum.reduce([patch * (0.85 + 0.1 * n), splat * 0.8, track_alpha * (0.45 + 0.4 * tread),
                                       boots * (0.45 + 0.4 * lugs)]), 0, 1)
    _save_decal('T_Decal_Mud', rgb, alpha * _decal_fade(S, 30))


SCENES = [shop_interior]
DECAL_FNS = [decal_leaves, decal_puddle, decal_mud]


SIGNS = [sign_brannigan, sign_laundromat, sign_hardware, sign_museum, sign_clothing, sign_diner, sign_parking, sign_street_names,
         sign_stop, sign_speed, house_numbers]
ALL = PICTURES + SIGNS + SCENES + DECAL_FNS

if __name__ == '__main__':
    os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..'))
    for folder in (OUT, DECALS):
        os.makedirs(folder, exist_ok=True)
    only = set(sys.argv[1:])
    unknown = only - {fn.__name__ for fn in ALL}
    if unknown:
        sys.exit('unknown image(s): ' + ', '.join(sorted(unknown)) + '\navailable: ' + ', '.join(f.__name__ for f in ALL))
    for fn in ALL:
        if only and fn.__name__ not in only:
            continue
        print('->', fn.__name__, flush=True)
        fn()
