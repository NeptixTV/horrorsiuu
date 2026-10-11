"""
Tileable PBR materials for the town, the houses and their interiors (streets, gardens, roofs,
siding, stone, wallpaper, floors, tiles...). Everything is procedural and periodic (numpy), so
every map tiles seamlessly; the real-world scale of each tile follows the 'uv' value of the
matching entry in Tools/AssetGen/blender/hhmaterials.py (tile size = 1 / uv metres).

Image orientation follows the Blender UVs (V = up): the top of each image is "up" on walls,
roofs (up the slope) and trunks; floors and boards run their strips along U.

Run from the HorrorHeist folder:  python Tools/AssetGen/textures/gen_world_textures.py [names...]
Writes SourceArt/Textures/Materials/<Name>_{BC,N,ORM}.jpg  (BC sRGB, N DirectX, ORM = AO/Rough/Metal)
"""
import math
import os
import sys
import numpy as np
from PIL import ImageFile

sys.path.insert(0, os.path.dirname(__file__))
from texlib import *  # noqa: F401,F403,E402
import texlib as T  # noqa: E402

S = 1024
OUT = 'SourceArt/Textures/Materials'
ImageFile.MAXBLOCK = 1 << 24      # optimized JPEGs of very detailed maps need a larger encoder buffer


# =======================================================================================
# Shared building blocks

def R(seed_value):
    """Local numpy generator (texlib's global one drives the noise functions)."""
    return np.random.default_rng(seed_value)


def grid():
    return np.mgrid[0:S, 0:S].astype(np.float64)


def stains(size, coverage=0.35, period=3, octaves=5, soft=0.08):
    n = fbm(size, period, octaves)
    return smoothstep(1 - coverage - soft, 1 - coverage + soft, n)


def mottling(lo=4, hi=64):
    """Broad + fine tonal variation, roughly zero-mean."""
    return (fbm(S, lo, 5) - 0.5) + 0.35 * (fbm(S, hi, 3) - 0.5)


def speckle(density, radius=0.0, rnd=None):
    """Sparse random dots (0..1). radius > 0 softens them with a periodic blur."""
    rnd = rnd or rng()
    m = (rnd.random((S, S)) < density).astype(np.float64)
    if radius > 0:
        m = gblur(m, radius)
        m = m / (m.max() + 1e-9)
    return m


def crack_paths(canvas, rnd, count, steps=(25, 70), seg=(4.0, 9.0), width=(1.6, 0.6), wander=0.32,
                branch=0.08, starts=None, angle=None, fill=1.0, zig=0.55):
    """Random-walk hairline cracks drawn on a WrapCanvas (tapering, occasionally branching)."""
    for i in range(count):
        if starts is not None:
            x, y = starts[i % len(starts)]
        else:
            x, y = rnd.uniform(0, S, 2)
        a = rnd.uniform(0, 2 * np.pi) if angle is None else angle + rnd.normal(0, 0.3)
        n = int(rnd.integers(*steps))
        _walk(canvas, rnd, x, y, a, n, seg, width, wander, branch, fill, depth=0, zig=zig)


def _walk(canvas, rnd, x, y, a, n, seg, width, wander, branch, fill, depth, zig=0.55):
    """Cracks keep a general heading (slow drift) but zig-zag at small scale."""
    w0, w1 = width
    heading = a
    for k in range(n):
        t = k / max(n - 1, 1)
        heading += rnd.normal(0, wander * 0.35)
        a = heading + rnd.normal(0, zig)
        step = rnd.uniform(*seg)
        nx, ny = x + math.cos(a) * step, y + math.sin(a) * step
        canvas.line([(x, y), (nx, ny)], fill=fill, width=w0 + (w1 - w0) * t)
        if depth < 1 and rnd.random() < branch:
            _walk(canvas, rnd, nx, ny, heading + rnd.choice([-1, 1]) * rnd.uniform(0.5, 0.9),
                  max(4, int((n - k) * 0.35)), seg, (max(w0 * 0.6, 0.5), w1 * 0.7), wander, 0, fill, depth + 1, zig)
        x, y = nx, ny


def downward_streaks(density=0.5, length=(60, 300), width=(1.0, 6.0), rnd=None, ss=1, softness=1.5):
    """Grime that ran down a wall: vertical streaks, strongest at their top, fading downwards,
    slightly wavering and changing width as they run."""
    rnd = rnd or rng()
    cv = WrapCanvas(S, ss=ss)
    count = int(density * 200)
    for _ in range(count):
        x = rnd.uniform(0, S)
        y = rnd.uniform(0, S)
        ln = rnd.uniform(*length)
        w = rnd.uniform(*width)
        strength = rnd.uniform(0.3, 1.0)
        ph = rnd.uniform(0, 6.28)
        segs = 12
        for k in range(segs):
            t0 = k / segs
            yy0 = y + ln * t0
            yy1 = y + ln * (k + 1) / segs
            xo0 = x + math.sin(yy0 * 0.03 + ph) * 1.5
            xo1 = x + math.sin(yy1 * 0.03 + ph) * 1.5
            wk = w * (1 - 0.6 * t0) * (0.75 + 0.5 * abs(math.sin(t0 * 7 + ph)))
            cv.line([(xo0, yy0), (xo1, yy1)], fill=float(strength * (1 - t0) ** 1.4), width=max(wk, 0.6))
    a = cv.array()
    return np.clip(gblur(a, softness, softness * 0.6), 0, 1)


def scratch_lines(rnd, count, length=(20, 120), width=(0.6, 1.4), angle=None, angle_jitter=0.4, ss=2):
    cv = WrapCanvas(S, ss=ss)
    for _ in range(count):
        x, y = rnd.uniform(0, S, 2)
        a = rnd.uniform(0, np.pi) if angle is None else angle + rnd.normal(0, angle_jitter)
        ln = rnd.uniform(*length)
        bend = rnd.normal(0, 0.15)
        pts = []
        for k in range(6):
            t = k / 5
            aa = a + bend * t
            pts.append((x + math.cos(aa) * ln * t, y + math.sin(aa) * ln * t))
        cv.line(pts, fill=float(rnd.uniform(0.4, 1.0)), width=rnd.uniform(*width))
    return np.clip(cv.array(), 0, 1)


def wood_grain(size=S, ring_rows=10, ring_cols=2, density=14.0, fibre_rows=160, seed_value=None):
    """Plank figure along U: wavy growth rings (sharp latewood lines) + fine fibres + pores.
    Returns (rings, fibres, pores) in 0..1."""
    if seed_value is not None:
        seed(seed_value)
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float64) / size
    bands = grain_field(size, ring_rows, ring_cols, 3)
    wob = fbm(size, 3, 3)
    phase = (bands * density + yy * density * 0.35 + wob * 2.0) % 1.0
    rings = smoothstep(0.55, 0.9, phase) * (1 - smoothstep(0.9, 1.0, phase))
    fibres = grain_field(size, fibre_rows, 4, 4)
    pores = smoothstep(0.62, 0.8, value_noise_aniso(size, min(size // 2, 512), 24)) * smoothstep(0.4, 0.7, fibres)
    return rings, fibres, pores


def normal_from_gradient(gx, gy, strength=1.0):
    """Same convention as texlib.normal_from_height (DirectX), from an analytic height gradient."""
    nx = -gx * strength
    ny = -gy * strength
    nz = np.ones_like(gx)
    length = np.sqrt(nx * nx + ny * ny + nz * nz)
    return np.stack([nx / length, ny / length, nz / length], axis=-1) * 0.5 + 0.5


def combine_normals(a, b):
    """Whiteout-style blend of two encoded normal maps."""
    na = a * 2 - 1
    nb = b * 2 - 1
    n = np.stack([na[..., 0] + nb[..., 0], na[..., 1] + nb[..., 1], na[..., 2] * nb[..., 2]], -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return n * 0.5 + 0.5


def finish(name, bc, height, rough, metal=None, ao=None, normal_strength=4.0, ao_radius=6, ao_strength=1.5,
           ao_mul=None, normal=None):
    """Writes the set through texlib (save_pbr), or with a precomputed normal map when given."""
    if ao is None:
        ao = ao_from_height(height, ao_radius, ao_strength)
    if ao_mul is not None:
        ao = ao * ao_mul
    bc = np.clip(bc, 0, 1)
    rough = np.clip(rough, 0.02, 1)
    ao = np.clip(ao, 0, 1)
    if normal is None:
        save_pbr(OUT, name, bc, height, rough, metal, ao, normal_strength=normal_strength)
        return
    if metal is None:
        metal = np.zeros((S, S))
    save_rgb(os.path.join(OUT, f'{name}_BC.jpg'), bc)
    save_rgb(os.path.join(OUT, f'{name}_N.jpg'), normal, quality=95)
    save_rgb(os.path.join(OUT, f'{name}_ORM.jpg'), np.stack([ao, rough, np.clip(metal, 0, 1)], axis=-1))


# =======================================================================================
# Ground: sidewalk, grass, dirt, gravel

def concrete_sidewalk():
    """2 m tile: 2 x 2 poured slabs (tooled joints every metre), broom finish, cracks, gum, stains."""
    seed(201)
    rnd = R(201)
    yy, xx = grid()                    # 2 m tile: 512 px per metre
    wob_x = (fbm(S, 6, 3) - 0.5) * 3.0
    wob_y = (fbm(S, 6, 3) - 0.5) * 3.0
    ux = (xx + wob_x - 256) % 512                       # joints every metre, at 256 / 768 px
    uy = (yy + wob_y - 256) % 512
    jx = np.minimum(ux, 512 - ux)
    jy = np.minimum(uy, 512 - uy)
    dj = np.minimum(jx, jy)
    groove = 1 - smoothstep(1.2, 3.6, dj)               # ~1 cm tooled groove
    margin = smoothstep(14, 22, dj)                     # smooth trowelled edge band (~3-4 cm)
    bevel = smoothstep(2.5, 9, dj)                      # rounded slab edge
    slab = ((np.floor(((xx - 256) % S) / 512) + 2 * np.floor(((yy - 256) % S) / 512))).astype(int)
    slab_tone = np.array([0.0, 0.035, -0.03, 0.015])[slab]

    # Broom finish: fine streaks along U, gently arced, absent in the trowelled margins.
    bw = (fbm(S, 2, 3) - 0.5) * 40
    broom = value_noise_aniso(S, 384, 6)
    sy = ((yy + bw).astype(int)) % S
    broom = broom[sy, xx.astype(int)]
    broom = 0.6 * broom + 0.4 * value_noise_aniso(S, 768, 12)[sy, xx.astype(int)]
    broom = (broom - 0.5) * margin

    sand = fbm(S, 256, 2)
    blot = fbm(S, 3, 6)
    pits = speckle(0.0022, 0.0, rnd)
    pits = np.clip(gblur(pits, 0.7) * 6, 0, 1)
    agg_d, _, agg_id = cells(S, 140)
    agg_tone = R(5).random(140 * 140)[agg_id]
    wear = smoothstep(0.55, 0.8, fbm(S, 4, 4))           # paste worn off, aggregate shows
    aggregate = (1 - smoothstep(0.25, 0.45, agg_d)) * wear * margin

    # Cracks: a few long ones running from joints across slabs + hairlines.
    cv = WrapCanvas(S, ss=2)
    for (x0, y0, a0, n0) in ((256, 410, 0.15, 60), (768, 130, 2.9, 45), (610, 768, -1.4, 38), (90, 256, 1.75, 30)):
        crack_paths(cv, rnd, 1, steps=(n0, n0 + 1), seg=(4, 8), width=(1.8, 0.7), wander=0.2, branch=0.04,
                    starts=[(x0, y0)], angle=a0)
    crack_paths(cv, rnd, 5, steps=(8, 20), seg=(3, 6), width=(1.0, 0.5), wander=0.3, branch=0.0)
    crack = np.clip(cv.array() * 1.2, 0, 1)
    crack_halo = np.clip(gblur(crack, 3) * 2.5, 0, 1)

    # Chewing gum: flattened dark discs.
    gum_cv = WrapCanvas(S, ss=2)
    hot = fbm(S, 3, 3)
    placed = 0
    while placed < 34:
        x, y = rnd.uniform(0, S, 2)
        if hot[int(y), int(x)] < 0.45 and rnd.random() < 0.7:
            continue
        r = rnd.uniform(2.5, 6.0)
        pts = []
        ph = rnd.uniform(0, 6.28, 2)
        for k in range(18):
            t = k / 18 * 2 * np.pi
            rr = r * (1 + 0.18 * math.sin(3 * t + ph[0]) + 0.1 * math.sin(5 * t + ph[1]))
            pts.append((x + math.cos(t) * rr, y + math.sin(t) * rr * rnd.uniform(0.85, 1.0)))
        gum_cv.polygon(pts, fill=float(rnd.uniform(0.45, 1.0)))
        placed += 1
    gum = gblur(gum_cv.array(), 0.7)
    gum_mask = np.clip(gum * 1.3, 0, 1)
    gum_rim = np.clip(gblur(gum_mask, 1.5) - gum_mask, 0, 1)

    # Wet / dirty stains, darker grime collected in joints, faint tannin leaf prints.
    wet = stains(S, 0.22, 3, 5, 0.12)
    dirt = stains(S, 0.35, 5, 5, 0.2)
    joint_dirt = np.clip(1 - smoothstep(0, 10, dj), 0, 1) * (0.5 + 0.5 * fbm(S, 16, 3))
    moss = (1 - smoothstep(0.5, 4.0, dj)) * smoothstep(0.55, 0.7, fbm(S, 12, 4))
    leaf_cv = WrapCanvas(S, ss=2)
    for _ in range(9):
        x, y = rnd.uniform(0, S, 2)
        a = rnd.uniform(0, np.pi)
        ln = rnd.uniform(14, 24)
        pts = []
        for k in range(16):
            t = k / 15 * 2 * np.pi
            u = math.cos(t) * ln
            v = math.sin(t) * ln * 0.45 * (1 - 0.3 * math.cos(t))
            pts.append((x + u * math.cos(a) - v * math.sin(a), y + u * math.sin(a) + v * math.cos(a)))
        leaf_cv.polygon(pts, fill=float(rnd.uniform(0.3, 0.7)))
    leafprint = gblur(leaf_cv.array(), 1.2)

    tone = 0.47 + slab_tone + 0.07 * (blot - 0.5) + 0.05 * (sand - 0.5) + 0.03 * broom
    tone = tone + 0.035 * (1 - margin) * bevel
    bc = np.stack([tone * 1.0, tone * 0.985, tone * 0.955], -1)
    bc = lerp(bc, bc * lerp(color([0.85, 0.82, 0.78]), color([1.15, 1.12, 1.08]), agg_tone), aggregate)
    bc = lerp(bc, bc * color([0.80, 0.78, 0.74]), dirt * 0.5)
    bc = lerp(bc, bc * color([0.62, 0.60, 0.57]), wet * 0.75)
    bc = lerp(bc, color([0.42, 0.33, 0.24]) * bc / 0.47, leafprint * 0.55)
    bc = lerp(bc, color([0.19, 0.18, 0.16]), joint_dirt * 0.7)
    bc = lerp(bc, color([0.15, 0.18, 0.09]), moss * 0.8)
    bc = lerp(bc, color([0.20, 0.19, 0.18]), pits * 0.7)
    bc = lerp(bc, color([0.17, 0.165, 0.16]), crack * 0.85)
    bc = lerp(bc, bc * 0.82, crack_halo * 0.4)
    gum_col = lerp(color([0.17, 0.165, 0.16]), color([0.30, 0.29, 0.28]), fbm(S, 48, 3)) * (0.9 + 0.25 * gum[..., None])
    bc = lerp(bc, gum_col, gum_mask * 0.92)
    bc = lerp(bc, bc * 0.85, gum_rim * 0.5)

    height = (broom * 0.22 + (sand - 0.5) * 0.08 + (blot - 0.5) * 0.1 + aggregate * 0.06
              - pits * 0.25 - crack * 0.7 - groove * 1.4 + (bevel - 1) * 0.35 + gum_mask * 0.12)
    rough = (0.88 - 0.05 * (1 - margin) - 0.28 * wet - 0.08 * dirt + 0.05 * joint_dirt
             - 0.3 * gum_mask + 0.03 * (sand - 0.5))
    finish('T_Concrete_Sidewalk', bc, height, rough, normal_strength=3.5, ao_radius=5, ao_strength=1.8)


def grass():
    """2 m tile: mown lawn from above - dense short blades over thatch, some brown patches, clover."""
    seed(202)
    rnd = R(202)
    cv = WrapCanvas(S, ss=2, mode='I', fill=0)
    brown = smoothstep(0.58, 0.72, fbm(S, 3, 4))
    clover_zone = smoothstep(0.6, 0.7, fbm(S, 5, 4)) * (1 - brown)
    attrs = []                        # per id: (z, r, g, b, kind)

    def add(z, rgb, kind):
        attrs.append((z, rgb[0], rgb[1], rgb[2], kind))
        return len(attrs)             # ids start at 1 (0 = soil)

    def blade(x, y, length, width, ang, bend, cols, z0, kind):
        pts = [(x, y)]
        a = ang
        for k in range(3):
            a += bend
            x += math.cos(a) * length / 3
            y += math.sin(a) * length / 3
            pts.append((x, y))
        for k in range(3):
            i = add(z0 + 0.04 * k, cols[k], kind)
            cv.line([pts[k], pts[k + 1]], fill=i, width=width * (1 - 0.22 * k))

    green_pal = np.array([[0.30, 0.37, 0.15], [0.26, 0.33, 0.12], [0.36, 0.42, 0.19], [0.22, 0.29, 0.11],
                          [0.33, 0.38, 0.18], [0.40, 0.44, 0.22]])
    straw_pal = np.array([[0.46, 0.40, 0.25], [0.40, 0.33, 0.20], [0.52, 0.46, 0.31], [0.34, 0.28, 0.18]])

    # Thatch: dead flattened blades at the bottom.
    for _ in range(22000):
        x, y = rnd.uniform(0, S, 2)
        c = straw_pal[rnd.integers(len(straw_pal))] * rnd.uniform(0.6, 1.0)
        blade(x, y, rnd.uniform(8, 26), rnd.uniform(1.0, 1.8), rnd.uniform(0, 2 * np.pi), rnd.normal(0, 0.15),
              [c * 0.85, c, c * 1.05], rnd.uniform(0.0, 0.2), 0)

    n_blades = 95000
    clover_at = int(n_blades * 0.45)
    for b in range(n_blades):
        if b == clover_at:
            for _ in range(900):
                x, y = rnd.uniform(0, S, 2)
                if clover_zone[int(y) % S, int(x) % S] < rnd.random():
                    continue
                r = rnd.uniform(3.2, 5.2)
                rot = rnd.uniform(0, 2 * np.pi)
                base = np.array([0.24, 0.34, 0.15]) * rnd.uniform(0.8, 1.1)
                z = rnd.uniform(0.45, 0.7)
                for k in range(3):
                    a = rot + k * 2 * np.pi / 3
                    lx, ly = x + math.cos(a) * r * 0.9, y + math.sin(a) * r * 0.9
                    i = add(z + 0.02 * k, base * rnd.uniform(0.9, 1.1), 2)
                    cv.ellipse(lx, ly, r, r, fill=i)
                    i2 = add(z + 0.03, base * 1.35, 2)
                    cv.ellipse(lx - math.cos(a) * r * 0.25, ly - math.sin(a) * r * 0.25, r * 0.35, r * 0.35, fill=i2)
        x, y = rnd.uniform(0, S, 2)
        t = b / n_blades
        z = 0.2 + 0.8 * t + rnd.uniform(-0.05, 0.05)
        dead = brown[int(y) % S, int(x) % S] > rnd.random() * 1.1
        if dead:
            c = straw_pal[rnd.integers(len(straw_pal))] * rnd.uniform(0.75, 1.05)
            tip = c * 1.1
        else:
            c = green_pal[rnd.integers(len(green_pal))] * rnd.uniform(0.8, 1.12)
            tip = c * np.array([1.18, 1.12, 0.95]) if rnd.random() < 0.6 else c * 1.05
        upright = rnd.random() < 0.45
        length = rnd.uniform(3, 9) if upright else rnd.uniform(9, 26)
        width = rnd.uniform(1.1, 2.0)
        blade(x, y, length, width, rnd.uniform(0, 2 * np.pi), rnd.normal(0, 0.12),
              [c * 0.8, c, tip], z, 1)

    ids = cv.array(reduce=False).astype(np.int64)
    A = np.array([(0.0, 0.10, 0.08, 0.05, 3)] + attrs)
    z = A[ids, 0]
    col = A[ids, 1:4]
    kind = A[ids, 4]
    soil = (ids == 0).astype(np.float64)
    z = np.where(ids == 0, -0.15, z)
    z = downsample(z, 2)
    col = downsample(col, 2)
    soil = downsample(soil, 2)
    clov = downsample((kind == 2).astype(np.float64), 2)
    thatch = downsample((kind == 0).astype(np.float64), 2)

    depth = np.clip(z, -0.15, 1.05)
    occl = 1 - np.clip((gblur(depth, 2.5) - depth) * 2.2, 0, 0.6)
    shade = (0.42 + 0.58 * np.clip(depth + 0.15, 0, 1)) * occl
    fine = fbm(S, 128, 2)
    bc = col * shade[..., None] * (0.92 + 0.16 * fine[..., None])
    bc = bc * (0.95 + 0.1 * (fbm(S, 4, 4)[..., None] - 0.5))
    height = gblur(depth, 0.6)
    rough = 0.93 + 0.04 * thatch + 0.03 * soil - 0.12 * clov + 0.02 * (fine - 0.5)
    ao = np.clip(0.55 + 0.45 * np.clip(depth + 0.15, 0, 1), 0, 1) * occl
    finish('T_Grass', bc, height, rough, ao=ao, normal_strength=2.6)


def dirt():
    """2 m tile: damp garden soil with clods, small pebbles, twigs and leaf litter."""
    seed(203)
    rnd = R(203)
    big = fbm(S, 3, 5)
    wy = (fbm(S, 8, 3) - 0.5) * 24
    wx = (fbm(S, 8, 3) - 0.5) * 24
    clod_d, _, clod_id = cells(S, 64, warp_y=wy, warp_x=wx)
    clods = (1 - smoothstep(0.1, 0.75, clod_d)) * (0.4 + 0.6 * R(1).random(64 * 64)[clod_id])
    mid_d, _, mid_id = cells(S, 150, warp_y=wy * 0.5, warp_x=wx * 0.5)
    lumps = (1 - smoothstep(0.05, 0.7, mid_d)) * (0.3 + 0.7 * R(2).random(150 * 150)[mid_id])
    small_d, _, small_id = cells(S, 330)
    crumbs = (1 - smoothstep(0.0, 0.6, small_d)) * (R(3).random(330 * 330)[small_id] > 0.35)
    grit = fbm(S, 256, 2)
    damp = smoothstep(0.45, 0.75, gblur(fbm(S, 5, 4), 3)) * 0.8
    clump_mask = smoothstep(0.35, 0.65, fbm(S, 6, 4))
    # Pebbles (0.5 - 2.5 cm) half buried in the soil.
    H = np.full((S, S), -9.0)
    PID = np.full((S, S), -1.0)
    n_peb = 520
    for i in range(n_peb):
        r = rnd.uniform(1.8, 6.5) if rnd.random() < 0.85 else rnd.uniform(6.5, 11)
        asp = rnd.uniform(0.55, 0.95)
        ang = rnd.uniform(0, np.pi)
        ph1, ph2 = rnd.uniform(0, 6.28, 2)
        ca, sa = math.cos(ang), math.sin(ang)
        cy, cx = rnd.uniform(0, S, 2)

        def patch(dy, dx, r=r, asp=asp, ca=ca, sa=sa, ph1=ph1, ph2=ph2, i=i):
            u = (dx * ca + dy * sa) / r
            v = (-dx * sa + dy * ca) / (r * asp)
            th = np.arctan2(v, u)
            q = (u * u + v * v) * (1 + 0.12 * np.sin(3 * th + ph1) + 0.07 * np.sin(5 * th + ph2))
            h = np.sqrt(np.clip(1 - q, 0, 1)) * r * 0.06
            return h, q < 1, np.full(h.shape, float(i))
        stamp([H, PID], cy, cx, patch, r * 1.3, S)
    peb = H > 0
    peb_h = np.where(peb, H, 0.0)
    peb_tone = R(7).random(n_peb + 1)
    peb_pal = np.array([[0.46, 0.43, 0.39], [0.52, 0.47, 0.40], [0.36, 0.33, 0.30], [0.55, 0.53, 0.50],
                        [0.43, 0.35, 0.27]])
    pid = np.clip(PID.astype(int), 0, n_peb)
    peb_col = peb_pal[(peb_tone[pid] * len(peb_pal)).astype(int) % len(peb_pal)]
    peb_col = peb_col * (0.8 + 0.35 * fbm(S, 128, 2)[..., None])

    # Twigs and leaf litter.
    tw = WrapCanvas(S, ss=2)
    for _ in range(60):
        x, y = rnd.uniform(0, S, 2)
        a = rnd.uniform(0, 2 * np.pi)
        ln = rnd.uniform(18, 90)
        w = rnd.uniform(1.6, 3.6)
        pts = [(x, y)]
        for k in range(5):
            a += rnd.normal(0, 0.25)
            x += math.cos(a) * ln / 5
            y += math.sin(a) * ln / 5
            pts.append((x, y))
        tw.line(pts, fill=float(rnd.uniform(0.5, 1.0)), width=w)
        if rnd.random() < 0.4:
            bx, by = pts[2]
            b = a + rnd.choice([-1, 1]) * rnd.uniform(0.5, 1.0)
            tw.line([(bx, by), (bx + math.cos(b) * ln * 0.3, by + math.sin(b) * ln * 0.3)], fill=0.6, width=w * 0.6)
    twig = np.clip(tw.array(), 0, 1)
    twig_tone = fbm(S, 32, 3)
    lf = WrapCanvas(S, ss=2)
    for _ in range(60):
        x, y = rnd.uniform(0, S, 2)
        a = rnd.uniform(0, np.pi)
        ln = rnd.uniform(8, 20)
        pts = []
        for k in range(14):
            t = k / 14 * 2 * np.pi
            u = math.cos(t) * ln
            v = math.sin(t) * ln * 0.42 * (1 - 0.35 * math.cos(t)) * (1 + 0.15 * math.sin(5 * t))
            pts.append((x + u * math.cos(a) - v * math.sin(a), y + u * math.sin(a) + v * math.cos(a)))
        lf.polygon(pts, fill=float(rnd.uniform(0.2, 1.0)))
    leaf = gblur(lf.array(), 0.5)
    leaf_m = np.clip(leaf * 3, 0, 1)

    soil_dry = color([0.44, 0.35, 0.26])
    soil_wet = color([0.21, 0.155, 0.11])
    relief = clods * clump_mask * 0.6 + lumps * 0.4 + crumbs * 0.25
    relief_n = np.clip(relief / 0.9, 0, 1)
    cavity = np.clip((gblur(relief, 3) - relief) * 4, 0, 1)
    organic = smoothstep(0.6, 0.75, fbm(S, 96, 3))
    bc = lerp(soil_wet, soil_dry, np.clip(0.15 + 0.75 * relief_n - 0.3 * damp, 0, 1))
    bc = bc * (0.82 + 0.36 * grit[..., None]) * (0.92 + 0.16 * (big[..., None] - 0.5))
    bc = lerp(bc, color([0.08, 0.06, 0.045]), cavity * 0.7)
    bc = lerp(bc, color([0.11, 0.08, 0.05]), organic * 0.5)
    bc = lerp(bc, bc * 1.18, (crumbs * (1 - damp))[..., None] * 0.35)
    bc = lerp(bc, color([0.30, 0.20, 0.12]) * (0.7 + 0.6 * leaf[..., None]), leaf_m * 0.85)
    bc = lerp(bc, lerp(color([0.30, 0.23, 0.16]), color([0.52, 0.45, 0.36]), twig_tone), twig)
    peb_ao = 1 - 0.5 * np.clip(gblur(peb.astype(float), 2.0) - peb, 0, 1)
    bc = bc * peb_ao[..., None]
    bc = lerp(bc, peb_col * lerp(1.0, 0.7, damp)[..., None], peb.astype(float))

    height = (clods * clump_mask * 0.5 + lumps * 0.25 + crumbs * 0.1 + grit * 0.06 + big * 0.2 + peb_h * 1.0
              + twig * 0.25 + leaf_m * 0.06)
    rough = 0.93 - 0.32 * damp - 0.12 * peb * (0.5 + damp) - 0.04 * crumbs
    finish('T_Dirt', bc, height, rough, normal_strength=5.0, ao_radius=4, ao_strength=2.0)


def gravel():
    """1.25 m tile: driveway gravel, packed crushed/rounded stones 1-3 cm, strong relief."""
    seed(204)
    rnd = R(204)
    ss = 2
    N = S * ss
    H = np.full((N, N), -9.0)
    GID = np.full((N, N), -1.0)
    ppm = N / 1.25
    stones = []
    # Bed of small stones, then medium, then a few larger ones on top (z-buffered).
    for count, rmin, rmax, zbase in ((12000, 0.005, 0.009, 0.0), (6000, 0.008, 0.013, 0.004), (600, 0.012, 0.017, 0.006)):
        for _ in range(count):
            stones.append((rnd.uniform(rmin, rmax) * ppm, zbase))
    tone = R(9).random(len(stones))
    for i, (r, zb) in enumerate(stones):
        asp = rnd.uniform(0.55, 0.95)
        ang = rnd.uniform(0, np.pi)
        ph = rnd.uniform(0, 6.28, 3)
        angular = rnd.uniform(0.05, 0.22)
        ca, sa = math.cos(ang), math.sin(ang)
        cy, cx = rnd.uniform(0, N, 2)
        flat = rnd.uniform(0.55, 0.9)
        z0 = zb * ppm + rnd.uniform(-0.002, 0.002) * ppm

        def patch(dy, dx, r=r, asp=asp, ca=ca, sa=sa, ph=ph, angular=angular, flat=flat, z0=z0, i=i):
            u = (dx * ca + dy * sa) / r
            v = (-dx * sa + dy * ca) / (r * asp)
            th = np.arctan2(v, u)
            q = (u * u + v * v) * (1 + angular * np.sin(3 * th + ph[0]) + angular * 0.6 * np.sin(4 * th + ph[1])
                                   + 0.05 * np.sin(7 * th + ph[2]))
            dome = np.sqrt(np.clip(1 - q, 0, 1))
            h = z0 + np.minimum(dome, flat) * r * asp * 0.9
            return h, q < 1, np.full(h.shape, float(i))
        stamp([H, GID], cy, cx, patch, r * 1.35, N)
    has = H > -9
    Hs = np.where(has, H, H[has].min() - 2)
    gid = np.clip(GID.astype(int), 0, len(stones) - 1)
    pal = np.array([[0.50, 0.49, 0.47], [0.58, 0.55, 0.50], [0.40, 0.39, 0.38], [0.47, 0.42, 0.36],
                    [0.62, 0.60, 0.57], [0.33, 0.32, 0.31], [0.52, 0.45, 0.38], [0.45, 0.45, 0.46]])
    t = tone[gid]
    col = pal[(t * len(pal)).astype(int) % len(pal)] * (0.88 + 0.24 * ((t * 7.3) % 1.0))[..., None]
    # Downsample stone attributes to the final resolution.
    col = downsample(col, ss)
    Hd = downsample(Hs, ss) / ppm * S / 1.25   # height in final pixels
    hasd = downsample(has.astype(float), ss)
    seed(205)
    grain = fbm(S, 256, 2)
    spk = speckle(0.03, 0.0, R(11))
    spk2 = speckle(0.02, 0.0, R(12))
    dirt_ = stains(S, 0.35, 4, 4, 0.2)
    damp = stains(S, 0.25, 3, 4, 0.15)
    low = Hd - gblur(Hd, 6)
    cavity = np.clip(-low / 6, 0, 1)
    bc = col * (0.9 + 0.2 * grain[..., None])
    bc = lerp(bc, bc * 0.55, spk * 0.6)
    bc = lerp(bc, np.minimum(bc * 1.35, 1), spk2 * 0.5)
    bc = lerp(bc, color([0.22, 0.19, 0.16]), (1 - hasd) * 0.9)
    bc = lerp(bc, bc * color([0.78, 0.72, 0.64]), dirt_ * 0.5)
    bc = bc * (1 - 0.55 * cavity[..., None])
    bc = lerp(bc, bc * 0.62, damp * 0.6)
    height = Hd / 14.0 + grain * 0.02
    rough = 0.82 + 0.08 * cavity - 0.25 * damp - 0.06 * (1 - grain)
    finish('T_Gravel', bc, height, rough, normal_strength=7.0, ao_radius=5, ao_strength=2.2)


# =======================================================================================
# Plants: foliage, bark

def foliage():
    """~0.9 m tile: dense hedge / canopy - thousands of overlapping small leaves, dark gaps."""
    seed(206)
    rnd = R(206)
    ss = 2
    N = S * ss
    H = np.full((N, N), -1e3)
    LID = np.zeros((N, N))
    GX = np.zeros((N, N))
    GY = np.zeros((N, N))
    VV = np.zeros((N, N))
    UU = np.zeros((N, N))
    # Twigs deep inside the bush (drawn into the z-buffer as thin low ridges).
    tw = WrapCanvas(S, ss=ss)
    for _ in range(140):
        x, y = rnd.uniform(0, S, 2)
        a = rnd.uniform(0, 2 * np.pi)
        pts = [(x, y)]
        for k in range(6):
            a += rnd.normal(0, 0.3)
            x += math.cos(a) * 14
            y += math.sin(a) * 14
            pts.append((x, y))
        tw.line(pts, fill=float(rnd.uniform(0.5, 1.0)), width=rnd.uniform(1.5, 3.5))
    twig = tw.array(reduce=False)
    n_leaves = 14000
    pal = np.array([[0.30, 0.42, 0.17], [0.26, 0.38, 0.15], [0.34, 0.46, 0.20], [0.22, 0.33, 0.13],
                    [0.38, 0.47, 0.22], [0.29, 0.40, 0.19]])
    info = np.zeros((n_leaves + 1, 4))
    info[0] = (0.04, 0.05, 0.03, 0)
    for i in range(1, n_leaves + 1):
        depth = i / n_leaves
        L = rnd.uniform(24, 40) * ss * 0.5          # half length (2x pixels)
        W = L * rnd.uniform(0.38, 0.52)
        ang = rnd.uniform(0, 2 * np.pi)
        ca, sa = math.cos(ang), math.sin(ang)
        tu = rnd.normal(0, 0.45)                    # tilt along / across the leaf (height per unit)
        tv = rnd.normal(0, 0.35)
        curl = rnd.uniform(0.15, 0.45)
        z0 = depth * 60 + rnd.uniform(-6, 6)
        cy, cx = rnd.uniform(0, N, 2)
        r = rnd.random()
        if r < 0.035:
            c = np.array([0.40, 0.35, 0.20]) * rnd.uniform(0.75, 1.0)      # dead leaf
        elif r < 0.15:
            c = np.array([0.42, 0.50, 0.21]) * rnd.uniform(0.9, 1.1)       # young leaf
        else:
            c = pal[rnd.integers(len(pal))] * rnd.uniform(0.85, 1.12)
        info[i] = (c[0], c[1], c[2], depth)

        def patch(dy, dx, L=L, W=W, ca=ca, sa=sa, tu=tu, tv=tv, curl=curl, z0=z0, i=i):
            u = (dx * ca + dy * sa) / L
            v = (-dx * sa + dy * ca) / W
            wprof = np.clip(1 - u * u, 0, 1) ** 0.7 * (1 - 0.18 * u)
            inside = (np.abs(u) < 1) & (np.abs(v) < wprof)
            vn = v / np.maximum(wprof, 1e-3)
            h = z0 + (tu * u + tv * v) * W * 0.9 + curl * W * (1 - np.clip(v, -1.2, 1.2) ** 2) - 0.25 * curl * W * u * u
            gy, gx = np.gradient(h)
            return h, inside, np.full(h.shape, float(i)), gx, gy, vn, u
        stamp([H, LID, GX, GY, VV, UU], cy, cx, patch, L * 1.05, N)
    lid = LID.astype(int)
    leaf = (lid > 0)
    gap_h = (twig * 8 - 10)
    Hn = np.where(leaf, H, gap_h)
    twig_m = (~leaf) & (twig > 0.3)
    col = info[lid, :3]
    depth = np.where(leaf, info[lid, 3], 0.0)
    vn = np.where(leaf, VV, 0)
    un = np.where(leaf, UU, 0)
    rib = np.exp(-(vn / 0.07) ** 2) * (np.abs(un) < 0.92)
    veins = smoothstep(0.93, 1.0, np.abs(np.sin((np.abs(vn) * 1.6 - un) * 7.0))) * (np.abs(vn) < 0.8) * 0.4
    col = col * (0.9 + 0.12 * (1 - np.abs(vn)))[..., None]
    col = col * (1 + 0.22 * rib[..., None] + 0.07 * veins[..., None])
    col = col * (0.88 + 0.12 * (un[..., None] + 1) / 2)
    col = np.where(twig_m[..., None], np.array([0.16, 0.12, 0.08]), col)
    # Downsample to final size.
    col = downsample(col, ss)
    Hd = downsample(Hn, ss) / ss
    leafd = downsample(leaf.astype(float), ss)
    depthd = downsample(depth, ss)
    gx = downsample(np.where(leaf, GX, 0), ss)
    gy = downsample(np.where(leaf, GY, 0), ss)
    ribd = downsample(rib, ss)
    # Shadowing: deep leaves and gaps get darker; cavity AO from the z-buffer.
    hz = np.clip(Hd, -10, 70)
    cav = np.clip((gblur(hz, 6) - hz) / 18, 0, 1)
    shade = (0.3 + 0.7 * depthd ** 0.7) * (1 - 0.6 * cav)
    fine = fbm(S, 128, 2)
    bc = col * shade[..., None] * (0.94 + 0.12 * fine[..., None])
    bc = lerp(color([0.025, 0.035, 0.02]), bc, np.clip(leafd + downsample(twig_m.astype(float), ss) * 0.6, 0, 1))
    n_leaf = normal_from_gradient(gx, gy, 1.0)
    n_detail = normal_from_height(ribd * 0.6 + fine * 0.1, 2.0)
    normal = combine_normals(n_leaf, n_detail)
    ao = np.clip((0.35 + 0.65 * depthd) * (1 - 0.7 * cav), 0, 1)
    ao = lerp(np.full((S, S), 0.15), ao, leafd)
    rough = lerp(np.full((S, S), 0.9), 0.55 + 0.1 * fine - 0.05 * ribd, leafd)
    finish('T_Foliage', bc, Hd, rough, ao=ao, normal=normal)


def bark():
    """0.67 m tile: rough tree bark, deep meandering vertical furrows (V = up the trunk), lichen."""
    seed(207)
    yy, xx = grid()
    wx = (fbm(S, 3, 3) - 0.5) * 50
    wy = (fbm(S, 4, 3) - 0.5) * 20

    def aniso(py, px, octaves=4, pers=0.5):
        out = np.zeros((S, S))
        amp, tot = 1.0, 0.0
        for o in range(octaves):
            out += value_noise_aniso(S, min(py * 2 ** o, S), min(px * 2 ** o, S)) * amp
            tot += amp
            amp *= pers
        return out / tot

    def warped(a):
        return a[((yy + wy).astype(int)) % S, ((xx + wx).astype(int)) % S]

    # Interlacing ridges: contour levels of strongly stretched noise -> braided vertical furrows.
    # Distance to the nearest contour is converted to pixels with the gradient, so furrows keep
    # a constant width while the ridges between them swell and pinch.
    n1 = aniso(2, 24, 3, 0.45)
    n1 = n1[yy.astype(int) % S, ((xx + wx * 0.8).astype(int)) % S]
    bands = n1 * 5.0
    gyb = (np.roll(bands, -1, 0) - np.roll(bands, 1, 0)) * 0.5
    gxb = (np.roll(bands, -1, 1) - np.roll(bands, 1, 1)) * 0.5
    grad = np.sqrt(gxb ** 2 + gyb ** 2) + 1e-4
    f = bands % 1.0
    dist = np.minimum(f, 1 - f) / grad                        # pixels to the furrow centre
    width_var = 0.7 + 0.6 * fbm(S, 8, 3)
    furrow = 1 - smoothstep(0.0, 5.0 * width_var, dist)
    plate = smoothstep(1.0, 15.0 * width_var, dist) ** 0.6
    hb = value_noise_aniso(S, 48, 3)
    hb = hb[((yy + wy * 0.3).astype(int)) % S, xx.astype(int)]
    breaks = (smoothstep(0.93, 0.985, 1 - np.abs(hb * 2 - 1)) * smoothstep(0.66, 0.74, fbm(S, 40, 3))
              * smoothstep(0.4, 0.8, plate))
    fibres = value_noise_aniso(S, 40, 512) * 0.5 + value_noise_aniso(S, 80, 512) * 0.3 + fbm(S, 128, 2) * 0.2
    coarse = value_noise_aniso(S, 16, 96) * 0.6 + value_noise_aniso(S, 32, 192) * 0.4
    lumps = fbm(S, 48, 3)
    pit_d = cells(S, 120)[0]
    pits = (1 - smoothstep(0.05, 0.22, pit_d)) * smoothstep(0.6, 0.7, fbm(S, 12, 3)) * plate
    sc_tone = fbm(S, 16, 3)
    flakes = smoothstep(0.58, 0.72, fbm(S, 20, 4)) * plate
    lichen = smoothstep(0.62, 0.72, fbm(S, 4, 5) + 0.12 * (fbm(S, 64, 3) - 0.5)) * smoothstep(0.5, 0.9, plate)
    moss = smoothstep(0.62, 0.74, fbm(S, 5, 4)) * (1 - smoothstep(0.0, 3.0, dist)) * 0.6
    tone = fbm(S, 6, 5)
    surf = coarse * 0.45 + fibres * 0.35 + lumps * 0.2 - pits * 0.4
    plate_h = plate * (0.75 + 0.35 * surf)

    top = lerp(color([0.34, 0.31, 0.27]), color([0.50, 0.47, 0.43]), np.clip(tone * 0.45 + coarse * 0.35 + sc_tone * 0.2, 0, 1))
    deep = color([0.07, 0.055, 0.04])
    inner = color([0.30, 0.19, 0.12])
    bc = lerp(deep, top, np.clip(plate, 0, 1) ** 0.7)
    bc = bc * (0.72 + 0.45 * surf[..., None])
    bc = lerp(bc, inner * (0.8 + 0.4 * fibres[..., None]), breaks * 0.4)
    bc = lerp(bc, color([0.12, 0.10, 0.08]), pits * 0.35)
    bc = lerp(bc, np.minimum(bc * 1.2, 1), flakes * 0.35)
    bc = lerp(bc, color([0.55, 0.57, 0.50]) * (0.8 + 0.4 * fbm(S, 128, 2)[..., None]), lichen * 0.65)
    bc = lerp(bc, color([0.15, 0.19, 0.08]), moss)
    bc = lerp(bc, color([0.07, 0.05, 0.04]), breaks * 0.75)
    height = plate_h * 1.0 + flakes * 0.05 - breaks * 0.4 + tone * 0.12
    rough = 0.9 + 0.05 * furrow - 0.04 * lichen
    finish('T_Bark', bc, height, rough, normal_strength=7.0, ao_radius=6, ao_strength=1.6)


# =======================================================================================
# Exteriors: roof, siding, stone, painted trim

def roof_shingles():
    """1.25 m tile: 3-tab asphalt shingles, 9 courses (~14 cm exposure), 4 tabs (~31 cm) across.
    Image top = up the slope; each tab's butt edge is at the bottom of its course."""
    seed(208)
    rnd = R(208)
    yy, xx = grid()
    rows = 9
    rh = S / rows
    tw = S / 4
    row = np.floor(yy / rh).astype(int) % rows
    t = (yy % rh) / rh                               # 0 = top of exposure, 1 = butt edge
    offsets = np.array([0.0, 0.5, 0.0, 0.5, 0.17, 0.67, 0.0, 0.5, 0.25]) + rnd.uniform(-0.04, 0.04, rows)
    edge_wob = (fbm(S, 16, 3) - 0.5) * 2.5           # butt edges are not perfectly straight
    ux = (xx / tw + offsets[row]) % 4.0
    tab = np.floor(ux).astype(int)
    fu = ux % 1.0
    tab_id = row * 4 + tab
    slot_px = np.minimum(fu, 1 - fu) * tw
    slot = 1 - smoothstep(1.5, 3.2, slot_px + (fbm(S, 32, 2) - 0.5) * 1.5)
    rtone = R(31).normal(0, 1, rows * 4)
    tab_tone = 0.035 * rtone[tab_id]
    replaced = R(32).random(rows * 4) < 0.07                # lighter replacement tabs
    missing_tab = np.zeros(rows * 4, bool)
    missing_tab[[3 * 4 + 2, 7 * 4 + 0]] = True             # two blown-off tabs
    rep = replaced[tab_id].astype(float)
    miss = missing_tab[tab_id].astype(float)
    tb = np.clip(t + edge_wob / rh, 0, 1)
    shadow = 1 - smoothstep(0.0, 0.16, tb)                 # cast by the course above's butt edge
    butt = smoothstep(0.9, 1.0, tb)

    # Granular surface: dense multicoloured granules.
    g1 = R(33).random((S, S))
    gran = gblur(g1, 0.6)
    gran = (gran - gran.mean()) / (gran.std() + 1e-9)
    gcol = R(34).random((S, S))
    granule_rgb = np.where((gcol < 0.08)[..., None], color([0.55, 0.52, 0.48]),
                           np.where((gcol < 0.2)[..., None], color([0.20, 0.19, 0.18]), color([0.36, 0.35, 0.34])))
    granule_rgb = gblur(granule_rgb, 0.5)
    loss = smoothstep(0.62, 0.75, fbm(S, 8, 5)) * (0.5 + 0.5 * butt)       # granule loss -> black asphalt
    algae = gblur(np.clip(downward_streaks(0.8, (80, 400), (4, 16), R(35)) * 1.2, 0, 1), 2)
    dirt_ = stains(S, 0.3, 4, 4, 0.2)
    moss_seed = smoothstep(0.62, 0.72, fbm(S, 10, 4)) * (smoothstep(0.65, 1.0, tb) + slot * 0.8)
    moss_tufts = (1 - smoothstep(0.1, 0.5, cells(S, 160)[0])) * moss_seed
    moss = np.clip(moss_seed * 0.6 + moss_tufts, 0, 1) * (1 - miss)

    base = 0.42 + tab_tone + 0.06 * rep
    bc = granule_rgb * (base / 0.36)[..., None]
    bc = bc * (0.92 + 0.08 * (fbm(S, 4, 4)[..., None] - 0.5))
    bc = lerp(bc, color([0.11, 0.105, 0.10]), loss * 0.75)
    bc = lerp(bc, bc * color([0.55, 0.55, 0.52]), algae * 0.6)
    bc = lerp(bc, bc * color([0.85, 0.80, 0.72]), dirt_ * 0.4)
    bc = bc * (1 - 0.35 * shadow[..., None])
    bc = lerp(bc, bc * 1.12, butt * 0.4)                  # worn, lighter butt edges
    # Missing tabs: the course below (headlap) shows, with nails and the tar seal strip.
    nails = np.zeros((S, S))
    for k in range(4):
        nails = np.maximum(nails, 1 - smoothstep(2.0, 3.2, np.sqrt((fu * tw - (k + 0.5) * tw / 4) ** 2 + ((t - 0.62) * rh) ** 2)))
    seal = 1 - smoothstep(0.03, 0.05, np.abs(t - 0.8 + (fbm(S, 64, 2) - 0.5) * 0.04))
    hole_col = granule_rgb * (0.55 / 0.36) * 0.62 * (1 - 0.3 * (1 - smoothstep(0.0, 0.3, t)))[..., None]
    hole_col = lerp(hole_col, color([0.035, 0.035, 0.035]), seal * 0.85)
    hole_col = lerp(hole_col, color([0.45, 0.45, 0.46]), nails)
    bc = lerp(bc, hole_col, miss)
    bc = lerp(bc, color([0.06, 0.06, 0.06]), slot * (1 - miss))
    bc = lerp(bc, color([0.20, 0.27, 0.09]) * (0.75 + 0.5 * fbm(S, 64, 3)[..., None]), moss * 0.85)

    height = (tb * 0.55 - butt * 0.15 + gran * 0.025 - loss * 0.04 - slot * 0.5
              - miss * 0.45 + nails * miss * 0.05 + moss_tufts * 0.12)
    rough = 0.9 - 0.05 * loss + 0.03 * moss
    ao_mul = 1 - 0.45 * shadow * (1 - miss) - 0.4 * slot
    finish('T_Roof_Shingles', bc, height, rough, normal_strength=5.0, ao_radius=4, ao_strength=1.0, ao_mul=ao_mul)


def _clapboard(name, worn, seed_value):
    """2 m tile of horizontal clapboard (13 boards, ~15 cm exposure), image top = up the wall."""
    seed(seed_value)
    rnd = R(seed_value)
    yy, xx = grid()
    rows = 13
    rh = S / rows
    row = np.floor(yy / rh).astype(int) % rows
    t = (yy % rh) / rh                                    # 0 = top of exposed face, 1 = butt (lower) edge
    joint_x = np.array([rnd.uniform(0, S) if rnd.random() < 0.55 else -1.0 for _ in range(rows)])
    jx = joint_x[row]
    has_joint = jx >= 0
    dj = np.where(has_joint, np.abs(((xx - jx + S / 2) % S) - S / 2), 999.0)
    side = np.where(has_joint, ((xx - jx) % S) < S / 2, False)
    board = row * 2 + side.astype(int)
    joint = 1 - smoothstep(0.6, 1.8, dj)
    # Grain: one large field, sampled at a different offset for every board.
    rings, fibres, pores = wood_grain(S, 8, 2, 10.0, 200)
    offy = R(seed_value + 1).integers(0, S, rows * 2)[board]
    offx = R(seed_value + 2).integers(0, S, rows * 2)[board]
    yi = (yy.astype(int) + offy) % S
    xi = (xx.astype(int) + offx) % S
    rings = rings[yi, xi]
    fibres = fibres[yi, xi]
    btone = R(seed_value + 3).normal(0, 1, rows * 2)[board]

    shadow = 1 - smoothstep(0.0, 0.14, t)                # under the board above
    butt = smoothstep(0.86, 1.0, t)
    profile = t ** 0.85 - smoothstep(0.95, 1.0, t) * 0.12   # wedge: thick at the butt
    # Nails: one per stud (40 cm) just above each butt edge.
    nail = np.zeros((S, S))
    rust = np.zeros((S, S))
    nrnd = R(seed_value + 4)
    cvr = WrapCanvas(S, ss=2)
    for r in range(rows):
        for k in range(5):
            nx_ = (k + 0.5) * S / 5 + nrnd.normal(0, 2.5)
            ny_ = (r + 0.86) * rh + nrnd.normal(0, 1.0)
            d = np.sqrt(((xx - nx_ + S / 2) % S - S / 2) ** 2 + ((yy - ny_ + S / 2) % S - S / 2) ** 2)
            nail = np.maximum(nail, 1 - smoothstep(1.4, 2.6, d))
            if nrnd.random() < (0.45 if worn else 0.3):
                ln = nrnd.uniform(8, 30) * (2.2 if worn else 1)
                cvr.line([(nx_, ny_), (nx_ + nrnd.normal(0, 1), ny_ + ln)], fill=float(nrnd.uniform(0.4, 1.0)),
                         width=nrnd.uniform(1.5, 3.5))
    rust = gblur(cvr.array(), 1.0)
    rust = np.clip(rust * (1 - 0.6 * np.clip((yy % rh) / rh - 0.9, 0, 1)), 0, 1)
    streaks = downward_streaks(0.9 if worn else 0.5, (60, 380), (3, 14), R(seed_value + 5), softness=2.5)
    grime = stains(S, 0.3, 3, 5, 0.25)
    fine = fbm(S, 128, 2)

    if not worn:
        brush = value_noise_aniso(S, 300, 10) * 0.6 + value_noise_aniso(S, 600, 20) * 0.4
        crack_mask = smoothstep(0.6, 0.75, fbm(S, 5, 4))
        alligator = (1 - smoothstep(0.0, 0.05, cells(S, 60, 120)[1] - cells(S, 60, 120)[0])) * crack_mask * 0.5
        peel_n = value_noise_aniso(S, 96, 12) * 0.5 + fbm(S, 24, 4) * 0.5
        peel = smoothstep(0.66, 0.69, peel_n + butt * 0.3 - 0.1) * smoothstep(0.55, 0.7, fbm(S, 4, 4))
        peel_edge = np.clip(gblur(peel, 1.0) - peel, 0, 1) * 3
        paint = color([0.80, 0.79, 0.76]) * (0.97 + 0.04 * btone[..., None] * 0.5)
        wood = lerp(color([0.42, 0.40, 0.36]), color([0.55, 0.52, 0.47]), fibres)
        bc = paint * (0.95 + 0.015 * (rings[..., None] - 0.3) + 0.03 * (fibres[..., None] - 0.5) + 0.04 * (brush[..., None] - 0.5) + 0.03 * fine[..., None])
        bc = lerp(bc, wood * (0.85 + 0.3 * fibres[..., None]), peel)
        bc = lerp(bc, bc * 0.8, peel_edge[..., None] * (1 - peel[..., None]) * 0.5)
        bc = lerp(bc, bc * 0.7, alligator[..., None] * 0.6)
        bc = bc * (1 - 0.25 * shadow[..., None])
        bc = lerp(bc, bc * color([0.62, 0.62, 0.56]), streaks * 0.5)
        bc = lerp(bc, bc * color([0.82, 0.80, 0.74]), grime * 0.45)
        bc = lerp(bc, bc * color([0.55, 0.40, 0.25]), rust * 0.55)
        bc = lerp(bc, color([0.30, 0.29, 0.27]), joint * 0.8)
        bc = lerp(bc, bc * 0.8, nail * 0.5)
        height = (profile * 1.0 + rings * 0.01 + fibres * 0.03 + (brush - 0.5) * 0.02 - peel * 0.05 + peel_edge * 0.03
                  - joint * 0.3 + nail * 0.04 - alligator * 0.03)
        rough = 0.62 + 0.06 * grime + 0.2 * peel + 0.08 * streaks + 0.05 * (fine - 0.5)
        ao_mul = 1 - 0.4 * shadow
        finish(name, bc, height, rough, normal_strength=4.0, ao_radius=5, ao_strength=1.0, ao_mul=ao_mul)
        return

    # Neglected: chalky grey-green paint peeling in long strips along the grain, grey wood, rot.
    peel_n = value_noise_aniso(S, 64, 6) * 0.45 + value_noise_aniso(S, 160, 20) * 0.25 + fbm(S, 12, 4) * 0.3
    big_peel = smoothstep(0.55, 0.57, peel_n + butt * 0.15 + 0.1 * (fbm(S, 3, 3) - 0.5) + (fbm(S, 64, 3) - 0.5) * 0.12)
    # Alligatored paint: grain-aligned flakes (~2 x 6 cm); a damage field decides which fell off.
    fl_f1, fl_f2, fl_id = cells(S, 96, 30, warp_y=(fbm(S, 16, 3) - 0.5) * 6, warp_x=(fbm(S, 16, 3) - 0.5) * 6)
    damage = np.clip(fbm(S, 5, 4) * 0.9 + butt * 0.35 - 0.18, 0, 1)
    flake_gone = (R(seed_value + 7).random(96 * 30)[fl_id] < damage ** 1.6).astype(float)
    flake_crack = (1 - smoothstep(0.02, 0.07, fl_f2 - fl_f1)) * smoothstep(0.25, 0.45, damage)
    peel = np.clip(np.maximum(big_peel, flake_gone), 0, 1)
    peel = np.clip(gblur(peel, 0.6) * 1.2, 0, 1)
    peel_edge = np.clip(gblur(peel, 1.2) - peel, 0, 1) * 3
    checks = smoothstep(0.86, 0.97, 1 - np.abs(value_noise_aniso(S, 140, 5) * 2 - 1)) * peel
    rot_line = 1 - np.clip((fbm(S, 10, 4) - 0.58) * 2.2 + (fbm(S, 48, 3) - 0.5) * 0.25, 0, 0.5)
    rot_row = (R(seed_value + 6).random(rows)[row] < 0.35)
    rot_amt = np.where(rot_row, 1 - rot_line, 0.0)
    hole = (smoothstep(-0.02, 0.0, t - (1 - rot_amt * 0.5)) * (rot_amt > 0.12)).astype(float)
    punky = np.clip(smoothstep(0.0, 0.2, rot_amt) * smoothstep(1 - rot_amt * 1.4 - 0.1, 1.0, t), 0, 1) * (1 - hole)
    water = stains(S, 0.3, 4, 5, 0.06)
    tide = np.clip(gblur(water, 2) - water, 0, 1) * 3
    mildew = smoothstep(0.55, 0.7, fbm(S, 10, 4)) * (shadow * 0.7 + butt * 0.5)

    chalk = fbm(S, 6, 4)
    paint = lerp(color([0.44, 0.49, 0.44]), color([0.55, 0.58, 0.54]), chalk) * (0.95 + 0.06 * btone[..., None] * 0.5)
    paint = paint * (0.9 + 0.12 * fbm(S, 24, 3)[..., None])
    wood = lerp(color([0.33, 0.33, 0.31]), color([0.58, 0.57, 0.54]), np.clip(fibres * 0.9 + 0.2 * fine, 0, 1))
    wood = lerp(wood, color([0.26, 0.25, 0.24]), rings * 0.3)
    bc = lerp(paint, wood, peel)
    bc = lerp(bc, color([0.62, 0.66, 0.60]), peel_edge * 0.5 * (1 - peel))
    bc = lerp(bc, color([0.16, 0.16, 0.14]), flake_crack * (1 - peel) * 0.8)
    bc = lerp(bc, color([0.12, 0.11, 0.10]), checks * 0.8)
    bc = lerp(bc, bc * color([0.66, 0.60, 0.52]), water * 0.4)
    bc = lerp(bc, bc * 0.7, tide * 0.4)
    bc = bc * (1 - 0.3 * shadow[..., None])
    bc = lerp(bc, bc * color([0.55, 0.55, 0.50]), streaks * 0.5)
    bc = lerp(bc, bc * color([0.75, 0.73, 0.66]), grime * 0.5)
    bc = lerp(bc, color([0.20, 0.22, 0.12]), mildew * 0.6)
    bc = lerp(bc, color([0.26, 0.16, 0.10]) * (0.7 + 0.5 * fibres[..., None]), punky * 0.85)
    bc = lerp(bc, bc * color([0.62, 0.42, 0.26]), rust * 0.6)
    bc = lerp(bc, color([0.28, 0.17, 0.10]), nail * 0.9)
    bc = lerp(bc, color([0.07, 0.05, 0.04]) * (0.7 + 0.6 * fibres[..., None]), hole)
    bc = lerp(bc, color([0.10, 0.09, 0.08]), joint * 0.9)
    height = (profile * 1.0 + rings * 0.05 * peel + fibres * 0.06 + peel_edge * 0.05 * (1 - peel) - peel * 0.04
              - flake_crack * (1 - peel) * 0.04
              - checks * 0.12 - punky * 0.15 - hole * 0.6 - joint * 0.4 + nail * 0.03 + rings * 0.04 * peel)
    rough = 0.82 + 0.08 * peel + 0.06 * grime - 0.1 * water * (1 - peel) + 0.05 * punky
    ao_mul = (1 - 0.45 * shadow) * (1 - 0.6 * hole)
    finish(name, bc, height, rough, normal_strength=4.5, ao_radius=5, ao_strength=1.2, ao_mul=ao_mul)


def siding_wood():
    _clapboard('T_Siding_Wood', False, 209)


def siding_worn():
    _clapboard('T_Siding_Worn', True, 210)


def stone_wall():
    """2 m tile: dressed grey stone, irregular courses (26-40 cm) of 30-75 cm blocks, lime mortar."""
    seed(211)
    rnd = R(211)
    yy, xx = grid()
    ppm = S / 2.0
    course_h = np.array([0.30, 0.38, 0.26, 0.34, 0.40, 0.32])
    edges_y = np.concatenate([[0], np.cumsum(course_h)]) * ppm
    wy = (fbm(S, 8, 3) - 0.5) * 5
    wx = (fbm(S, 8, 3) - 0.5) * 5
    Y = (yy + wy) % S
    X = xx + wx
    course = np.clip(np.searchsorted(edges_y, Y, side='right') - 1, 0, len(course_h) - 1)
    cy0 = edges_y[course]
    cy1 = edges_y[course + 1]
    block_id = np.zeros((S, S), int)
    dxe = np.zeros((S, S))
    bx0_map = np.zeros((S, S))
    bw_map = np.zeros((S, S))
    nid = 0
    for c in range(len(course_h)):
        lens = []
        while sum(lens) < 2.0:
            lens.append(rnd.uniform(0.32, 0.75))
        lens = np.array(lens) * 2.0 / sum(lens)
        starts = (np.concatenate([[0], np.cumsum(lens)[:-1]]) + rnd.uniform(0, 2.0)) * ppm
        sel = course == c
        xs = X[sel]
        best_d = np.full(xs.shape, 1e9)
        best = np.zeros(xs.shape, int)
        x0s = np.zeros(xs.shape)
        ws = np.zeros(xs.shape)
        for b, (st, ln) in enumerate(zip(starts, lens * ppm)):
            rel = (xs - st) % S
            inside = rel < ln
            dist = np.minimum(rel, ln - rel)
            upd = inside
            best = np.where(upd, nid + b, best)
            best_d = np.where(upd, dist, best_d)
            x0s = np.where(upd, rel, x0s)
            ws = np.where(upd, ln, ws)
        block_id[sel] = best
        dxe[sel] = best_d
        bx0_map[sel] = x0s
        bw_map[sel] = ws
        nid += len(lens)
    dye = np.minimum(Y - cy0, cy1 - Y)
    # Rounded / chipped arrises.
    chip_n = fbm(S, 24, 4)
    edge_d = np.minimum(dxe, dye) - (chip_n - 0.5) * 6 - smoothstep(0.7, 0.85, fbm(S, 32, 3)) * 5
    mortar = 1 - smoothstep(3.0, 4.5, edge_d)
    bevel = smoothstep(3.5, 16, edge_d)
    rnd_b = R(212).random((nid, 4))
    bt = rnd_b[block_id]
    # Block face: slightly domed, pocked by the dressing tool.
    u = bx0_map / np.maximum(bw_map, 1)
    v = (Y - cy0) / np.maximum(cy1 - cy0, 1)
    dome = (1 - (2 * u - 1) ** 2) * (1 - (2 * v - 1) ** 2)
    pock_d, _, pock_id = cells(S, 170)
    pocks = (1 - smoothstep(0.04, 0.22, pock_d)) * (R(214).random(170 * 170)[pock_id] < 0.12) * 0.6
    surf = fbm(S, 12, 5) * 0.55 + fbm(S, 48, 3) * 0.3 + fbm(S, 160, 2) * 0.15
    pits = smoothstep(0.78, 0.86, fbm(S, 128, 2)) * 0.6
    lichen_mask = smoothstep(0.6, 0.72, fbm(S, 5, 5)) * (1 - mortar)
    lichen_dots = (1 - smoothstep(0.15, 0.5, cells(S, 110)[0])) * smoothstep(0.5, 0.6, fbm(S, 8, 4))
    lichen = np.clip(lichen_mask * (0.5 + 0.5 * fbm(S, 64, 3)) + lichen_dots * lichen_mask * 0.8, 0, 1)
    lichen_y = (1 - smoothstep(0.1, 0.4, cells(S, 200)[0])) * smoothstep(0.66, 0.74, fbm(S, 6, 4)) * (1 - mortar)
    streaks = downward_streaks(1.0, (80, 420), (6, 26), R(213), softness=5)
    soot = stains(S, 0.35, 3, 5, 0.25)

    grey = 0.45 + 0.08 * (bt[..., 2] - 0.5)
    warm = 0.01 + bt[..., 3] * 0.025
    stone = np.stack([grey + warm, grey + warm * 0.5, grey - warm * 0.3], -1)
    stone = stone * (0.84 + 0.26 * surf[..., None]) * (1 - 0.12 * pocks[..., None])
    stone = lerp(stone, stone * 0.7, pits * 0.5)
    stone = lerp(stone, stone * 1.1, (1 - bevel) * (1 - mortar) * 0.4)
    mort = color([0.56, 0.54, 0.50]) * (0.82 + 0.3 * fbm(S, 96, 2)[..., None])
    bc = lerp(stone, mort, mortar)
    bc = lerp(bc, bc * color([0.62, 0.62, 0.60]), streaks * 0.45)
    bc = lerp(bc, bc * color([0.70, 0.70, 0.66]), soot * 0.55)
    bc = lerp(bc, color([0.60, 0.62, 0.55]), lichen * 0.55)
    bc = lerp(bc, color([0.58, 0.50, 0.24]), lichen_y * 0.6)
    bc = lerp(bc, bc * color([0.62, 0.64, 0.56]), (1 - smoothstep(2.0, 6.0, edge_d)) * smoothstep(0.5, 0.7, fbm(S, 6, 3)) * 0.35)
    height = (bevel * 0.8 + dome * 0.15 + surf * 0.16 - pocks * 0.06 - pits * 0.05 + lichen * 0.03
              - mortar * 0.25 + fbm(S, 96, 2) * 0.03 * mortar)
    rough = 0.84 + 0.06 * mortar - 0.05 * streaks + 0.04 * lichen
    finish('T_Stone_Wall', bc, height, rough, normal_strength=5.0, ao_radius=6, ao_strength=1.6)


def paint_wood():
    """1 m tile: near-white enamel over wood (grain along U), brush marks, chips through to an
    older paint layer and bare wood. Tinted per material instance."""
    seed(214)
    rnd = R(214)
    rings, fibres, pores = wood_grain(S, 6, 2, 9.0, 160)
    brush = value_noise_aniso(S, 420, 8) * 0.5 + value_noise_aniso(S, 840, 16) * 0.3 + value_noise_aniso(S, 200, 4) * 0.2
    sag = smoothstep(0.7, 0.85, value_noise_aniso(S, 6, 40)) * 0.3
    patches = smoothstep(0.55, 0.68, fbm(S, 4, 4))
    chip_n = warp(fbm(S, 20, 5), 0.015, 8)
    chips_old = smoothstep(0.66, 0.69, chip_n) * patches
    chips_wood = smoothstep(0.71, 0.73, chip_n) * patches
    lip = np.clip(gblur(chips_old, 1.0) - chips_old, 0, 1) * 3
    hair = smoothstep(0.9, 0.98, 1 - np.abs(value_noise_aniso(S, 110, 5) * 2 - 1)) * smoothstep(0.55, 0.7, fbm(S, 6, 4))
    grime = stains(S, 0.3, 3, 5, 0.25)
    dings = speckle(0.0006, 1.2, rnd)
    fine = fbm(S, 128, 2)

    paint = color([0.83, 0.82, 0.79])
    old_paint = color([0.58, 0.62, 0.55])
    wood = lerp(color([0.33, 0.22, 0.13]), color([0.52, 0.38, 0.24]), np.clip(fibres * 0.6 + (1 - rings) * 0.4, 0, 1))
    bc = paint * (0.95 + 0.04 * (brush[..., None] - 0.5) + 0.012 * (rings[..., None] - 0.3) + 0.02 * fine[..., None])
    bc = lerp(bc, bc * color([0.82, 0.80, 0.74]), grime * 0.4)
    bc = lerp(bc, bc * 0.85, (hair * 0.6)[..., None] * grime[..., None])
    bc = lerp(bc, old_paint * (0.9 + 0.1 * fine[..., None]), chips_old)
    bc = lerp(bc, wood, chips_wood)
    bc = lerp(bc, bc * 0.75, dings[..., None] * 0.6)
    height = (rings * 0.015 + fibres * 0.04 + (brush - 0.5) * 0.06 + sag * 0.05 - chips_old * 0.12 - chips_wood * 0.1
              + lip * 0.05 - hair * 0.05 - dings * 0.15)
    rough = 0.42 + 0.06 * (brush - 0.5) + 0.12 * grime + 0.18 * chips_old + 0.3 * chips_wood + 0.05 * hair
    finish('T_Paint_Wood', bc, height, rough, normal_strength=3.0, ao_radius=4, ao_strength=1.0)


# =======================================================================================
# Interiors: wallpaper

def _rgb(c):
    return tuple(int(round(v * 255)) for v in c)


def _leaf_pts(x, y, ang, length, width, n=16, curl=0.0):
    """Pointed leaf outline from (x, y) along `ang`; curl bends it sideways towards the tip."""
    pts_l, pts_r = [], []
    for k in range(n + 1):
        t = k / n
        u = t * length
        w = math.sin(math.pi * t) ** 0.75 * width * (1 - 0.35 * t)
        off = curl * length * t * t
        pts_l.append((u, w + off))
        pts_r.append((u, -w + off))
    pts = pts_l + pts_r[::-1]
    ca, sa = math.cos(ang), math.sin(ang)
    return [(x + u * ca - v * sa, y + u * sa + v * ca) for u, v in pts]


def _arc_pts(cx, cy, r, a0, a1, n=16, squash=1.0):
    return [(cx + math.cos(a0 + (a1 - a0) * k / n) * r, cy + math.sin(a0 + (a1 - a0) * k / n) * r * squash)
            for k in range(n + 1)]


def _paper(name, pattern, seed_value, ground, rough_ground=0.82, rough_pattern=None, motif=None, shift=3,
           stain_amount=0.5, peel=0.0, fade=0.15, emboss=0.0):
    """Shared wallpaper finish: rolls 50 cm wide (seams at x = 0 / 512, the right strip hung a few
    pixels lower), paper fibre, uneven fading, faint dirt, water stains near the bottom of the tile."""
    seed(seed_value)
    yy, xx = grid()
    pat = pattern.copy()
    mot = motif.copy() if motif is not None else np.zeros((S, S))
    pat[:, 512:] = np.roll(pat[:, 512:], shift, axis=0)
    mot[:, 512:] = np.roll(mot[:, 512:], shift, axis=0)
    fibre = fbm(S, 256, 2) * 0.6 + value_noise_aniso(S, 512, 128) * 0.4
    sx = np.minimum(np.abs(xx - 0.5), np.minimum(np.abs(xx - 512.5), np.abs(xx - S + 0.5)))
    seam = 1 - smoothstep(0.4, 1.4, sx)
    seam_lift = (1 - smoothstep(0.5, 6.0, sx)) * smoothstep(0.55, 0.75, fbm(S, 8, 3))
    faded = fbm(S, 2, 4)
    dirt_ = stains(S, 0.3, 3, 5, 0.25)
    # Water stains: lower part of the tile, with darker tide-mark rims.
    low = smoothstep(0.6, 1.0, yy / S)
    wet = smoothstep(0.66, 0.7, fbm(S, 4, 5) * 0.85 + low * 0.22 + (fbm(S, 24, 3) - 0.5) * 0.08) * stain_amount
    tide = np.clip(gblur(wet, 2.5) - wet, 0, 1) * 4 * stain_amount
    spots = (1 - smoothstep(0.05, 0.25, cells(S, 120)[0])) * smoothstep(0.62, 0.7, fbm(S, 6, 3)) * 0.5
    grey = pat.mean(-1, keepdims=True)
    bc = lerp(pat, grey * color([1.02, 1.0, 0.92]), fade * (0.6 + 0.8 * faded))           # faded / yellowed
    bc = bc * (0.96 + 0.06 * fibre[..., None]) * (0.95 + 0.08 * (faded[..., None] - 0.5))
    bc = lerp(bc, bc * color([0.84, 0.80, 0.72]), dirt_ * 0.35)
    bc = lerp(bc, bc * color([0.78, 0.68, 0.52]), wet * 0.6)
    bc = lerp(bc, bc * color([0.62, 0.52, 0.40]), tide * 0.6)
    bc = lerp(bc, bc * color([0.60, 0.56, 0.48]), spots * stain_amount)
    bc = lerp(bc, bc * 0.72, seam * 0.6)
    height = fibre * 0.05 + mot * emboss + seam_lift * 0.08 - seam * 0.1
    if peel > 0:
        # Seams lifting: the paper edge curls away showing pale backing and a dark gap.
        curl = (1 - smoothstep(0.5, 10.0 + 8 * fbm(S, 6, 3), sx)) * smoothstep(0.5, 0.62, fbm(S, 5, 4)) * peel
        backing = curl * smoothstep(0.0, 3.0, sx) * (1 - smoothstep(4.0, 9.0, sx))
        gap = curl * (1 - smoothstep(0.5, 2.5, sx))
        bc = lerp(bc, color([0.62, 0.58, 0.48]), backing * 0.8)
        bc = lerp(bc, color([0.05, 0.04, 0.035]), gap)
        height = height + curl * 0.3 - gap * 0.4
    rg = np.full((S, S), rough_ground)
    if rough_pattern is not None:
        rg = lerp(rg, np.full((S, S), rough_pattern), mot)
    rough = rg + 0.04 * (fibre - 0.5) + 0.05 * dirt_ - 0.08 * wet
    finish(name, bc, height, rough, normal_strength=2.0, ao_radius=4, ao_strength=1.0)


def wallpaper_floral():
    """1 m tile: vintage trailing roses on a cream ground with sage leaves; four vine columns."""
    rnd = R(215)
    ground = color([0.80, 0.76, 0.64])
    cv = WrapCanvas(S, ss=2, mode='RGB', fill=_rgb(ground))
    # Faint ground texture: tiny sprigs / dots between columns.
    for _ in range(420):
        x, y = rnd.uniform(0, S, 2)
        cv.ellipse(x, y, 1.3, 1.3, _rgb(lerp(ground, color([0.60, 0.62, 0.50]), 0.6)))
    stem_c = _rgb(color([0.48, 0.50, 0.38]))
    leaf_cs = [_rgb(color([0.50, 0.57, 0.43])), _rgb(color([0.41, 0.48, 0.36])), _rgb(color([0.56, 0.61, 0.48]))]
    vein_c = _rgb(color([0.66, 0.70, 0.56]))
    rose_light = color([0.82, 0.60, 0.57])
    rose_mid = color([0.72, 0.47, 0.46])
    rose_dark = color([0.55, 0.32, 0.33])
    period = 256.0
    for col in range(4):
        cx = 128 + col * 256
        phase = 0.0 if col % 2 == 0 else 0.5
        pts = []
        for k in range(0, 129):
            y = k / 128 * S
            pts.append((cx + math.sin((y / period + phase) * 2 * np.pi) * 34, y))
        cv.line(pts, fill=stem_c, width=2.2)
        for rep in range(4):
            for side in (0, 1):
                y0 = (rep + phase + 0.25 + 0.5 * side) * period
                x0 = cx + math.sin((y0 / period + phase) * 2 * np.pi) * 34
                sgn = 1 if side == 0 else -1
                # Short side stem with leaves and a rose at its end.
                ex, ey = x0 + sgn * 42, y0 - 30
                cv.line([(x0, y0), (x0 + sgn * 20, y0 - 22), (ex, ey)], fill=stem_c, width=1.8)
                for (lx, ly, la) in ((x0 + sgn * 14, y0 - 12, -np.pi / 2 + sgn * 0.9), (x0 + sgn * 26, y0 - 26, -np.pi / 2 - sgn * 0.6),
                                     (x0 - sgn * 4, y0 + 24, np.pi / 2 + sgn * 0.8), (x0 + sgn * 6, y0 + 50, np.pi / 2 - sgn * 0.9)):
                    c = leaf_cs[rnd.integers(3)]
                    cv.polygon(_leaf_pts(lx, ly, la, 30, 9), fill=c)
                    cv.line([(lx, ly), (lx + math.cos(la) * 24, ly + math.sin(la) * 24)], fill=vein_c, width=0.8)
                r = 21 if side == 0 else 17
                cv.ellipse(ex, ey, r, r * 0.92, _rgb(rose_mid))
                for k in range(6):
                    a = k / 6 * 2 * np.pi + rep
                    cv.ellipse(ex + math.cos(a) * r * 0.55, ey + math.sin(a) * r * 0.5, r * 0.5, r * 0.45, _rgb(rose_light))
                cv.ellipse(ex, ey, r * 0.62, r * 0.58, _rgb(rose_mid))
                for k, rr in enumerate((0.85, 0.62, 0.42, 0.24)):
                    a0 = k * 1.9 + rep
                    cv.line(_arc_pts(ex, ey, r * rr, a0, a0 + 3.6, 14, 0.92), fill=_rgb(rose_dark), width=1.6 - 0.25 * k)
                cv.ellipse(ex, ey, r * 0.13, r * 0.12, _rgb(rose_dark * 0.85))
                # A bud further along.
                bx, by = x0 - sgn * 30, y0 + 70
                cv.line([(x0, y0 + 40), (bx, by)], fill=stem_c, width=1.4)
                cv.polygon(_leaf_pts(bx - sgn * 2, by + 10, -np.pi / 2, 16, 6), fill=_rgb(rose_mid))
                cv.polygon(_leaf_pts(bx, by + 12, -np.pi / 2 - sgn * 0.5, 13, 4), fill=leaf_cs[1])
    # Small rose sprigs between the vine columns (half-drop).
    for col in range(4):
        for rep in range(4):
            sx_ = (col * 256 + 256) % S
            sy_ = (rep + (0.5 if col % 2 else 0.0)) * period + 64
            for (la, ll) in ((-2.4, 20), (-0.7, 20), (1.9, 16)):
                cv.polygon(_leaf_pts(sx_, sy_, la, ll, 6), fill=leaf_cs[rnd.integers(3)])
            r = 10
            cv.ellipse(sx_, sy_ - 2, r, r * 0.92, _rgb(rose_mid))
            for k in range(5):
                a = k / 5 * 2 * np.pi + rep
                cv.ellipse(sx_ + math.cos(a) * r * 0.5, sy_ - 2 + math.sin(a) * r * 0.45, r * 0.48, r * 0.42, _rgb(rose_light))
            cv.ellipse(sx_, sy_ - 2, r * 0.55, r * 0.5, _rgb(rose_mid))
            cv.line(_arc_pts(sx_, sy_ - 2, r * 0.7, rep * 1.3, rep * 1.3 + 3.8, 12, 0.92), fill=_rgb(rose_dark), width=1.2)
            cv.line(_arc_pts(sx_, sy_ - 2, r * 0.38, rep * 1.3 + 2, rep * 1.3 + 5.5, 10, 0.92), fill=_rgb(rose_dark), width=1.0)
            for k in range(3):
                a = rnd.uniform(0, 2 * np.pi)
                dx_, dy_ = math.cos(a) * rnd.uniform(28, 44), math.sin(a) * rnd.uniform(40, 70)
                for q in range(5):
                    b = q / 5 * 2 * np.pi
                    cv.ellipse(sx_ + dx_ + math.cos(b) * 2.6, sy_ + dy_ + math.sin(b) * 2.6, 2.2, 2.2,
                               _rgb(color([0.66, 0.66, 0.74])))
                cv.ellipse(sx_ + dx_, sy_ + dy_, 1.3, 1.3, _rgb(color([0.80, 0.70, 0.40])))
    pat = cv.array() / 255.0
    motif = np.clip(np.abs(pat - ground).sum(-1) * 3, 0, 1)
    _paper('T_Wallpaper_Floral', pat, 215, ground, 0.84, None, motif, shift=4, stain_amount=0.3, fade=0.18, emboss=0.03)


def wallpaper_stripe():
    """1 m tile: dusty blue / cream Regency stripe (12.8 cm repeat: 6.4 cm blue band, cream band
    with two thin pinstripes), printed slightly unevenly and faded."""
    seed(216)
    yy, xx = grid()
    u = xx % 128
    blue = color([0.44, 0.53, 0.60])
    cream = color([0.82, 0.79, 0.70])
    pin = color([0.52, 0.58, 0.62])
    gold = color([0.70, 0.62, 0.45])
    wob = (fbm(S, 16, 3) - 0.5) * 0.8
    uu = u + wob
    band = smoothstep(1.0, 2.2, uu) * (1 - smoothstep(63.0, 64.2, uu))
    edge_line = (np.abs(uu - 4) < 0.9) | (np.abs(uu - 60) < 0.9)
    pins = (np.abs(uu - 86) < 1.0) | (np.abs(uu - 106) < 1.0)
    fleck = (np.abs(uu - 96) < 0.6)
    ink = 0.94 + 0.08 * fbm(S, 64, 3)
    pat = lerp(cream, blue * ink[..., None], band)
    pat = lerp(pat, pat * 0.82, edge_line.astype(float) * band)
    pat = lerp(pat, pin, pins.astype(float) * 0.85)
    pat = lerp(pat, gold, fleck.astype(float) * 0.7)
    pat = T.gblur(pat, 0.5)
    motif = band
    _paper('T_Wallpaper_Stripe', pat, 216, cream, 0.84, 0.8, motif, shift=0, stain_amount=0.25, fade=0.22, emboss=0.01)


def _damask_motif(w, h):
    """One bilaterally symmetric damask medallion (palmette, crown, pendant, acanthus C- and
    S-scrolls, ogee frame vine) on a w x h canvas; returns a 0..1 mask."""
    from PIL import Image, ImageDraw
    ss = 3
    img = Image.new('L', (w * ss, h * ss), 0)
    d = ImageDraw.Draw(img)
    cx = w / 2

    def P(pts):
        return [(x * ss, y * ss) for x, y in pts]

    def M(pts):
        return [(2 * cx - x, y) for x, y in pts]

    def shape(pts, fill=255, both=True):
        d.polygon(P(pts), fill=fill)
        if both:
            d.polygon(P(M(pts)), fill=fill)

    def stroke(pts, width, fill=255, both=True):
        d.line(P(pts), fill=fill, width=max(1, int(width * ss)), joint='curve')
        if both:
            d.line(P(M(pts)), fill=fill, width=max(1, int(width * ss)), joint='curve')

    def taper(pts, w0, w1, fill=255):
        for k in range(len(pts) - 1):
            t = k / (len(pts) - 1)
            stroke([pts[k], pts[k + 1]], w0 + (w1 - w0) * t, fill)

    def spiral(x0, y0, r0, a0, turns, sgn=1, sq=1.0, n=60):
        pts = []
        for k in range(n):
            t = k / (n - 1)
            a = a0 + sgn * t * turns * 2 * math.pi
            r = r0 * (1 - 0.8 * t)
            pts.append((x0 + math.cos(a) * r, y0 + math.sin(a) * r * sq))
        return pts

    # Ogee frame vine with leaflets.
    frame = []
    for k in range(80):
        t = k / 79
        y = h * (0.04 + 0.92 * t)
        v = (y / h - 0.5) * 2
        xw = w * 0.47 * math.sqrt(max(0, 1 - v * v)) * (1 - 0.2 * v) ** 1.2
        frame.append((cx - xw, y))
    stroke(frame, 2.2)
    for k in range(6, 76, 9):
        x, y = frame[k]
        a = math.atan2(frame[k + 1][1] - y, frame[k + 1][0] - x)
        shape(_leaf_pts(x, y, a + 0.9, w * 0.09, w * 0.022, curl=0.15))
    # Central palmette.
    for k, a in enumerate(np.linspace(-1.45, -0.12, 6)):
        ln = h * (0.17 + 0.05 * math.sin(k / 5 * math.pi))
        shape(_leaf_pts(cx, h * 0.54, -math.pi / 2 + a, ln, w * 0.05, curl=-0.06))
    d.ellipse(P([(cx - w * 0.12, h * 0.5), (cx + w * 0.12, h * 0.6)]), fill=255)
    d.ellipse(P([(cx - w * 0.06, h * 0.525), (cx + w * 0.06, h * 0.575)]), fill=0)
    d.ellipse(P([(cx - w * 0.03, h * 0.538), (cx + w * 0.03, h * 0.562)]), fill=255)
    # Crown: tall fleuron + flanking curled leaves + small top bud.
    shape(_leaf_pts(cx, h * 0.33, -math.pi / 2, h * 0.2, w * 0.07), both=False)
    shape(_leaf_pts(cx - 2, h * 0.31, -math.pi / 2 - 0.55, h * 0.12, w * 0.045, curl=0.25))
    d.ellipse(P([(cx - 6, h * 0.10), (cx + 6, h * 0.10 + 12)]), fill=255)
    # Pendant.
    shape(_leaf_pts(cx, h * 0.6, math.pi / 2, h * 0.22, w * 0.075), both=False)
    shape(_leaf_pts(cx, h * 0.72, math.pi / 2 - 0.7, h * 0.1, w * 0.04, curl=-0.2))
    d.ellipse(P([(cx - 7, h * 0.84), (cx + 7, h * 0.84 + 14)]), fill=255)
    # Big C-scroll with acanthus leaflets.
    sc = spiral(cx - w * 0.25, h * 0.42, w * 0.2, math.pi * 0.35, 0.95, 1, 1.35)
    taper(sc, 9, 3)
    for k in range(3, 40, 6):
        x, y = sc[k]
        x1, y1 = sc[k + 1]
        a = math.atan2(y1 - y, x1 - x)
        shape(_leaf_pts(x, y, a - 1.2, w * 0.1, w * 0.025, curl=0.3))
    d.ellipse(P([(sc[-1][0] - 6, sc[-1][1] - 6), (sc[-1][0] + 6, sc[-1][1] + 6)]), fill=255)
    d.ellipse(P([(M([sc[-1]])[0][0] - 6, sc[-1][1] - 6), (M([sc[-1]])[0][0] + 6, sc[-1][1] + 6)]), fill=255)
    # Lower S-scroll.
    sc2 = spiral(cx - w * 0.22, h * 0.76, w * 0.13, -math.pi * 0.2, 0.85, -1, 1.2)
    taper(sc2, 6.5, 2.5)
    for k in range(4, 40, 8):
        x, y = sc2[k]
        x1, y1 = sc2[k + 1]
        shape(_leaf_pts(x, y, math.atan2(y1 - y, x1 - x) + 1.3, w * 0.07, w * 0.02, curl=-0.3))
    # Upper small scrolls near the crown.
    sc3 = spiral(cx - w * 0.17, h * 0.2, w * 0.09, math.pi * 0.1, 0.8, 1, 1.3)
    taper(sc3, 5, 2)
    # Veins cut into palmette and crown.
    for a in np.linspace(-1.45, -0.12, 6):
        aa = -math.pi / 2 + a
        stroke([(cx + math.cos(aa) * h * 0.04, h * 0.54 + math.sin(aa) * h * 0.04),
                (cx + math.cos(aa) * h * 0.15, h * 0.54 + math.sin(aa) * h * 0.15)], 1.1, fill=0)
    stroke([(cx, h * 0.31), (cx, h * 0.17)], 1.2, fill=0, both=False)
    stroke([(cx, h * 0.63), (cx, h * 0.79)], 1.2, fill=0, both=False)
    img = img.resize((w, h), Image.LANCZOS)
    return np.asarray(img).astype(np.float64) / 255.0


def wallpaper_damask():
    """1 m tile: dark burgundy damask, half-drop medallions (25 x 50 cm), satin motif on a matte
    ground; water-stained and lifting at the seams (for the run-down old house)."""
    w, h = 256, 512
    m = _damask_motif(w, h)
    tile = np.zeros((S, S))
    for col in range(4):                     # half-drop: odd columns hang half a repeat lower
        y0 = 256 if col % 2 else 0
        tile[:, col * 256:(col + 1) * 256] = np.maximum(np.roll(np.tile(m, (2, 1)), y0, axis=0)[:S], 0)
    ground = color([0.30, 0.11, 0.10])
    dark = color([0.17, 0.055, 0.05])
    seed(217)
    tex = fbm(S, 96, 3)
    pat = lerp(ground * (0.92 + 0.12 * fbm(S, 8, 4)[..., None]), dark * (0.9 + 0.2 * tex[..., None]), tile)
    edge = np.clip(gblur(tile, 1.2) - tile, 0, 1)
    pat = lerp(pat, ground * 1.25, edge * 0.5)                  # thin lighter outline from the print
    _paper('T_Wallpaper_Damask', pat, 217, ground, 0.86, 0.55, tile, shift=6, stain_amount=1.0, peel=1.0,
           fade=0.1, emboss=0.06)


# =======================================================================================
# Interiors: floors, tiles, counters

def wood_floor():
    """2 m tile: oak strip flooring, 28 strips (~7 cm) along U with staggered lengths, satin finish."""
    seed(218)
    rnd = R(218)
    yy, xx = grid()
    rows = 28
    rh = S / rows
    vrow = (yy % rh) / rh
    board = np.zeros((S, S), int)
    end_d = np.full((S, S), 999.0)
    nb = 0
    for r in range(rows):
        sel = slice(int(round(r * rh)), int(round((r + 1) * rh)))
        start = rnd.uniform(0, S)
        cuts = [start]
        while True:
            ln = rnd.uniform(0.45, 1.4) * S / 2.0
            if cuts[-1] + ln >= start + S - 0.2 * S / 2.0:
                break
            cuts.append(cuts[-1] + ln)
        cuts = np.array(cuts)
        xr = (xx[sel] - start) % S + start
        idx = np.searchsorted(cuts, xr, side='right') - 1
        d0 = xr - cuts[idx]
        nxt = np.where(idx + 1 < len(cuts), cuts[np.minimum(idx + 1, len(cuts) - 1)], start + S)
        board[sel] = nb + idx
        end_d[sel] = np.minimum(d0, nxt - xr)
        nb += len(cuts)
    rings, fibres, pores = wood_grain(S, 10, 2, 22.0, 260, seed_value=219)
    offy = R(220).integers(0, S, nb)[board]
    offx = R(221).integers(0, S, nb)[board]
    yi = (yy.astype(int) + offy) % S
    xi = (xx.astype(int) + offx) % S
    rings = rings[yi, xi]
    fibres = fibres[yi, xi]
    pores = pores[yi, xi]
    bt = R(222).normal(0, 1, (nb, 2))[board]
    seed(223)
    side_d = np.minimum(vrow, 1 - vrow) * rh
    seam = 1 - smoothstep(0.2, 0.9, side_d)
    endj = 1 - smoothstep(0.2, 0.9, end_d)
    bevel = smoothstep(0.5, 3.0, np.minimum(side_d, end_d))
    cup = (np.abs(vrow - 0.5) * 2) ** 2
    traffic = smoothstep(0.4, 0.75, fbm(S, 2, 4))
    scr = scratch_lines(R(224), 380, (10, 90), (0.5, 1.1), angle=0.0, angle_jitter=0.35)
    scr2 = scratch_lines(R(225), 120, (5, 40), (0.5, 1.0))
    scratches = np.clip(scr * 0.8 + scr2 * 0.6, 0, 1)
    dents = speckle(0.0004, 1.0, R(226))
    smudge = stains(S, 0.25, 4, 4, 0.2)

    light = color([0.60, 0.43, 0.26])
    dark = color([0.40, 0.26, 0.14])
    fine_f = value_noise_aniso(S, 700, 24)[yi, xi]
    t = np.clip(0.55 + 0.4 * (fibres - 0.5) + 0.2 * (fine_f - 0.5) - 0.2 * rings + 0.12 * bt[..., 0], 0, 1)
    bc = lerp(dark, light, t)
    bc = bc * (1 + 0.06 * bt[..., 1, None] * color([1.0, 0.9, 0.75]))
    bc = lerp(bc, bc * 0.7, pores[..., None] * 0.6)
    bc = lerp(bc, np.minimum(bc * 1.12 + 0.02, 1), traffic[..., None] * 0.35)
    bc = lerp(bc, np.minimum(bc * 1.2 + 0.04, 1), scratches[..., None] * 0.45)
    bc = lerp(bc, color([0.14, 0.10, 0.06]), np.clip(seam + endj, 0, 1)[..., None] * 0.7)
    bc = lerp(bc, bc * 0.9, (1 - bevel)[..., None] * 0.3)
    bc = lerp(bc, bc * 0.75, dents[..., None] * 0.5)
    height = (bevel * 0.4 - cup * 0.05 + fibres * 0.04 - rings * 0.02 - pores * 0.04 - scratches * 0.05
              - dents * 0.2 - (seam + endj) * 0.3)
    rough = (0.38 + 0.08 * (fibres - 0.5) + 0.12 * traffic + 0.18 * scratches + 0.1 * smudge + 0.3 * (seam + endj)
             + 0.06 * pores)
    finish('T_Wood_Floor', bc, height, rough, normal_strength=3.0, ao_radius=3, ao_strength=1.0)


def carpet():
    """1 m tile: low cut-pile carpet, neutral (tinted per instance), worn traffic lanes."""
    seed(227)
    rnd = R(227)
    f1, f2, ids = cells(S, 380, warp_y=(fbm(S, 64, 2) - 0.5) * 3, warp_x=(fbm(S, 64, 2) - 0.5) * 3)
    tuft_tone = R(228).random(380 * 380)[ids]
    tufts = gblur(1 - smoothstep(0.1, 0.8, f1), 0.6)
    f1b, _, idsb = cells(S, 520)
    fine = 1 - smoothstep(0.05, 0.7, f1b)
    fib = R(229).random((S, S))
    fib = gblur(fib, 0.5)
    shade = value_noise_aniso(S, 6, 3) * 0.5 + fbm(S, 3, 4) * 0.5                   # pile direction shading
    traffic = smoothstep(0.45, 0.8, fbm(S, 2, 4) * 0.7 + value_noise_aniso(S, 2, 6) * 0.3)
    stain = stains(S, 0.08, 3, 4, 0.08)
    lint = speckle(0.0015, 0.6, rnd)
    base = color([0.76, 0.74, 0.70])
    spark = R(230).random((S, S))
    tone = (0.9 + 0.1 * tuft_tone) * (0.92 + 0.08 * tufts) * (0.93 + 0.1 * fib) * (0.92 + 0.12 * shade) * (0.96 + 0.06 * spark)
    bc = base * tone[..., None]
    bc = lerp(bc, bc * color([0.90, 0.88, 0.84]), traffic[..., None] * 0.5)
    bc = lerp(bc, bc * color([0.80, 0.74, 0.66]), stain[..., None] * 0.35)
    bc = lerp(bc, color([0.92, 0.92, 0.92]), lint[..., None] * 0.5)
    height = tufts * (0.45 - 0.25 * traffic) + fine * 0.2 + fib * 0.15 + shade * 0.1
    rough = 0.94 + 0.04 * (fib - 0.5) - 0.03 * traffic
    finish('T_Carpet', bc, height, rough, normal_strength=2.5, ao_radius=2, ao_strength=1.6)


def _tile_grid(n, grout_px, cushion_px, wob=0.6):
    """Square tile layout: per-pixel tile index, distance to the tile edge (px), local uv."""
    yy, xx = grid()
    size = S / n
    wy = (fbm(S, 8, 2) - 0.5) * wob
    wx = (fbm(S, 8, 2) - 0.5) * wob
    X = xx + wx
    Y = yy + wy
    ix = np.floor(X / size).astype(int) % n
    iy = np.floor(Y / size).astype(int) % n
    fx = (X % size)
    fy = (Y % size)
    ex = np.minimum(fx, size - fx)
    ey = np.minimum(fy, size - fy)
    e = np.minimum(ex, ey)
    corner = np.sqrt(np.clip(cushion_px - ex, 0, None) ** 2 + np.clip(cushion_px - ey, 0, None) ** 2)
    edge = np.where((ex < cushion_px) & (ey < cushion_px), cushion_px - corner, e)
    grout = 1 - smoothstep(grout_px * 0.5 - 0.4, grout_px * 0.5 + 0.6, edge)
    cushion = smoothstep(grout_px * 0.5, grout_px * 0.5 + cushion_px, edge)
    return iy * n + ix, edge, grout, cushion, fx / size, fy / size


def tiles_square():
    """1 m tile: 10 x 10 white glazed ceramic tiles (10 cm), grey grout with grime, a few cracks."""
    seed(230)
    rnd = R(230)
    tid, edge, grout, cushion, u, v = _tile_grid(10, 3.0, 5.0)
    tv = R(231).random((100, 4))[tid]
    cracked = np.zeros(100, bool)
    cracked[[13, 47, 82]] = True
    cv = WrapCanvas(S, ss=2)
    for t_ in np.nonzero(cracked)[0]:
        x0 = (t_ % 10) * 102.4
        y0 = (t_ // 10) * 102.4
        crack_paths(cv, rnd, 1, steps=(14, 18), seg=(5, 8), width=(1.1, 0.7), wander=0.15, branch=0.15,
                    starts=[(x0 + rnd.uniform(5, 30), y0 + rnd.uniform(10, 90))], angle=rnd.uniform(-0.6, 0.6), zig=0.35)
    crack = np.clip(cv.array() * 1.3, 0, 1) * (1 - grout)
    craze_mask = (R(232).random(100) < 0.08)[tid]
    craze = (1 - smoothstep(0.0, 0.04, cells(S, 60)[1] - cells(S, 60)[0])) * craze_mask * 0.5
    wave = fbm(S, 10, 3)
    grime = smoothstep(0.3, 0.8, fbm(S, 4, 5))
    smudge = stains(S, 0.25, 4, 4, 0.25)
    chips = (1 - smoothstep(2.5, 5, edge)) * smoothstep(0.84, 0.88, fbm(S, 40, 3)) * (1 - grout)

    white = color([0.86, 0.86, 0.84])
    tile = white * (0.975 + 0.04 * (tv[..., 0, None] - 0.5)) * lerp(color([1, 1, 1]), color([1.0, 0.985, 0.95]), tv[..., 1])
    tile = tile * (0.99 + 0.02 * wave[..., None])
    tile = lerp(tile, tile * color([0.85, 0.83, 0.78]), smudge[..., None] * 0.25)
    tile = lerp(tile, tile * 0.95, (1 - cushion)[..., None] * 0.6)
    tile = lerp(tile, color([0.30, 0.29, 0.27]), crack[..., None] * 0.85)
    tile = lerp(tile, tile * 0.8, craze[..., None])
    tile = lerp(tile, color([0.55, 0.53, 0.50]), chips[..., None] * 0.8)
    gr = lerp(color([0.52, 0.51, 0.48]), color([0.26, 0.23, 0.19]), grime)[..., :] * (0.85 + 0.25 * fbm(S, 128, 2)[..., None])
    bc = lerp(tile, gr, grout[..., None])
    height = cushion * 0.6 + wave * 0.02 * cushion - crack * 0.15 - chips * 0.2 - grout * 0.2 + fbm(S, 160, 2) * 0.03 * grout
    rough = lerp(0.1 + 0.08 * smudge + 0.03 * tv[..., 2] + 0.4 * chips + 0.3 * crack, 0.85 + 0.05 * grime, grout)
    finish('T_Tiles_Square', bc, height, rough, normal_strength=3.0, ao_radius=3, ao_strength=1.2)


def tiles_checker():
    """1.2 m tile (catalogue uv 1/1.2): 4 x 4 black & white 30 cm floor tiles, scuffed and dulled."""
    seed(233)
    rnd = R(233)
    tid, edge, grout, cushion, u, v = _tile_grid(4, 2.4, 3.0, wob=0.8)
    ix = tid % 4
    iy = tid // 4
    black = ((ix + iy) % 2 == 1)
    tv = R(234).random((16, 3))[tid]
    # Heel scuffs: short dark arcs on the white tiles, pale grey ones on the black tiles.
    cv = WrapCanvas(S, ss=2)
    lanes = fbm(S, 2, 3)
    placed = 0
    while placed < 150:
        x, y = rnd.uniform(0, S, 2)
        if lanes[int(y), int(x)] < rnd.uniform(0.35, 0.7):
            continue
        a = rnd.normal(0.3, 0.5)
        ln = rnd.uniform(6, 28)
        bend = rnd.normal(0, 0.12)
        pts = [(x + math.cos(a + bend * k / 4) * ln * k / 4, y + math.sin(a + bend * k / 4) * ln * k / 4) for k in range(5)]
        cv.line(pts, fill=float(rnd.uniform(0.2, 0.7)), width=rnd.uniform(1.5, 4.0))
        placed += 1
    scuff = gblur(np.clip(cv.array(), 0, 1), 1.6)
    scr = scratch_lines(R(235), 220, (10, 70), (0.5, 1.0))
    traffic = smoothstep(0.4, 0.8, fbm(S, 2, 4))
    dirt_ = stains(S, 0.3, 4, 5, 0.25)
    chips = (1 - smoothstep(3, 9, edge)) * smoothstep(0.8, 0.86, fbm(S, 30, 3)) * (1 - grout)
    cvc = WrapCanvas(S, ss=2)
    crack_paths(cvc, rnd, 1, steps=(30, 34), seg=(5, 8), width=(1.2, 0.7), wander=0.15, branch=0.1,
                starts=[(256 * 2 + 20, 256 + 60)], angle=0.4, zig=0.35)
    crack = np.clip(cvc.array() * 1.3, 0, 1) * (1 - grout)
    wave = fbm(S, 6, 3)

    w_col = color([0.78, 0.77, 0.72]) * (0.97 + 0.05 * tv[..., 0, None])
    w_col = lerp(w_col, w_col * color([0.95, 0.90, 0.80]), tv[..., 1, None] * 0.5)
    b_col = color([0.055, 0.055, 0.058]) * (0.9 + 0.2 * tv[..., 0, None])
    blackf = black.astype(float)[..., None]
    tile = lerp(w_col, b_col, blackf)
    tile = tile * (0.98 + 0.04 * wave[..., None])
    tile = lerp(tile, lerp(tile * color([0.80, 0.77, 0.70]), tile + color([0.06, 0.06, 0.055]), blackf), dirt_[..., None] * 0.5)
    tile = lerp(tile, lerp(tile * 0.93, tile + 0.05, blackf), traffic[..., None] * 0.6)
    tile = lerp(tile, lerp(color([0.25, 0.24, 0.22]), color([0.22, 0.22, 0.21]), blackf), scuff[..., None] * 0.6)
    tile = lerp(tile, lerp(tile * 0.88, tile + 0.08, blackf), scr[..., None] * 0.5)
    tile = lerp(tile, color([0.45, 0.43, 0.40]), chips[..., None] * 0.8)
    tile = lerp(tile, color([0.18, 0.17, 0.16]), crack[..., None] * 0.8)
    gr = color([0.20, 0.19, 0.17]) * (0.8 + 0.3 * fbm(S, 128, 2)[..., None])
    bc = lerp(tile, gr, grout[..., None])
    height = cushion * 0.5 + wave * 0.03 - scr * 0.03 - chips * 0.2 - crack * 0.12 - grout * 0.15
    rough = lerp(0.28 + 0.15 * traffic + 0.12 * dirt_ + 0.2 * scuff + 0.25 * scr + 0.4 * chips, np.full((S, S), 0.85), grout)
    finish('T_Tiles_Checker', bc, height, rough, normal_strength=2.5, ao_radius=3, ao_strength=1.0)


def countertop():
    """1 m tile: speckled granite-look laminate, fairly smooth, faint wear, knife marks, rings."""
    seed(236)
    base = fbm(S, 6, 5)
    d1, _, id1 = cells(S, 300)
    d2, _, id2 = cells(S, 560)
    d3, _, id3 = cells(S, 200)
    t1 = R(237).random(300 * 300)[id1]
    t2 = R(238).random(560 * 560)[id2]
    t3 = R(239).random(200 * 200)[id3]
    pal = np.array([[0.52, 0.50, 0.47], [0.46, 0.44, 0.42], [0.58, 0.56, 0.53], [0.40, 0.38, 0.36],
                    [0.54, 0.50, 0.45], [0.49, 0.48, 0.46]])
    col = pal[(t1 * len(pal)).astype(int) % len(pal)]
    blk = (t2 < 0.18) & (d2 < 0.55)
    wht = (t2 > 0.9) & (d2 < 0.45)
    rust = (t3 > 0.975) & (d3 < 0.35)
    col = np.where(blk[..., None], color([0.16, 0.16, 0.16]), col)
    col = np.where(wht[..., None], color([0.80, 0.79, 0.76]), col)
    col = np.where(rust[..., None], color([0.45, 0.33, 0.25]), col)
    col = gblur(col, 0.6)
    col = col * (0.92 + 0.12 * base[..., None])
    knife = scratch_lines(R(240), 160, (8, 60), (0.4, 0.9))
    rings_ = np.zeros((S, S))
    yy, xx = grid()
    for (cx, cy, r) in ((300, 620, 38), (720, 240, 32), (760, 300, 30)):
        d = np.sqrt(((xx - cx + S / 2) % S - S / 2) ** 2 + ((yy - cy + S / 2) % S - S / 2) ** 2)
        rings_ = np.maximum(rings_, np.exp(-((d - r) / 1.6) ** 2) * (0.5 + 0.5 * fbm(S, 16, 2)))
    wear = smoothstep(0.5, 0.8, fbm(S, 3, 4))
    bc = col
    bc = lerp(bc, bc * 1.1, knife[..., None] * 0.35)
    bc = lerp(bc, bc * 0.9, rings_[..., None] * 0.4)
    height = base * 0.02 + (d2 < 0.5) * 0.004 - knife * 0.03
    rough = 0.28 + 0.1 * wear + 0.2 * knife + 0.15 * rings_ + 0.03 * (base - 0.5)
    finish('T_Countertop', bc, height, rough, normal_strength=2.0, ao_radius=3, ao_strength=0.5)


ALL = [concrete_sidewalk, grass, dirt, gravel, foliage, bark, roof_shingles, siding_wood, siding_worn,
       stone_wall, paint_wood, wallpaper_floral, wallpaper_stripe, wallpaper_damask, wood_floor, carpet,
       tiles_square, tiles_checker, countertop]

if __name__ == '__main__':
    os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..'))
    only = set(sys.argv[1:])
    unknown = only - {fn.__name__ for fn in ALL}
    if unknown:
        sys.exit('unknown texture(s): ' + ', '.join(sorted(unknown)) + '\navailable: ' + ', '.join(f.__name__ for f in ALL))
    for fn in ALL:
        if only and fn.__name__ not in only:
            continue
        print('->', fn.__name__, flush=True)
        fn()
