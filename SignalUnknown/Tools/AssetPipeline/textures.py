"""Texture helpers: resize / re-encode source textures, and synthesize own (license-free) textures."""
import io
import numpy as np
from PIL import Image


def encode(img, max_size=2048, jpeg_quality=88, force_png=False):
    """PIL image -> (bytes, mime). Opaque images become JPEG, images with real alpha stay PNG."""
    if max(img.size) > max_size:
        s = max_size / max(img.size)
        img = img.resize((max(1, round(img.size[0] * s)), max(1, round(img.size[1] * s))), Image.LANCZOS)
    has_alpha = img.mode in ('RGBA', 'LA') and np.asarray(img.split()[-1]).min() < 250
    buf = io.BytesIO()
    if has_alpha or force_png:
        img.save(buf, 'PNG', optimize=True)
        return buf.getvalue(), 'image/png'
    img.convert('RGB').save(buf, 'JPEG', quality=jpeg_quality, optimize=True)
    return buf.getvalue(), 'image/jpeg'


def load(data):
    return Image.open(io.BytesIO(data))


def fbm(size, octaves=6, seed=1, persistence=0.55, base=4):
    """Tileable fractal value noise in 0..1 (float32, size x size)."""
    rng = np.random.default_rng(seed)
    out = np.zeros((size, size), dtype=np.float32)
    amp, total, freq = 1.0, 0.0, base
    ys, xs = np.mgrid[0:size, 0:size].astype(np.float32) / size
    for _ in range(octaves):
        grid = rng.random((freq, freq), dtype=np.float32)
        gx, gy = xs * freq, ys * freq
        x0, y0 = np.floor(gx).astype(int) % freq, np.floor(gy).astype(int) % freq
        x1, y1 = (x0 + 1) % freq, (y0 + 1) % freq
        fx, fy = gx - np.floor(gx), gy - np.floor(gy)
        fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
        v = (grid[y0, x0] * (1 - fx) + grid[y0, x1] * fx) * (1 - fy) + (grid[y1, x0] * (1 - fx) + grid[y1, x1] * fx) * fy
        out += v * amp
        total += amp
        amp *= persistence
        freq *= 2
    return out / total


def normal_from_height(h, strength=2.0):
    """Tileable tangent space normal map (RGB uint8) from a height field 0..1."""
    dx = (np.roll(h, -1, axis=1) - np.roll(h, 1, axis=1)) * strength * h.shape[1] / 64.0
    dy = (np.roll(h, -1, axis=0) - np.roll(h, 1, axis=0)) * strength * h.shape[0] / 64.0
    n = np.stack([-dx, dy, np.ones_like(h)], axis=-1)   # glTF: +Y up in texture space (v flipped image rows)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return ((n * 0.5 + 0.5) * 255).clip(0, 255).astype(np.uint8)


def to_image(rgb):
    return Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8), 'RGB')
