"""
Tiny signed-distance modeller: primitives combined with smooth unions on a voxel grid, meshed
with (vectorised) surface nets. Used for organic shapes (the body, hair, bags) where metaballs
produce lumpy joints.
"""
import numpy as np


def smin(a, b, k):
    if k <= 0:
        return np.minimum(a, b)
    h = np.clip(0.5 + 0.5 * (b - a) / k, 0.0, 1.0)
    return b * (1 - h) + a * h - k * h * (1 - h)


def smax(a, b, k):
    return -smin(-a, -b, k)


class Field:
    def __init__(self, lo, hi, h):
        self.h = float(h)
        self.lo = np.array(lo, dtype=np.float32)
        n = np.ceil((np.array(hi) - np.array(lo)) / h).astype(int) + 1
        self.shape = tuple(int(v) for v in n)
        self.d = np.full(self.shape, 1e3, dtype=np.float32)

    def _box(self, lo, hi):
        i0 = np.clip(np.floor((np.array(lo) - self.lo) / self.h).astype(int), 0, np.array(self.shape) - 1)
        i1 = np.clip(np.ceil((np.array(hi) - self.lo) / self.h).astype(int) + 1, 1, np.array(self.shape))
        sl = tuple(slice(a, b) for a, b in zip(i0, i1))
        axes = [self.lo[k] + np.arange(i0[k], i1[k], dtype=np.float32) * self.h for k in range(3)]
        X, Y, Z = np.meshgrid(*axes, indexing='ij')
        return sl, np.stack([X, Y, Z], -1)

    def apply(self, dist_fn, lo, hi, k=0.0, op='union'):
        margin = k + 2 * self.h
        sl, P = self._box(np.array(lo) - margin, np.array(hi) + margin)
        d = dist_fn(P)
        cur = self.d[sl]
        if op == 'union':
            self.d[sl] = smin(cur, d, k)
        elif op == 'subtract':
            self.d[sl] = smax(cur, -d, k)
        elif op == 'intersect':
            self.d[sl] = smax(cur, d, k)

    # -- primitives (centres/points in field units) ---------------------------------------
    def sphere(self, c, r, k=0.0, op='union'):
        c = np.array(c, dtype=np.float32)
        self.apply(lambda P: np.linalg.norm(P - c, axis=-1) - r, c - r, c + r, k, op)

    def ellipsoid(self, c, radii, k=0.0, axes=None, op='union'):
        """Approximate ellipsoid distance; 'axes' (3x3 rows) orients it."""
        c = np.array(c, dtype=np.float32)
        r = np.array(radii, dtype=np.float32)
        M = np.eye(3, dtype=np.float32) if axes is None else np.array(axes, dtype=np.float32)

        def dist(P):
            q = (P - c) @ M.T
            k0 = np.linalg.norm(q / r, axis=-1)
            k1 = np.linalg.norm(q / (r * r), axis=-1)
            return k0 * (k0 - 1.0) / np.maximum(k1, 1e-6)
        ext = float(r.max())
        self.apply(dist, c - ext, c + ext, k, op)

    def cone(self, a, b, ra, rb, k=0.0, op='union'):
        """Capsule with linearly tapering radius (round cone, slightly inexact)."""
        a = np.array(a, dtype=np.float32)
        b = np.array(b, dtype=np.float32)
        ba = b - a
        L2 = float(ba @ ba)

        def dist(P):
            t = np.clip(((P - a) @ ba) / L2, 0.0, 1.0)
            q = a + t[..., None] * ba
            return np.linalg.norm(P - q, axis=-1) - (ra + (rb - ra) * t)
        r = max(ra, rb)
        self.apply(dist, np.minimum(a, b) - r, np.maximum(a, b) + r, k, op)

    def box(self, c, half, radius=0.0, k=0.0, op='union'):
        c = np.array(c, dtype=np.float32)
        hb = np.array(half, dtype=np.float32) - radius

        def dist(P):
            q = np.abs(P - c) - hb
            return np.linalg.norm(np.maximum(q, 0), axis=-1) + np.minimum(q.max(-1), 0) - radius
        self.apply(dist, c - half, c + half, k, op)

    def grid(self):
        axes = [self.lo[i] + np.arange(self.shape[i], dtype=np.float32) * self.h for i in range(3)]
        X, Y, Z = np.meshgrid(*axes, indexing='ij')
        return np.stack([X, Y, Z], -1)

    def global_op(self, dist_fn, op='intersect', k=0.0):
        """Apply an operation over the whole grid (needed for intersections)."""
        d = dist_fn(self.grid())
        if op == 'intersect':
            self.d = smax(self.d, d, k)
        elif op == 'subtract':
            self.d = smax(self.d, -d, k)
        else:
            self.d = smin(self.d, d, k)

    def plane_cut(self, point, normal, k=0.0):
        """Keep the side the normal points to."""
        p = np.array(point, dtype=np.float32)
        n = np.array(normal, dtype=np.float32)
        n = n / np.linalg.norm(n)
        sl = tuple(slice(0, s) for s in self.shape)
        axes = [self.lo[i] + np.arange(self.shape[i], dtype=np.float32) * self.h for i in range(3)]
        X, Y, Z = np.meshgrid(*axes, indexing='ij')
        d = -((np.stack([X, Y, Z], -1) - p) @ n)
        self.d[sl] = smax(self.d, d, k)

    # -- meshing ---------------------------------------------------------------------------
    def mesh(self):
        """Surface nets. Returns (verts Nx3, quads Mx4)."""
        F = self.d
        nx, ny, nz = F.shape
        offs = [(0, 0, 0), (1, 0, 0), (0, 1, 0), (1, 1, 0), (0, 0, 1), (1, 0, 1), (0, 1, 1), (1, 1, 1)]
        C = [F[i:nx - 1 + i, j:ny - 1 + j, k:nz - 1 + k] for i, j, k in offs]
        edges = [(0, 1), (2, 3), (4, 5), (6, 7), (0, 2), (1, 3), (4, 6), (5, 7), (0, 4), (1, 5), (2, 6), (3, 7)]
        acc = np.zeros(C[0].shape + (3,), dtype=np.float32)
        cnt = np.zeros(C[0].shape, dtype=np.float32)
        for a, b in edges:
            fa, fb = C[a], C[b]
            m = (fa < 0) != (fb < 0)
            if not m.any():
                continue
            t = np.where(m, fa / np.where(m, fa - fb, 1.0), 0.0)
            pa = np.array(offs[a], dtype=np.float32)
            pb = np.array(offs[b], dtype=np.float32)
            acc += m[..., None] * (pa + t[..., None] * (pb - pa))
            cnt += m
        active = cnt > 0
        idx = -np.ones(active.shape, dtype=np.int64)
        idx[active] = np.arange(int(active.sum()))
        cells = np.argwhere(active).astype(np.float32)
        verts = self.lo + (cells + acc[active] / cnt[active][:, None]) * self.h
        inside = F < 0
        quads = []
        # Edges along x between (i,j,k) and (i+1,j,k): cells (i, j-1..j, k-1..k).
        m = inside[:-1, 1:-1, 1:-1] != inside[1:, 1:-1, 1:-1]
        i, j, k = np.nonzero(m)
        j += 1
        k += 1
        q = np.stack([idx[i, j - 1, k - 1], idx[i, j, k - 1], idx[i, j, k], idx[i, j - 1, k]], -1)
        flip = inside[i, j, k]
        q[flip] = q[flip][:, ::-1]
        quads.append(q)
        m = inside[1:-1, :-1, 1:-1] != inside[1:-1, 1:, 1:-1]
        i, j, k = np.nonzero(m)
        i += 1
        k += 1
        q = np.stack([idx[i - 1, j, k - 1], idx[i - 1, j, k], idx[i, j, k], idx[i, j, k - 1]], -1)
        flip = inside[i, j, k]
        q[flip] = q[flip][:, ::-1]
        quads.append(q)
        m = inside[1:-1, 1:-1, :-1] != inside[1:-1, 1:-1, 1:]
        i, j, k = np.nonzero(m)
        i += 1
        j += 1
        q = np.stack([idx[i - 1, j - 1, k], idx[i, j - 1, k], idx[i, j, k], idx[i - 1, j, k]], -1)
        flip = inside[i, j, k]
        q[flip] = q[flip][:, ::-1]
        quads.append(q)
        quads = np.concatenate(quads)
        quads = quads[(quads >= 0).all(-1)]
        return verts, quads


def to_blender_mesh(name, verts, quads):
    import bmesh
    import bpy
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts.tolist(), [], quads.tolist())
    me.validate()
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-4)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    return me


def orient(d, up_hint=(0, -1, 0)):
    """Rows: (axis along d, a perpendicular towards up_hint, their cross)."""
    d = np.array(d, dtype=np.float64)
    d /= np.linalg.norm(d)
    u = np.array(up_hint, dtype=np.float64)
    u = u - d * (u @ d)
    u /= np.linalg.norm(u)
    w = np.cross(d, u)
    return np.stack([d, u, w])
