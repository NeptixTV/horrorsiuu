"""Geometry helpers for the asset pipeline (numpy)."""
import numpy as np


class Part:
    """A triangle list with one material: pos (N,3), nrm (N,3), uv (N,2), idx (M*3), material key."""

    def __init__(self, pos, nrm, uv, idx, material, name=None):
        self.pos = np.asarray(pos, dtype=np.float64)
        self.nrm = None if nrm is None else np.asarray(nrm, dtype=np.float64)
        self.uv = None if uv is None else np.asarray(uv, dtype=np.float64)
        self.idx = np.asarray(idx, dtype=np.int64).reshape(-1)
        self.material = material
        self.name = name

    def copy(self):
        return Part(self.pos.copy(), None if self.nrm is None else self.nrm.copy(), None if self.uv is None else self.uv.copy(), self.idx.copy(), self.material, self.name)

    @property
    def tris(self):
        return len(self.idx) // 3

    def bounds(self):
        return self.pos.min(axis=0), self.pos.max(axis=0)

    def transform(self, m):
        """4x4 matrix (points as column vectors)."""
        m = np.asarray(m, dtype=np.float64)
        self.pos = self.pos @ m[:3, :3].T + m[:3, 3]
        if self.nrm is not None:
            n = self.nrm @ np.linalg.inv(m[:3, :3])
            self.nrm = n / np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
        if np.linalg.det(m[:3, :3]) < 0:
            self.idx = self.idx.reshape(-1, 3)[:, ::-1].reshape(-1)
        return self

    def translate(self, t):
        self.pos = self.pos + np.asarray(t, dtype=np.float64)
        return self

    def scale(self, s):
        self.pos = self.pos * s
        return self

    def select_tris(self, mask):
        """New part with only the triangles where mask is True (vertices compacted)."""
        tri = self.idx.reshape(-1, 3)[mask]
        used, inv = np.unique(tri, return_inverse=True)
        return Part(self.pos[used], None if self.nrm is None else self.nrm[used], None if self.uv is None else self.uv[used], inv.reshape(-1), self.material, self.name)

    def compute_normals(self):
        tri = self.idx.reshape(-1, 3)
        fn = np.cross(self.pos[tri[:, 1]] - self.pos[tri[:, 0]], self.pos[tri[:, 2]] - self.pos[tri[:, 0]])
        n = np.zeros_like(self.pos)
        for k in range(3):
            np.add.at(n, tri[:, k], fn)
        self.nrm = n / np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
        return self


def flatten(g):
    """All primitives of a glTF in world space as Parts (material = source material index)."""
    world, _ = g.world_matrices()
    out = []
    for ni, n in enumerate(g.js['nodes']):
        if 'mesh' not in n:
            continue
        for p in g.js['meshes'][n['mesh']]['primitives']:
            a = p['attributes']
            pos = g.accessor(a['POSITION'])
            nrm = g.accessor(a['NORMAL']) if 'NORMAL' in a else None
            uv = g.accessor(a['TEXCOORD_0']) if 'TEXCOORD_0' in a else None
            idx = g.accessor(p['indices']).reshape(-1) if 'indices' in p else np.arange(len(pos))
            part = Part(pos, nrm, uv, idx, p.get('material'), n.get('name'))
            part.node = ni
            part.transform(world[ni])
            out.append(part)
    return out


def merge(parts, material=None, name=None):
    """Concatenate parts (must share the material unless one is given)."""
    pos, nrm, uv, idx, off = [], [], [], [], 0
    for p in parts:
        pos.append(p.pos)
        nrm.append(p.nrm if p.nrm is not None else np.tile([0.0, 1.0, 0.0], (len(p.pos), 1)))
        uv.append(p.uv if p.uv is not None else np.zeros((len(p.pos), 2)))
        idx.append(p.idx + off)
        off += len(p.pos)
    return Part(np.concatenate(pos), np.concatenate(nrm), np.concatenate(uv), np.concatenate(idx), parts[0].material if material is None else material, name or parts[0].name)


def merge_by_material(parts):
    groups = {}
    for p in parts:
        groups.setdefault(p.material, []).append(p)
    return [merge(v) for v in groups.values()]


def tri_components(part, tol=1e-3):
    """Connected component id per triangle (vertices welded by position)."""
    key = np.round(part.pos / tol).astype(np.int64)
    _, inv = np.unique(key, axis=0, return_inverse=True)
    inv = inv.reshape(-1)
    tri = inv[part.idx].reshape(-1, 3)
    parent = np.arange(inv.max() + 1)

    def find(a):
        root = a
        while parent[root] != root:
            root = parent[root]
        while parent[a] != root:
            parent[a], a = root, parent[a]
        return root
    for a, b, c in tri:
        ra, rb, rc = find(a), find(b), find(c)
        m = min(ra, rb, rc)
        parent[ra] = m
        parent[rb] = m
        parent[rc] = m
    roots = np.array([find(v) for v in tri[:, 0]])
    _, comp = np.unique(roots, return_inverse=True)
    return comp.reshape(-1)


def bounds(parts):
    lo = np.min([p.pos.min(axis=0) for p in parts], axis=0)
    hi = np.max([p.pos.max(axis=0) for p in parts], axis=0)
    return lo, hi


def rot_y(deg):
    a = np.radians(deg)
    m = np.eye(4)
    m[0, 0], m[0, 2], m[2, 0], m[2, 2] = np.cos(a), np.sin(a), -np.sin(a), np.cos(a)
    return m


def translation(t):
    m = np.eye(4)
    m[:3, 3] = t
    return m


class Builder:
    """Collects quads / boxes for generated level geometry. UVs are in meters * uv_scale (tiling textures)."""

    def __init__(self):
        self.parts = {}  # material -> lists

    def _buf(self, material):
        return self.parts.setdefault(material, {'pos': [], 'nrm': [], 'uv': [], 'idx': [], 'n': 0})

    def quad(self, material, p0, p1, p2, p3, uv_scale=0.5, uvs=None, flip=False):
        """Quad p0-p1-p2-p3 (counter-clockwise seen from the front). UVs: planar from edge lengths unless given."""
        p = np.array([p0, p1, p2, p3], dtype=np.float64)
        n = np.cross(p[1] - p[0], p[3] - p[0])
        ln = np.linalg.norm(n)
        if ln < 1e-12:
            return
        n /= ln
        if flip:
            p = p[::-1].copy()
            n = -n
        if uvs is None:
            # world aligned planar mapping so neighbouring quads tile seamlessly
            ax = np.argmax(np.abs(n))
            if ax == 1:
                uvs = p[:, [0, 2]] * uv_scale
            elif ax == 0:
                uvs = np.stack([p[:, 2], p[:, 1]], axis=1) * uv_scale
            else:
                uvs = np.stack([p[:, 0], p[:, 1]], axis=1) * uv_scale
            uvs = uvs * np.array([1.0, -1.0])
        b = self._buf(material)
        b['pos'].append(p)
        b['nrm'].append(np.tile(n, (4, 1)))
        b['uv'].append(np.asarray(uvs, dtype=np.float64))
        o = b['n']
        b['idx'].append(np.array([o, o + 1, o + 2, o, o + 2, o + 3]))
        b['n'] += 4

    def box(self, material, lo, hi, uv_scale=0.5, faces='xXyYzZ', materials=None):
        """Axis aligned solid box, outward facing. faces: which sides to emit (x = -X side, X = +X side ...)."""
        x0, y0, z0 = lo
        x1, y1, z1 = hi
        m = lambda f: (materials or {}).get(f, material)
        if 'x' in faces:
            self.quad(m('x'), (x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0), uv_scale)
        if 'X' in faces:
            self.quad(m('X'), (x1, y0, z1), (x1, y0, z0), (x1, y1, z0), (x1, y1, z1), uv_scale)
        if 'y' in faces:
            self.quad(m('y'), (x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1), uv_scale)
        if 'Y' in faces:
            self.quad(m('Y'), (x0, y1, z1), (x1, y1, z1), (x1, y1, z0), (x0, y1, z0), uv_scale)
        if 'z' in faces:
            self.quad(m('z'), (x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0), uv_scale)
        if 'Z' in faces:
            self.quad(m('Z'), (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1), uv_scale)

    def add_part(self, part):
        b = self._buf(part.material)
        b['pos'].append(part.pos)
        b['nrm'].append(part.nrm)
        b['uv'].append(part.uv if part.uv is not None else np.zeros((len(part.pos), 2)))
        b['idx'].append(part.idx + b['n'])
        b['n'] += len(part.pos)

    def to_parts(self):
        out = []
        for material, b in self.parts.items():
            if b['n']:
                out.append(Part(np.concatenate(b['pos']), np.concatenate(b['nrm']), np.concatenate(b['uv']), np.concatenate(b['idx']), material))
        return out
