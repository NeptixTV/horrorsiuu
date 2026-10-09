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
