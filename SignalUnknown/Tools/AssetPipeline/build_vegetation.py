"""Vegetation prefabs from "a forest (3) with a road at night for game" by dasy444 (CC-BY-4.0).

The source is a small diorama (19 x 10 units). Next to the terrain it contains one clean sample of
every plant. Those samples become individual prefabs, scaled to real-world sizes.
"""
import os
import numpy as np
from PIL import Image
from gltfio import Gltf
from meshutil import flatten, merge_by_material
from glbout import GlbFile, MaterialSpec, stats

CREDIT = '"a forest (3) with a road at night for game" by dasy444 (https://sketchfab.com/dasy444), CC-BY-4.0, modified'

# sample x position in the source -> (prefab name, real height in meters)
SAMPLES = [
    (-14.10, 'SM_Veg_GrassDry', 0.75),
    (-13.14, 'SM_Veg_GrassGreen', 0.6),
    (-12.37, 'SM_Veg_SpruceBushy', 10.0),
    (-11.57, 'SM_Veg_ConiferLight', 8.5),
    (-10.57, 'SM_Veg_ConiferTall', 26.0),
    (-9.45, 'SM_Veg_PineTall', 21.0),
    (-8.38, 'SM_Veg_ConiferMid', 15.0),
    (-7.36, 'SM_Veg_SpruceSmall', 4.0),
    (-6.54, 'SM_Veg_Bush', 2.4),
]


def bleed_alpha(img, iterations=24):
    """Spread colour into fully transparent pixels so mip maps do not get dark halos."""
    a = np.asarray(img.convert('RGBA')).astype(np.float32)
    rgb, alpha = a[..., :3].copy(), a[..., 3]
    known = alpha > 8
    for _ in range(iterations):
        acc = np.zeros_like(rgb)
        cnt = np.zeros(alpha.shape, dtype=np.float32)
        for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0)):
            k = np.roll(known, (dy, dx), axis=(0, 1))
            acc += np.roll(rgb, (dy, dx), axis=(0, 1)) * k[..., None]
            cnt += k
        fill = (~known) & (cnt > 0)
        rgb[fill] = acc[fill] / cnt[fill][:, None]
        known = known | fill
    rgb[~known] = rgb[known].mean(axis=0)
    out = np.concatenate([rgb, alpha[..., None]], axis=-1)
    return Image.fromarray(out.clip(0, 255).astype(np.uint8), 'RGBA')


def build(src_dir, out_dir):
    g = Gltf(os.path.join(src_dir, 'scene.gltf'))
    parts = flatten(g)
    tdir = os.path.join(src_dir, 'textures')
    atlas = bleed_alpha(Image.open(os.path.join(tdir, 'Material_baseColor.png')))
    bark = Image.open(os.path.join(tdir, 'Material.004_baseColor.jpeg'))
    bark_n = Image.open(os.path.join(tdir, 'Material.004_normal.png'))
    materials = {
        0: MaterialSpec('M_Veg_Foliage', base=atlas, roughness=0.85, alpha_mode='MASK', alpha_cutoff=0.38, double_sided=False, max_size=2048),
        1: MaterialSpec('M_Veg_Bark', base=bark, normal=bark_n, roughness=0.95, max_size=512),
    }
    out = GlbFile(materials, CREDIT)
    manifest = {}
    for sx, name, height in SAMPLES:
        mine = [p.copy() for p in parts if p.material in (0, 1) and abs(p.pos.mean(axis=0)[0] - sx) < 0.35 and abs(p.pos.mean(axis=0)[2]) < 0.6]
        assert mine, name
        trunk = [p for p in mine if p.material == 1]
        lo = np.min([p.pos.min(axis=0) for p in mine], axis=0)
        hi = np.max([p.pos.max(axis=0) for p in mine], axis=0)
        ref = trunk[0] if trunk else None
        cx = ref.pos[:, 0].mean() if ref else (lo[0] + hi[0]) * 0.5
        cz = ref.pos[:, 2].mean() if ref else (lo[2] + hi[2]) * 0.5
        s = height / (hi[1] - lo[1])
        for p in mine:
            p.translate([-cx, -lo[1], -cz]).scale(s)
            if p.material == 0:
                # soft "puffy" normals: cards are lit like a volume instead of flat boards
                radial = p.pos * np.array([1.0, 0.0, 1.0])
                radial /= np.maximum(np.linalg.norm(radial, axis=1, keepdims=True), 1e-6)
                n = radial * 0.55 + np.array([0.0, 0.85, 0.0])
                p.nrm = n / np.linalg.norm(n, axis=1, keepdims=True)
                # both sides as front faces with the same normal (two-sided materials would flip it and
                # light one side of every card differently)
                count = len(p.pos)
                p.pos = np.concatenate([p.pos, p.pos])
                p.nrm = np.concatenate([p.nrm, p.nrm])
                p.uv = np.concatenate([p.uv, p.uv])
                p.idx = np.concatenate([p.idx, p.idx.reshape(-1, 3)[:, ::-1].reshape(-1) + count])
            # sink slightly so nothing floats on slopes
            p.translate([0.0, -0.02 * height, 0.0])
        merged = merge_by_material(mine)
        out.add_mesh(name, merged)
        manifest[name] = dict(stats(merged), kind='static', file='Vegetation.glb', height_m=height, trunk=bool(trunk))
    out.save(os.path.join(out_dir, 'Vegetation.glb'))
    return manifest
