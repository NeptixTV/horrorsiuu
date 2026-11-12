"""Writes game-ready GLB files: meters, +Y up, front = +Z, one mesh per prefab at the origin."""
import numpy as np
from gltfio import GlbWriter
import textures as tex


class MaterialSpec:
    def __init__(self, name, base=None, normal=None, orm=None, base_factor=None, metallic=0.0, roughness=0.9,
                 alpha_mode=None, alpha_cutoff=0.5, double_sided=False, max_size=2048, emissive=None, emissive_tex=None):
        self.name = name
        self.base, self.normal, self.orm = base, normal, orm     # PIL images (or None)
        self.base_factor, self.metallic, self.roughness = base_factor, metallic, roughness
        self.alpha_mode, self.alpha_cutoff, self.double_sided = alpha_mode, alpha_cutoff, double_sided
        self.max_size = max_size
        self.emissive, self.emissive_tex = emissive, emissive_tex


class GlbFile:
    """Collects meshes (each: list of Parts whose .material is a key into `materials`)."""

    def __init__(self, materials, credit=None):
        self.w = GlbWriter()
        if credit:
            self.w.js['asset']['copyright'] = credit
        self.specs = materials
        self.mat_index = {}
        self.tex_index = {}

    def _texture(self, img, max_size, normal=False):
        if img is None:
            return None
        key = (id(img), max_size)
        if key not in self.tex_index:
            data, mime = tex.encode(img, max_size=max_size, jpeg_quality=92 if normal else 88)
            self.tex_index[key] = self.w.image(data, mime)
        return self.tex_index[key]

    def _material(self, key):
        if key not in self.mat_index:
            s = self.specs[key]
            self.mat_index[key] = self.w.material(
                s.name, self._texture(s.base, s.max_size), self._texture(s.normal, s.max_size, True), self._texture(s.orm, s.max_size),
                s.base_factor, s.metallic, s.roughness, s.alpha_mode, s.alpha_cutoff, s.double_sided, s.emissive, self._texture(s.emissive_tex, s.max_size))
        return self.mat_index[key]

    def add_mesh(self, name, parts, node_name=None):
        prims = []
        for p in parts:
            if p.tris == 0:
                continue
            prims.append(self.w.primitive(p.pos, p.nrm, p.uv, p.idx, self._material(p.material)))
        mesh = self.w.mesh(name, prims)
        return self.w.node(node_name or name, mesh=mesh)

    def save(self, path):
        self.w.save(path)


def stats(parts):
    lo = np.min([p.pos.min(axis=0) for p in parts], axis=0)
    hi = np.max([p.pos.max(axis=0) for p in parts], axis=0)
    return {'size_m': [round(float(v), 3) for v in (hi - lo)], 'min_m': [round(float(v), 3) for v in lo], 'max_m': [round(float(v), 3) for v in hi],
            'tris': int(sum(p.tris for p in parts))}
