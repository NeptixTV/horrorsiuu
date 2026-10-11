"""
Procedural texture toolkit (numpy). All noise is periodic so materials tile seamlessly.
Outputs follow Unreal conventions: BaseColor sRGB, Normal in DirectX orientation (green down),
packed ORM (R = ambient occlusion, G = roughness, B = metallic).
"""
import os
import numpy as np
from PIL import Image

_rng = np.random.default_rng(7)


def seed(value):
    global _rng
    _rng = np.random.default_rng(value)


def rng():
    return _rng


# ---------------------------------------------------------------------------------------
# Periodic noise

def _smooth(t):
    return t * t * t * (t * (t * 6 - 15) + 10)


def value_noise(size, period, seed_offset=0):
    """Tileable value noise: random lattice of `period` cells, quintic interpolation."""
    lattice = rng().random((period, period))
    coords = np.arange(size) * period / size
    i0 = np.floor(coords).astype(int)
    f = _smooth(coords - i0)
    i1 = (i0 + 1) % period
    i0 %= period
    a = lattice[np.ix_(i0, i0)]
    b = lattice[np.ix_(i0, i1)]
    c = lattice[np.ix_(i1, i0)]
    d = lattice[np.ix_(i1, i1)]
    fy = f[:, None]
    fx = f[None, :]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def value_noise_aniso(size, period_y, period_x):
    """Tileable value noise with different lattice periods per axis (streaky grain)."""
    lattice = rng().random((period_y, period_x))
    cy = np.arange(size) * period_y / size
    cx = np.arange(size) * period_x / size
    y0 = np.floor(cy).astype(int)
    x0 = np.floor(cx).astype(int)
    fy = _smooth(cy - y0)[:, None]
    fx = _smooth(cx - x0)[None, :]
    y1 = (y0 + 1) % period_y
    x1 = (x0 + 1) % period_x
    y0 %= period_y
    x0 %= period_x
    a = lattice[np.ix_(y0, x0)]
    b = lattice[np.ix_(y0, x1)]
    c = lattice[np.ix_(y1, x0)]
    d = lattice[np.ix_(y1, x1)]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def grain_field(size, rows=64, cols=3, octaves=4):
    """Long horizontal streaks, like wood fibres along U."""
    out = np.zeros((size, size))
    amp = 1.0
    total = 0.0
    for o in range(octaves):
        out += value_noise_aniso(size, min(rows * 2 ** o, size), min(cols * 2 ** o, size)) * amp
        total += amp
        amp *= 0.55
    return out / total


def fbm(size, base_period=4, octaves=5, persistence=0.5):
    out = np.zeros((size, size))
    amp = 1.0
    total = 0.0
    period = base_period
    for _ in range(octaves):
        if period > size:
            break
        out += value_noise(size, period) * amp
        total += amp
        amp *= persistence
        period *= 2
    return out / total


def ridged(size, base_period=4, octaves=5):
    n = fbm(size, base_period, octaves)
    return 1 - np.abs(n * 2 - 1)


def worley(size, cells, metric='f1'):
    """Tileable cellular noise. Returns distance to the nearest (f1) or f2-f1 (edges)."""
    pts = rng().random((cells, cells, 2))
    yy, xx = np.mgrid[0:size, 0:size] / size * cells
    cy = np.floor(yy).astype(int)
    cx = np.floor(xx).astype(int)
    d1 = np.full((size, size), 9.0)
    d2 = np.full((size, size), 9.0)
    ids = np.zeros((size, size), dtype=int)
    for oy in (-1, 0, 1):
        for ox in (-1, 0, 1):
            ny = (cy + oy) % cells
            nx = (cx + ox) % cells
            p = pts[ny, nx]
            py = cy + oy + p[..., 0]
            px = cx + ox + p[..., 1]
            d = np.sqrt((yy - py) ** 2 + (xx - px) ** 2)
            closer = d < d1
            d2 = np.where(closer, d1, np.minimum(d2, d))
            ids = np.where(closer, ny * cells + nx, ids)
            d1 = np.where(closer, d, d1)
    if metric == 'f1':
        return d1, ids
    return d2 - d1, ids


def warp(field, strength, size_period=4):
    """Domain-warp a periodic field by periodic noise offsets."""
    size = field.shape[0]
    wx = (fbm(size, size_period, 3) - 0.5) * strength * size
    wy = (fbm(size, size_period, 3) - 0.5) * strength * size
    yy, xx = np.mgrid[0:size, 0:size]
    sy = ((yy + wy).astype(int)) % size
    sx = ((xx + wx).astype(int)) % size
    return field[sy, sx]


def blur(img, radius):
    """Periodic box blur (fast, repeated = gaussian-ish)."""
    out = img.astype(np.float64)
    for _ in range(3):
        acc = np.zeros_like(out)
        for d in range(-radius, radius + 1):
            acc += np.roll(out, d, axis=0)
        out = acc / (2 * radius + 1)
        acc = np.zeros_like(out)
        for d in range(-radius, radius + 1):
            acc += np.roll(out, d, axis=1)
        out = acc / (2 * radius + 1)
    return out


def remap(x, a, b):
    return np.clip((x - a) / (b - a + 1e-9), 0, 1)


def smoothstep(a, b, x):
    t = remap(x, a, b)
    return t * t * (3 - 2 * t)


def lerp(a, b, t):
    a = np.asarray(a, dtype=np.float64)
    b = np.asarray(b, dtype=np.float64)
    t = np.asarray(t, dtype=np.float64)
    if t.ndim == 2 and (a.ndim in (1, 3) or b.ndim in (1, 3)):
        t = t[..., None]
    return a * (1 - t) + b * t


def color(rgb):
    return np.array(rgb, dtype=np.float64)


# ---------------------------------------------------------------------------------------
# Maps

def normal_from_height(height, strength=4.0):
    """Tangent-space normal map (DirectX: green flipped) from a periodic height field."""
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 0.5
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 0.5
    nx = -dx * strength
    ny = -dy * strength
    nz = np.ones_like(height)
    length = np.sqrt(nx * nx + ny * ny + nz * nz)
    n = np.stack([nx / length, ny / length, nz / length], axis=-1)
    # Image rows go down, which already matches DirectX (+Y down) for this derivative.
    return n * 0.5 + 0.5


def ao_from_height(height, radius=6, strength=1.5):
    blurred = blur(height, radius)
    ao = 1 - np.clip((blurred - height) * strength, 0, 1)
    return np.clip(ao, 0, 1)


def to_srgb8(linear_or_display):
    return np.clip(np.round(linear_or_display * 255), 0, 255).astype(np.uint8)


def save_rgb(path, rgb, quality=92):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img = Image.fromarray(to_srgb8(np.clip(rgb, 0, 1)), 'RGB')
    if path.endswith('.jpg'):
        img.save(path, quality=quality, subsampling=0, optimize=True)
    else:
        img.save(path, optimize=True)


def save_rgba(path, rgba):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    Image.fromarray(to_srgb8(np.clip(rgba, 0, 1)), 'RGBA').save(path, optimize=True)


def save_gray(path, gray):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    Image.fromarray(to_srgb8(np.clip(gray, 0, 1)), 'L').save(path, optimize=True)


def save_pbr(folder, name, base_color, height, roughness, metallic=None, ao=None, normal_strength=4.0, extra_normal=None):
    """Writes <name>_BC.jpg, <name>_N.jpg (DirectX), <name>_ORM.jpg."""
    size = height.shape[0]
    if metallic is None:
        metallic = np.zeros((size, size))
    if ao is None:
        ao = ao_from_height(height)
    n = normal_from_height(height, normal_strength)
    if extra_normal is not None:
        n = np.clip(n + (extra_normal - 0.5), 0, 1)
    save_rgb(os.path.join(folder, f'{name}_BC.jpg'), base_color)
    save_rgb(os.path.join(folder, f'{name}_N.jpg'), n, quality=95)
    orm = np.stack([ao, np.clip(roughness, 0.02, 1), np.clip(metallic, 0, 1)], axis=-1)
    save_rgb(os.path.join(folder, f'{name}_ORM.jpg'), orm)


# ---------------------------------------------------------------------------------------
# Extra helpers (used by gen_world_textures.py / gen_world_images.py)

def gblur(img, sigma, sigma_x=None):
    """Periodic gaussian blur through the FFT, so tileable maps stay seamless.
    Works on HxW or HxWxC arrays; sigma_x gives an anisotropic (streak) blur."""
    img = np.asarray(img, dtype=np.float64)
    sy = float(sigma)
    sx = sy if sigma_x is None else float(sigma_x)
    if sy <= 0 and sx <= 0:
        return img.copy()
    h, w = img.shape[:2]
    fy = np.fft.fftfreq(h)[:, None]
    fx = np.fft.rfftfreq(w)[None, :]
    g = np.exp(-2 * np.pi ** 2 * ((sy * fy) ** 2 + (sx * fx) ** 2))
    if img.ndim == 2:
        return np.fft.irfft2(np.fft.rfft2(img) * g, s=(h, w))
    return np.stack([np.fft.irfft2(np.fft.rfft2(img[..., c]) * g, s=(h, w)) for c in range(img.shape[2])], -1)


def norm01(a):
    a = np.asarray(a, dtype=np.float64)
    lo, hi = a.min(), a.max()
    return (a - lo) / (hi - lo + 1e-12)


def cells(size, cy, cx=None, jitter=1.0, warp_y=None, warp_x=None):
    """Tileable (optionally anisotropic and domain-warped) Voronoi.
    cy/cx = number of cells along each axis; warp_y/warp_x are pixel offset fields.
    Returns (f1, f2, ids) with distances in cell units and ids = row * cx + col."""
    cx = cy if cx is None else cx
    pts = 0.5 + (rng().random((cy, cx, 2)) - 0.5) * jitter
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float64)
    if warp_y is not None:
        yy = yy + warp_y
    if warp_x is not None:
        xx = xx + warp_x
    yy = yy / size * cy
    xx = xx / size * cx
    by = np.floor(yy).astype(int)
    bx = np.floor(xx).astype(int)
    d1 = np.full((size, size), 9.0)
    d2 = np.full((size, size), 9.0)
    ids = np.zeros((size, size), dtype=np.int64)
    for oy in (-1, 0, 1):
        for ox in (-1, 0, 1):
            ny = (by + oy) % cy
            nx = (bx + ox) % cx
            p = pts[ny, nx]
            d = np.sqrt((yy - (by + oy + p[..., 0])) ** 2 + (xx - (bx + ox + p[..., 1])) ** 2)
            closer = d < d1
            d2 = np.where(closer, d1, np.minimum(d2, d))
            ids = np.where(closer, ny * cx + nx, ids)
            d1 = np.where(closer, d, d1)
    return d1, d2, ids


def fold(canvas, size, margin, op=np.add):
    """Wrap a (size + 2 * margin) canvas back onto a size x size tile with `op`
    (np.add for accumulated values, np.maximum for z-buffers / increasing draw ids)."""
    m = margin
    out = canvas[m:m + size, m:m + size].copy()
    spans = [((0, m), (size - m, size)), ((m + size, size + 2 * m), (0, m))]
    rows = [((m, m + size), (0, size))] + spans
    for (ry0, ry1), (ty0, ty1) in rows:
        for (rx0, rx1), (tx0, tx1) in rows:
            if (ry0, rx0) == (m, m):
                continue
            src = canvas[ry0:ry1, rx0:rx1]
            out[ty0:ty1, tx0:tx1] = op(out[ty0:ty1, tx0:tx1], src)
    return out


def downsample(img, factor):
    """Box-filter downsample by an integer factor (HxW or HxWxC)."""
    h, w = img.shape[:2]
    shp = (h // factor, factor, w // factor, factor) + img.shape[2:]
    return img.reshape(shp).mean(axis=(1, 3))


def wrap_index(start, length, size):
    """Wrapped index range for writing a patch into a tileable map."""
    return np.arange(start, start + length) % size


class WrapCanvas:
    """PIL drawing surface that wraps every primitive across the tile borders (each shape is
    re-drawn at the +-size offsets it overlaps), optionally supersampled for anti-aliasing.
    mode 'F' accumulates float values (last draw wins); mode 'I' is handy for id maps."""

    def __init__(self, size, ss=2, mode='F', fill=0):
        from PIL import ImageDraw
        self.size = size
        self.ss = ss
        self.full = size * ss
        self.img = Image.new(mode, (self.full, self.full), fill)
        self.draw = ImageDraw.Draw(self.img)

    def _offsets(self, xs, ys):
        f = self.full
        x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
        for oy in (-f, 0, f):
            if y1 + oy < -2 or y0 + oy > f + 2:
                continue
            for ox in (-f, 0, f):
                if x1 + ox < -2 or x0 + ox > f + 2:
                    continue
                yield ox, oy

    def _pts(self, pts):
        return [(x * self.ss, y * self.ss) for x, y in pts]

    def line(self, pts, fill, width=1.0):
        p = self._pts(pts)
        w = max(1, int(round(width * self.ss)))
        for ox, oy in self._offsets([q[0] for q in p], [q[1] for q in p]):
            self.draw.line([(x + ox, y + oy) for x, y in p], fill=fill, width=w, joint='curve')

    def polygon(self, pts, fill):
        p = self._pts(pts)
        for ox, oy in self._offsets([q[0] for q in p], [q[1] for q in p]):
            self.draw.polygon([(x + ox, y + oy) for x, y in p], fill=fill)

    def ellipse(self, cx, cy, rx, ry, fill):
        s = self.ss
        x0, x1, y0, y1 = (cx - rx) * s, (cx + rx) * s, (cy - ry) * s, (cy + ry) * s
        for ox, oy in self._offsets([x0, x1], [y0, y1]):
            self.draw.ellipse([x0 + ox, y0 + oy, x1 + ox, y1 + oy], fill=fill)

    def array(self, reduce=True):
        a = np.asarray(self.img).astype(np.float64)
        return downsample(a, self.ss) if (reduce and self.ss > 1) else a


def stamp(buffers, cy, cx, patch_fn, radius, size):
    """Z-buffered stamp into tileable maps. patch_fn(dy, dx) gets local offset grids (pixels)
    and returns (height, mask, extras...); a pixel is written where mask and height wins.
    buffers = [H, *extras] (same length as what patch_fn returns minus the mask)."""
    r = int(np.ceil(radius)) + 1
    iy = np.arange(int(np.floor(cy)) - r, int(np.floor(cy)) + r + 1)
    ix = np.arange(int(np.floor(cx)) - r, int(np.floor(cx)) + r + 1)
    dy = (iy - cy)[:, None] + np.zeros((1, ix.size))
    dx = (ix - cx)[None, :] + np.zeros((iy.size, 1))
    res = patch_fn(dy, dx)
    h, mask = res[0], res[1]
    sel = np.ix_(iy % size, ix % size)
    cur = buffers[0][sel]
    win = mask & (h > cur)
    buffers[0][sel] = np.where(win, h, cur)
    for buf, val in zip(buffers[1:], res[2:]):
        cur_v = buf[sel]
        buf[sel] = np.where(win, val, cur_v)
    return win
