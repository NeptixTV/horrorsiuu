"""Own procedural, tileable textures (no third-party content): base colour + normal map as PIL images."""
import numpy as np
from PIL import Image
from textures import fbm, normal_from_height, to_image


def _rgb(c):
    return np.array(c, dtype=np.float32)[None, None, :]


def _finish(color, height, strength):
    return to_image(color), Image.fromarray(normal_from_height(height.astype(np.float32), strength), 'RGB')


def _stretch(size, seed, sx, sy, octaves=5):
    """Noise stretched along one axis (wood grain): sample a tileable field with repeats."""
    n = fbm(size, octaves, seed, 0.6, 4)
    ys, xs = np.mgrid[0:size, 0:size]
    return n[(ys * sy) % size, (xs * sx) % size]


def plaster(size=512, seed=11, tint=(168, 160, 140)):
    """Old painted plaster with stains and fine cracks."""
    a, b, c = fbm(size, 6, seed, 0.55, 3), fbm(size, 5, seed + 1, 0.6, 8), fbm(size, 4, seed + 2, 0.5, 2)
    stain = np.clip((c - 0.45) * 3.0, 0, 1)
    crack = np.clip(1.0 - np.abs(fbm(size, 5, seed + 3, 0.5, 6) - 0.5) * 40.0, 0, 1) * np.clip((a - 0.5) * 6, 0, 1)
    col = _rgb(tint) * (0.78 + 0.3 * a[..., None]) * (1.0 - 0.38 * stain[..., None]) * (1.0 - 0.55 * crack[..., None])
    col = col + _rgb((14, 18, 6)) * stain[..., None] * b[..., None]
    h = 0.5 + 0.12 * b - 0.5 * crack
    return _finish(col, h, 1.4)


def wallpaper(size=512, seed=21, base=(104, 96, 74), stripe=(86, 70, 54)):
    """Faded striped wallpaper, peeling in places."""
    ys, xs = np.mgrid[0:size, 0:size]
    s = ((xs // (size // 16)) % 2).astype(np.float32)
    a, c = fbm(size, 6, seed, 0.55, 3), fbm(size, 4, seed + 2, 0.5, 2)
    col = _rgb(base) * (1 - s[..., None]) + _rgb(stripe) * s[..., None]
    peel = np.clip((c - 0.58) * 9.0, 0, 1)
    col = col * (0.72 + 0.4 * a[..., None]) * (1 - peel[..., None]) + _rgb((150, 142, 126)) * peel[..., None] * (0.8 + 0.3 * a[..., None])
    drip = np.clip((fbm(size, 4, seed + 5, 0.5, 12)[:1].repeat(size, axis=0) - 0.55) * 4, 0, 1) * (ys / size)
    col = col * (1 - 0.45 * drip[..., None])
    return _finish(col, 0.5 - 0.25 * peel + 0.05 * a, 1.0)


def planks(size=512, seed=31, count=6, tint=(92, 68, 44), gap=0.035, vertical=False):
    """Wooden boards (running along U unless vertical)."""
    ys, xs = np.mgrid[0:size, 0:size]
    v = ys / size * count
    idx = np.floor(v).astype(int)
    f = v - idx
    rng = np.random.default_rng(seed)
    tone = rng.uniform(0.72, 1.12, count)[idx % count]
    shift = (rng.integers(0, size, count)[idx % count])
    g = _stretch(size, seed + 1, 1, 6)
    grain = g[ys, (xs + shift) % size]
    ring = np.sin(grain * 38.0) * 0.5 + 0.5
    edge = np.minimum(f, 1 - f)
    groove = np.clip(1 - edge / gap, 0, 1)
    # butt joints
    joint_x = (rng.integers(0, size, count)[idx % count])
    joint = np.clip(1 - np.abs(((xs - joint_x) % size) - size / 2) / (size * gap * 0.25), 0, 1) * 0.0
    dirt = fbm(size, 5, seed + 4, 0.55, 3)
    col = _rgb(tint) * tone[..., None] * (0.7 + 0.25 * ring[..., None] + 0.2 * grain[..., None]) * (0.8 + 0.3 * dirt[..., None])
    col = col * (1 - 0.8 * groove[..., None])
    h = 0.5 + 0.08 * ring - 0.5 * groove - joint
    if vertical:
        col, h = np.transpose(col, (1, 0, 2)), h.T
    return _finish(col, h, 1.6)


def brick(size=512, seed=41, rows=8, tint=(112, 58, 42), mortar=(92, 88, 80)):
    ys, xs = np.mgrid[0:size, 0:size]
    bh = size / rows
    row = np.floor(ys / bh).astype(int)
    bw = size / (rows // 2)
    off = (row % 2) * bw * 0.5
    colx = np.floor((xs + off) / bw).astype(int)
    fx = ((xs + off) / bw) % 1.0
    fy = (ys / bh) % 1.0
    m = 0.06
    edge = np.minimum(np.minimum(fx, 1 - fx) * (bw / bh), np.minimum(fy, 1 - fy))
    mort = np.clip(1 - edge / m, 0, 1)
    rng = np.random.default_rng(seed)
    tone = rng.uniform(0.62, 1.15, (rows, rows))[row % rows, colx % (rows // 2)]
    n, n2 = fbm(size, 6, seed + 1, 0.6, 6), fbm(size, 4, seed + 2, 0.5, 2)
    col = _rgb(tint) * tone[..., None] * (0.75 + 0.4 * n[..., None])
    col = col * (1 - mort[..., None]) + _rgb(mortar) * mort[..., None] * (0.7 + 0.4 * n[..., None])
    damp = np.clip((n2 - 0.5) * 4, 0, 1)
    col = col * (1 - 0.4 * damp[..., None]) + _rgb((10, 16, 8)) * damp[..., None] * 0.6
    h = 0.55 + 0.1 * n - 0.4 * mort
    return _finish(col, h, 2.2)


def flagstone(size=512, seed=51, cells=5, tint=(96, 94, 88)):
    """Irregular stone slabs (tileable Voronoi)."""
    rng = np.random.default_rng(seed)
    pts = (np.stack(np.meshgrid(np.arange(cells), np.arange(cells)), -1).reshape(-1, 2) + rng.uniform(0.15, 0.85, (cells * cells, 2))) / cells
    tones = rng.uniform(0.7, 1.1, len(pts))
    ys, xs = np.mgrid[0:size, 0:size].astype(np.float32) / size
    d1 = np.full((size, size), 9.0, dtype=np.float32)
    d2 = np.full((size, size), 9.0, dtype=np.float32)
    owner = np.zeros((size, size), dtype=np.int32)
    for k, (px, py) in enumerate(pts):
        dx = np.abs(xs - px)
        dy = np.abs(ys - py)
        dx, dy = np.minimum(dx, 1 - dx), np.minimum(dy, 1 - dy)
        d = np.sqrt(dx * dx + dy * dy)
        closer = d < d1
        d2 = np.where(closer, d1, np.minimum(d2, d))
        owner = np.where(closer, k, owner)
        d1 = np.where(closer, d, d1)
    gap = np.clip(1 - (d2 - d1) * cells * 9.0, 0, 1)
    n, n2 = fbm(size, 6, seed + 1, 0.6, 5), fbm(size, 4, seed + 3, 0.5, 2)
    col = _rgb(tint) * tones[owner][..., None] * (0.72 + 0.45 * n[..., None]) * (1 - 0.75 * gap[..., None])
    col = col * (1 - 0.3 * np.clip((n2 - 0.55) * 5, 0, 1)[..., None])
    return _finish(col, 0.55 + 0.12 * n - 0.45 * gap, 2.0)


def rock(size=512, seed=61, tint=(74, 70, 66)):
    a, b, c = fbm(size, 7, seed, 0.6, 3), fbm(size, 6, seed + 1, 0.55, 6), fbm(size, 4, seed + 2, 0.5, 2)
    ridge = 1 - np.abs(a - 0.5) * 2
    strata = np.sin((np.mgrid[0:size, 0:size][0] / size * 9 + a * 3.0) * np.pi * 2) * 0.5 + 0.5
    col = _rgb(tint) * (0.55 + 0.5 * b[..., None] + 0.2 * ridge[..., None]) * (0.86 + 0.14 * strata[..., None])
    wet = np.clip((c - 0.55) * 4, 0, 1)
    col = col * (1 - 0.35 * wet[..., None]) + _rgb((6, 12, 9)) * wet[..., None] * 0.5
    return _finish(col, 0.3 * ridge + 0.5 * b + 0.2 * a, 3.0)


def dirt(size=512, seed=71, tint=(66, 52, 38)):
    a, b = fbm(size, 7, seed, 0.6, 4), fbm(size, 5, seed + 1, 0.55, 12)
    pebbles = np.clip((fbm(size, 3, seed + 2, 0.5, 40) - 0.62) * 9, 0, 1)
    col = _rgb(tint) * (0.6 + 0.6 * a[..., None]) * (0.9 + 0.2 * b[..., None]) + _rgb((60, 58, 54)) * pebbles[..., None] * 0.6
    return _finish(col, 0.5 * a + 0.3 * b + 0.4 * pebbles, 2.4)


def timber(size=512, seed=81, tint=(58, 42, 28)):
    """Rough dark beams: grain along V."""
    g = _stretch(size, seed, 6, 1)
    ring = np.sin(g * 30.0) * 0.5 + 0.5
    n = fbm(size, 5, seed + 2, 0.55, 3)
    split = np.clip(1 - np.abs(_stretch(size, seed + 5, 10, 1, 4) - 0.5) * 30, 0, 1) * np.clip((n - 0.45) * 4, 0, 1)
    col = _rgb(tint) * (0.6 + 0.35 * ring[..., None] + 0.25 * n[..., None]) * (1 - 0.7 * split[..., None])
    return _finish(col, 0.5 + 0.15 * ring - 0.5 * split, 2.0)


def metal(size=256, seed=91, tint=(70, 66, 62), rust=(96, 46, 22)):
    a, b = fbm(size, 6, seed, 0.6, 4), fbm(size, 5, seed + 1, 0.55, 2)
    r = np.clip((b - 0.42) * 4, 0, 1)
    col = _rgb(tint) * (0.7 + 0.4 * a[..., None]) * (1 - r[..., None]) + _rgb(rust) * r[..., None] * (0.6 + 0.6 * a[..., None])
    return _finish(col, 0.5 + 0.25 * a * r, 1.5)


def fabric(size=256, seed=95, tint=(74, 30, 28)):
    ys, xs = np.mgrid[0:size, 0:size]
    weave = (np.sin(xs * np.pi * 2 * 48 / size) * np.sin(ys * np.pi * 2 * 48 / size)) * 0.5 + 0.5
    a = fbm(size, 5, seed, 0.55, 3)
    col = _rgb(tint) * (0.65 + 0.2 * weave[..., None] + 0.35 * a[..., None])
    return _finish(col, 0.5 + 0.2 * weave, 1.0)
