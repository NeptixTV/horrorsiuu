"""
Tileable PBR materials for the hideout, cosmetics and props.
Run from the HorrorHeist folder:  python Tools/AssetGen/textures/gen_textures.py [names...]
Writes SourceArt/Textures/Materials/<Name>_{BC,N,ORM}.jpg
"""
import os
import sys
import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
from texlib import *  # noqa: F401,F403
import texlib as T  # noqa: E402

S = 1024
OUT = 'SourceArt/Textures/Materials'


def grain_speckle(size, density=0.02, softness=1):
    mask = (rng().random((size, size)) < density).astype(float)
    return blur(mask, softness) if softness > 0 else mask


def stains(size, coverage=0.35, period=3):
    n = fbm(size, period, 5)
    return smoothstep(1 - coverage - 0.08, 1 - coverage + 0.08, n)


def cracks(size, cells=6, width=0.018, keep=0.35):
    edges, ids = worley(size, cells, 'f2')
    line = 1 - smoothstep(0.0, width, edges)
    # Keep only some cell borders, wobble them.
    keep_mask = value_noise(size, cells * 2) < keep
    line = warp(line * keep_mask, 0.02, 8)
    return line


# =======================================================================================

def concrete_floor():
    seed(101)
    base = fbm(S, 3, 6)
    detail = fbm(S, 32, 4)
    agg_d, agg_ids = worley(S, 48, 'f1')
    aggregate = (1 - smoothstep(0.05, 0.35, agg_d)) * (value_noise(S, 48) > 0.55)
    stain = stains(S, 0.28, 2)
    oil = stains(S, 0.1, 3) * stains(S, 0.5, 6)
    crack = cracks(S, 5, 0.012, 0.3)

    tone = 0.36 + 0.12 * (base - 0.5) + 0.06 * (detail - 0.5) + 0.05 * aggregate
    bc = np.stack([tone, tone * 0.985, tone * 0.955], axis=-1)
    bc = lerp(bc, color([0.20, 0.19, 0.17]), stain * 0.55)
    bc = lerp(bc, color([0.07, 0.065, 0.06]), oil * 0.8)
    bc = lerp(bc, color([0.12, 0.11, 0.10]), crack * 0.8)
    height = detail * 0.3 + aggregate * 0.25 - crack * 0.8 + base * 0.1
    rough = 0.82 - 0.1 * detail - 0.35 * oil - 0.12 * stain
    save_pbr(OUT, 'T_Concrete_Floor', bc, height, rough, normal_strength=3.0)


def cinderblock_painted():
    seed(102)
    # 1.6 m square: 4 blocks (40 cm) x 8 courses (20 cm), running bond.
    yy, xx = np.mgrid[0:S, 0:S] / S
    course = np.floor(yy * 8).astype(int)
    u = (xx * 4 + 0.5 * (course % 2)) % 1.0
    v = (yy * 8) % 1.0
    mortar_u = np.minimum(u, 1 - u) * 40
    mortar_v = np.minimum(v, 1 - v) * 20
    mortar = 1 - smoothstep(0.35, 0.9, np.minimum(mortar_u, mortar_v))
    pores = grain_speckle(S, 0.05, 1) * 0.6 + fbm(S, 64, 3) * 0.4
    block_var = value_noise(S, 8) * 0.05
    peel = stains(S, 0.22, 4) * (1 - mortar)
    peel_edge = np.clip(blur(peel, 3) - peel, 0, 1) * 4
    grime = fbm(S, 2, 5)

    paint = color([0.58, 0.60, 0.54])
    bare = color([0.40, 0.39, 0.37])
    bc = np.ones((S, S, 3)) * paint
    bc = bc * (0.92 + block_var[..., None] + 0.08 * (pores[..., None] - 0.5))
    bc = lerp(bc, bare * (0.9 + 0.2 * pores[..., None]), peel)
    bc = lerp(bc, color([0.33, 0.33, 0.31]), mortar * 0.6)
    bc = bc * (0.85 + 0.2 * grime[..., None])
    height = (1 - mortar) * 0.6 + pores * 0.15 - peel * 0.12 + peel_edge * 0.08
    rough = 0.78 + 0.12 * peel - 0.05 * (1 - pores)
    save_pbr(OUT, 'T_Cinderblock_Painted', bc, height, rough, normal_strength=5.0)


def brick_old():
    seed(103)
    yy, xx = np.mgrid[0:S, 0:S] / S
    rows = 13
    cols = 4
    row = np.floor(yy * rows).astype(int)
    u = xx * cols + 0.5 * (row % 2)
    col = np.floor(u).astype(int) % cols
    uf = u % 1.0
    vf = (yy * rows) % 1.0
    gap_u = np.minimum(uf, 1 - uf) * 22
    gap_v = np.minimum(vf, 1 - vf) * 7
    mortar = 1 - smoothstep(0.25, 0.7, np.minimum(gap_u, gap_v))
    brick_id = (row * 7 + col * 13) % 97
    rnd = np.random.default_rng(5).random(97)
    tint = rnd[brick_id]
    pal = np.array([[0.42, 0.17, 0.12], [0.36, 0.15, 0.11], [0.48, 0.22, 0.15], [0.30, 0.14, 0.11], [0.45, 0.25, 0.18]])
    base = pal[(tint * len(pal)).astype(int) % len(pal)]
    surf = fbm(S, 32, 4)
    chips = smoothstep(0.72, 0.8, fbm(S, 16, 4)) * (1 - mortar)
    soot = fbm(S, 2, 5)
    bc = base * (0.85 + 0.3 * surf[..., None])
    bc = lerp(bc, color([0.55, 0.53, 0.49]), mortar)
    bc = lerp(bc, bc * 0.6, chips)
    bc = bc * (0.65 + 0.45 * soot[..., None])
    height = (1 - mortar) * 0.7 + surf * 0.2 - chips * 0.25
    rough = 0.86 - 0.08 * surf
    save_pbr(OUT, 'T_Brick_Old', bc, height, rough, normal_strength=5.0)


def wood_planks():
    seed(104)
    yy, xx = np.mgrid[0:S, 0:S] / S
    planks = 8
    p = np.floor(yy * planks).astype(int)
    vf = (yy * planks) % 1.0
    gap = 1 - smoothstep(0.0, 0.025, np.minimum(vf, 1 - vf))
    offs = np.random.default_rng(9).random(planks)[p]
    # Fibres run along the plank (U). Growth rings appear as long bands.
    fibres = grain_field(S, 128, 4, 4)
    bands = grain_field(S, 24, 2, 3)
    rings = np.sin((bands * 9 + offs * 6 + vf * 1.5) * np.pi * 2) * 0.5 + 0.5
    rings = rings ** 1.6
    fine = fbm(S, 64, 3)
    knots_d, _ = worley(S, 5, 'f1')
    knots = (1 - smoothstep(0.015, 0.09, knots_d)) * (value_noise(S, 5) > 0.6)
    wear = fbm(S, 6, 4)
    dark = color([0.15, 0.09, 0.05])
    light = color([0.36, 0.23, 0.13])
    bc = lerp(dark, light, rings * 0.45 + fibres * 0.45 + fine * 0.1)
    bc = bc * (0.8 + 0.4 * offs[..., None])
    bc = lerp(bc, color([0.07, 0.045, 0.03]), knots * 0.85)
    bc = lerp(bc, bc * 1.3, smoothstep(0.6, 0.85, wear) * 0.45)
    bc = lerp(bc, color([0.03, 0.02, 0.015]), gap)
    height = rings * 0.12 + fibres * 0.2 - gap * 1.0 - knots * 0.05
    rough = 0.6 + 0.15 * fibres - 0.12 * smoothstep(0.6, 0.85, wear)
    save_pbr(OUT, 'T_Wood_Planks', bc, height, rough, normal_strength=4.0)


def wood_raw():
    seed(105)
    yy, xx = np.mgrid[0:S, 0:S] / S
    fibres = grain_field(S, 160, 4, 4)
    bands = grain_field(S, 20, 2, 3)
    rings = (np.sin((bands * 12 + yy * 3) * np.pi * 2) * 0.5 + 0.5) ** 2
    fine = fbm(S, 128, 2)
    stain = stains(S, 0.2, 3)
    bc = lerp(color([0.46, 0.34, 0.21]), color([0.66, 0.51, 0.34]), fibres * 0.55 + (1 - rings) * 0.35 + fine * 0.1)
    bc = lerp(bc, color([0.27, 0.19, 0.12]), stain * 0.45)
    height = rings * 0.1 + fibres * 0.15
    rough = 0.7 + 0.1 * fine
    save_pbr(OUT, 'T_Wood_Raw', bc, height, rough, normal_strength=3.0)


def metal_painted():
    seed(106)
    base = fbm(S, 4, 5)
    chips = smoothstep(0.66, 0.72, warp(fbm(S, 12, 5), 0.03, 6))
    chip_edge = np.clip(blur(chips, 2) - chips * 0.6, 0, 1)
    rust = chips * smoothstep(0.4, 0.7, fbm(S, 16, 4))
    scratches = np.zeros((S, S))
    r = rng()
    for _ in range(120):
        y = r.integers(0, S)
        x = r.integers(0, S)
        length = r.integers(20, 160)
        angle = r.uniform(0, np.pi)
        for t in range(length):
            sy = int(y + np.sin(angle) * t) % S
            sx = int(x + np.cos(angle) * t) % S
            scratches[sy, sx] = 1
    scratches = blur(scratches, 1) * 3
    grime = fbm(S, 3, 5)
    # Paint is near white so material instances tint it (green, grey, blue...).
    paint = color([0.78, 0.78, 0.76]) * (0.92 + 0.12 * base[..., None])
    steel = color([0.35, 0.35, 0.36])
    rust_c = color([0.30, 0.13, 0.06])
    bc = lerp(paint, steel, np.clip(chips + scratches * 0.6, 0, 1))
    bc = lerp(bc, rust_c, rust)
    bc = bc * (0.8 + 0.25 * grime[..., None])
    height = -chips * 0.5 - scratches * 0.2 + chip_edge * 0.15 + base * 0.05
    rough = 0.5 + 0.15 * grime + 0.3 * rust - 0.15 * chips * (1 - rust)
    metal = np.clip((chips + scratches * 0.6) * (1 - rust), 0, 1)
    save_pbr(OUT, 'T_Metal_Painted', bc, height, rough, metal, normal_strength=4.0)


def metal_rust():
    seed(107)
    base = fbm(S, 4, 6)
    pits_d, _ = worley(S, 64, 'f1')
    pits = 1 - smoothstep(0.02, 0.2, pits_d)
    flakes = smoothstep(0.55, 0.75, fbm(S, 24, 4))
    bc = lerp(color([0.22, 0.09, 0.04]), color([0.45, 0.22, 0.09]), base)
    bc = lerp(bc, color([0.12, 0.06, 0.03]), pits * 0.7)
    bc = lerp(bc, color([0.55, 0.30, 0.14]), flakes * 0.5)
    height = base * 0.3 - pits * 0.4 + flakes * 0.2
    rough = 0.85 + 0.1 * base
    metal = (1 - smoothstep(0.2, 0.35, base)) * 0.4
    save_pbr(OUT, 'T_Metal_Rust', bc, height, rough, metal, normal_strength=5.0)


def metal_steel():
    seed(108)
    yy, xx = np.mgrid[0:S, 0:S] / S
    streak = blur(rng().random((S, S)), 1)
    streak = np.repeat(streak.mean(axis=1, keepdims=True), S, axis=1) * 0.5 + fbm(S, 64, 2) * 0.5
    smudge = stains(S, 0.3, 3)
    bc = np.ones((S, S, 3)) * color([0.56, 0.57, 0.58]) * (0.9 + 0.15 * streak[..., None])
    bc = bc * (0.85 + 0.1 * smudge[..., None])
    height = streak * 0.05
    rough = 0.32 + 0.12 * streak + 0.25 * smudge
    metal = np.ones((S, S))
    save_pbr(OUT, 'T_Metal_Steel', bc, height, rough, metal, normal_strength=1.5)


def weave(size, threads, depth=1.0):
    yy, xx = np.mgrid[0:size, 0:size] / size * threads
    wx = np.sin(xx * np.pi * 2) * 0.5 + 0.5
    wy = np.sin(yy * np.pi * 2) * 0.5 + 0.5
    over = ((np.floor(xx) + np.floor(yy)) % 2).astype(float)
    return (wx * over + wy * (1 - over)) * depth


def fabric_couch():
    seed(109)
    w = weave(S, 96)
    wear = stains(S, 0.25, 3)
    stain = stains(S, 0.12, 4)
    base = fbm(S, 8, 4)
    bc = np.ones((S, S, 3)) * color([0.72, 0.70, 0.66]) * (0.85 + 0.2 * w[..., None]) * (0.9 + 0.15 * base[..., None])
    bc = lerp(bc, bc * 1.15, wear * 0.6)
    bc = lerp(bc, color([0.3, 0.25, 0.2]), stain * 0.4)
    height = w * 0.3 + base * 0.05 - wear * 0.05
    rough = 0.92 - 0.05 * wear
    save_pbr(OUT, 'T_Fabric_Weave', bc, height, rough, normal_strength=3.0)


def leather_worn():
    seed(110)
    cell_edges, _ = worley(S, 40, 'f2')
    cracks_ = 1 - smoothstep(0.0, 0.06, cell_edges)
    pores = grain_speckle(S, 0.08, 1)
    wear = stains(S, 0.3, 4)
    base = fbm(S, 6, 5)
    bc = np.ones((S, S, 3)) * color([0.62, 0.60, 0.58]) * (0.85 + 0.2 * base[..., None])
    bc = lerp(bc, bc * 0.6, cracks_ * 0.6)
    bc = lerp(bc, bc * 1.25, wear * 0.4)
    height = -cracks_ * 0.4 - pores * 0.1 + base * 0.05
    rough = 0.55 + 0.25 * wear + 0.1 * cracks_
    save_pbr(OUT, 'T_Leather', bc, height, rough, normal_strength=4.0)


def cardboard():
    seed(111)
    yy, xx = np.mgrid[0:S, 0:S] / S
    corr = np.sin(xx * 160 * np.pi) * 0.5 + 0.5
    fibers = fbm(S, 128, 3)
    blotch = fbm(S, 4, 4)
    bc = np.ones((S, S, 3)) * color([0.55, 0.42, 0.27]) * (0.85 + 0.2 * blotch[..., None] + 0.1 * fibers[..., None])
    height = corr * 0.08 + fibers * 0.05
    rough = 0.9
    save_pbr(OUT, 'T_Cardboard', bc, height, np.full((S, S), rough), normal_strength=2.0)


def cork():
    seed(112)
    d, ids = worley(S, 90, 'f1')
    grains = 1 - smoothstep(0.0, 0.5, d)
    tone = np.random.default_rng(3).random(90 * 90)[ids]
    holes = smoothstep(0.75, 0.85, fbm(S, 64, 3))
    bc = lerp(color([0.42, 0.28, 0.15]), color([0.62, 0.44, 0.26]), tone * 0.7 + grains * 0.3)
    bc = lerp(bc, color([0.2, 0.12, 0.06]), holes * 0.6)
    height = grains * 0.3 - holes * 0.4
    save_pbr(OUT, 'T_Cork', bc, height, np.full((S, S), 0.88), normal_strength=4.0)


def rubber():
    seed(113)
    n = fbm(S, 64, 3)
    dust = stains(S, 0.3, 3)
    bc = np.ones((S, S, 3)) * 0.06 * (0.9 + 0.2 * n[..., None])
    bc = lerp(bc, color([0.18, 0.17, 0.15]), dust * 0.5)
    save_pbr(OUT, 'T_Rubber', bc, n * 0.1, 0.8 + 0.1 * dust, normal_strength=2.0)


def knit():
    seed(114)
    yy, xx = np.mgrid[0:S, 0:S] / S
    cols = 64
    rows = 48
    cu = (xx * cols) % 1.0
    cv = (yy * rows) % 1.0
    # Each stitch: a "V" made of two slanted loops.
    left = np.exp(-((cu - 0.25 - (cv - 0.5) * 0.35) ** 2) / 0.012)
    right = np.exp(-((cu - 0.75 + (cv - 0.5) * 0.35) ** 2) / 0.012)
    loops = np.clip(left + right, 0, 1) * (0.6 + 0.4 * np.sin(cv * np.pi))
    fuzz = fbm(S, 128, 2)
    bc = np.ones((S, S, 3)) * color([0.75, 0.74, 0.72]) * (0.7 + 0.3 * loops[..., None]) * (0.95 + 0.1 * fuzz[..., None])
    height = loops * 0.6 + fuzz * 0.1
    save_pbr(OUT, 'T_Knit', bc, height, np.full((S, S), 0.95), normal_strength=4.0)


def denim():
    seed(115)
    yy, xx = np.mgrid[0:S, 0:S] / S
    twill = np.sin((xx + yy) * 260 * np.pi) * 0.5 + 0.5
    fade_ = stains(S, 0.35, 3)
    fibers = fbm(S, 128, 2)
    bc = lerp(color([0.70, 0.71, 0.74]), color([0.90, 0.90, 0.92]), twill * 0.3 + fade_ * 0.5)
    bc = bc * (0.92 + 0.08 * fibers[..., None])
    height = twill * 0.15 + fibers * 0.05
    save_pbr(OUT, 'T_Denim', bc, height, np.full((S, S), 0.9), normal_strength=2.5)


def cotton():
    seed(116)
    w = weave(S, 220)
    n = fbm(S, 16, 4)
    bc = np.ones((S, S, 3)) * color([0.82, 0.82, 0.81]) * (0.93 + 0.07 * w[..., None]) * (0.95 + 0.08 * n[..., None])
    save_pbr(OUT, 'T_Cotton', bc, w * 0.15 + n * 0.03, np.full((S, S), 0.93), normal_strength=2.0)


def nylon():
    seed(117)
    yy, xx = np.mgrid[0:S, 0:S] / S
    grid = np.maximum((np.sin(xx * 64 * np.pi) > 0.96), (np.sin(yy * 64 * np.pi) > 0.96)).astype(float)
    n = fbm(S, 8, 4)
    bc = np.ones((S, S, 3)) * color([0.78, 0.78, 0.78]) * (0.9 + 0.1 * n[..., None])
    height = grid * 0.3 + n * 0.05
    save_pbr(OUT, 'T_Nylon', bc, height, 0.55 + 0.1 * n, normal_strength=2.0)


def skin():
    seed(118)
    pores = grain_speckle(S, 0.12, 1)
    mottle = fbm(S, 6, 5)
    red = stains(S, 0.3, 5)
    bc = np.ones((S, S, 3)) * color([0.80, 0.80, 0.80]) * (0.95 + 0.08 * mottle[..., None])
    bc = lerp(bc, bc * color([1.0, 0.92, 0.9]), red * 0.3)
    height = -pores * 0.2 + mottle * 0.05
    save_pbr(OUT, 'T_Skin', bc, height, 0.55 + 0.1 * mottle, normal_strength=1.5)


def plaster():
    seed(119)
    n = fbm(S, 4, 6)
    fine = fbm(S, 96, 3)
    cracks_ = cracks(S, 4, 0.006, 0.25)
    damp = stains(S, 0.25, 2)
    bc = np.ones((S, S, 3)) * color([0.62, 0.60, 0.56]) * (0.9 + 0.12 * n[..., None] + 0.05 * fine[..., None])
    bc = lerp(bc, color([0.42, 0.40, 0.33]), damp * 0.45)
    bc = lerp(bc, bc * 0.5, cracks_ * 0.7)
    height = n * 0.2 + fine * 0.1 - cracks_ * 0.5
    save_pbr(OUT, 'T_Plaster', bc, height, 0.88 - 0.05 * damp, normal_strength=3.0)


def gravel():
    seed(120)
    d, ids = worley(S, 70, 'f1')
    stones = 1 - smoothstep(0.1, 0.55, d)
    tone = np.random.default_rng(4).random(70 * 70)[ids]
    wet = stains(S, 0.4, 3)
    bc = lerp(color([0.18, 0.17, 0.16]), color([0.42, 0.40, 0.37]), tone) * (0.6 + 0.4 * stones[..., None])
    bc = lerp(bc, bc * 0.55, wet * 0.6)
    save_pbr(OUT, 'T_Asphalt_Wet', bc, stones * 0.5, 0.45 - 0.3 * wet, normal_strength=5.0)


ALL = [concrete_floor, cinderblock_painted, brick_old, wood_planks, wood_raw, metal_painted, metal_rust,
       metal_steel, fabric_couch, leather_worn, cardboard, cork, rubber, knit, denim, cotton, nylon, skin,
       plaster, gravel]

if __name__ == '__main__':
    os.chdir(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
    only = set(sys.argv[1:])
    for fn in ALL:
        if only and fn.__name__ not in only:
            continue
        print('->', fn.__name__, flush=True)
        fn()
