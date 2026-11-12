"""Organic underground geometry: signed distance field on a grid, meshed with naive surface nets.

Convention: field < 0 is air (walkable space), field > 0 is rock. Faces look into the air.
"""
import numpy as np
from meshutil import Part


class Field:
    def __init__(self, lo, hi, cell=0.4, margin=3.0):
        self.cell = float(cell)
        self.lo = np.asarray(lo, dtype=np.float64) - margin
        hi = np.asarray(hi, dtype=np.float64) + margin
        self.n = np.ceil((hi - self.lo) / self.cell).astype(int) + 1
        self.f = np.full(tuple(self.n), 4.0, dtype=np.float32)

    # ---- grid helpers
    def _region(self, lo, hi):
        a = np.clip(np.floor((np.asarray(lo) - self.lo) / self.cell).astype(int), 0, self.n - 1)
        b = np.clip(np.ceil((np.asarray(hi) - self.lo) / self.cell).astype(int) + 1, 1, self.n)
        sl = tuple(slice(a[k], b[k]) for k in range(3))
        ax = [self.lo[k] + np.arange(a[k], b[k]) * self.cell for k in range(3)]
        X, Y, Z = np.meshgrid(ax[0], ax[1], ax[2], indexing='ij')
        return sl, X, Y, Z

    def _carve(self, sl, d):
        self.f[sl] = np.minimum(self.f[sl], d.astype(np.float32))

    def _fill(self, sl, d):
        """Add rock: d < 0 inside the solid."""
        self.f[sl] = np.maximum(self.f[sl], (-d).astype(np.float32))

    # ---- primitives (all carve air)
    def tunnel(self, a, b, ra, rb=None, height=None, floor_noise=0.0):
        """Tube from floor point a to floor point b. ra / rb: half width; height: clear height above the floor."""
        a, b = np.asarray(a, dtype=np.float64), np.asarray(b, dtype=np.float64)
        rb = ra if rb is None else rb
        rmax = max(ra, rb)
        h = height if height is not None else rmax * 1.7
        lo = np.minimum(a, b) - [rmax + 1, 1.0, rmax + 1]
        hi = np.maximum(a, b) + [rmax + 1, h + 1.5, rmax + 1]
        sl, X, Y, Z = self._region(lo, hi)
        ab = b - a
        t = np.clip(((X - a[0]) * ab[0] + (Y - a[1]) * ab[1] + (Z - a[2]) * ab[2]) / max(float(ab @ ab), 1e-9), 0.0, 1.0)
        fx, fy, fz = a[0] + t * ab[0], a[1] + t * ab[1], a[2] + t * ab[2]
        r = ra + (rb - ra) * t
        # cross section: ellipse centred half way up the clear height
        cy = fy + h * 0.5
        dh = np.sqrt((X - fx) ** 2 + (Z - fz) ** 2) / r
        dv = (Y - cy) / (h * 0.5)
        d = (np.sqrt(dh * dh + dv * dv) - 1.0) * np.minimum(r, h * 0.5)
        floor = fy - Y                                   # > 0 below the floor
        self._carve(sl, np.maximum(d, floor))

    def cavern(self, centre, radii, floor_y=None):
        c, r = np.asarray(centre, dtype=np.float64), np.asarray(radii, dtype=np.float64)
        sl, X, Y, Z = self._region(c - r - 1, c + r + 1)
        d = (np.sqrt(((X - c[0]) / r[0]) ** 2 + ((Y - c[1]) / r[1]) ** 2 + ((Z - c[2]) / r[2]) ** 2) - 1.0) * float(r.min())
        if floor_y is not None:
            d = np.maximum(d, floor_y - Y)
        self._carve(sl, d)

    def box(self, lo, hi):
        lo, hi = np.asarray(lo, dtype=np.float64), np.asarray(hi, dtype=np.float64)
        sl, X, Y, Z = self._region(lo - 1, hi + 1)
        c, e = (lo + hi) * 0.5, (hi - lo) * 0.5
        qx, qy, qz = np.abs(X - c[0]) - e[0], np.abs(Y - c[1]) - e[1], np.abs(Z - c[2]) - e[2]
        outside = np.sqrt(np.maximum(qx, 0) ** 2 + np.maximum(qy, 0) ** 2 + np.maximum(qz, 0) ** 2)
        inside = np.minimum(np.maximum(qx, np.maximum(qy, qz)), 0)
        self._carve(sl, outside + inside)

    # ---- rock features
    def stalagmite(self, base, height, radius, hanging=False):
        base = np.asarray(base, dtype=np.float64)
        y0, y1 = (base[1] - height, base[1]) if hanging else (base[1], base[1] + height)
        sl, X, Y, Z = self._region([base[0] - radius - 1, y0 - 1, base[2] - radius - 1], [base[0] + radius + 1, y1 + 1, base[2] + radius + 1])
        u = np.clip((base[1] - Y) / height if hanging else (Y - base[1]) / height, -0.3, 1.0)
        r = radius * (1 - u) ** 1.3
        d = np.sqrt((X - base[0]) ** 2 + (Z - base[2]) ** 2) - r
        d = np.maximum(d, (u - 1.0) * height)
        self._fill(sl, d)

    def boulder(self, centre, radius):
        c = np.asarray(centre, dtype=np.float64)
        sl, X, Y, Z = self._region(c - radius - 1, c + radius + 1)
        self._fill(sl, np.sqrt((X - c[0]) ** 2 + ((Y - c[1]) * 1.25) ** 2 + (Z - c[2]) ** 2) - radius)

    def roughen(self, amplitude=0.3, scale=2.2, seed=5, keep_floor=None):
        """Displace the surface with 3D noise. keep_floor(X, Y, Z) -> weight 0..1 (0 = leave untouched)."""
        n = noise3(self.n, self.cell / scale, seed) * 0.65 + noise3(self.n, self.cell / (scale * 0.37), seed + 1) * 0.35
        self.f += ((n - 0.5) * 2.0 * amplitude).astype(np.float32)

    # ---- meshing
    def mesh(self, material, uv_scale=0.3):
        f = self.f
        neg = f < 0
        nx, ny, nz = self.n
        # corner sign pattern per cell
        c = [neg[i:nx - 1 + i, j:ny - 1 + j, k:nz - 1 + k] for i in (0, 1) for j in (0, 1) for k in (0, 1)]
        cnt = sum(x.astype(np.int8) for x in c)
        active = (cnt > 0) & (cnt < 8)
        vid = np.full(active.shape, -1, dtype=np.int64)
        ci, cj, ck = np.nonzero(active)
        vid[ci, cj, ck] = np.arange(len(ci))
        # vertex = mean of the zero crossings on the 12 cell edges
        acc = np.zeros((len(ci), 3))
        num = np.zeros(len(ci))
        corner = lambda di, dj, dk: f[ci + di, cj + dj, ck + dk].astype(np.float64)
        edges = []
        for axis in range(3):
            o = [(0, 0), (0, 1), (1, 0), (1, 1)]
            for p, q in o:
                d0 = [0, 0, 0]
                d0[(axis + 1) % 3], d0[(axis + 2) % 3] = p, q
                d1 = list(d0)
                d1[axis] = 1
                edges.append((tuple(d0), tuple(d1)))
        for d0, d1 in edges:
            a, b = corner(*d0), corner(*d1)
            cross = (a < 0) != (b < 0)
            t = np.where(cross, a / np.where(np.abs(a - b) < 1e-12, 1.0, a - b), 0.0)
            pt = np.array(d0, dtype=np.float64)[None, :] + t[:, None] * (np.array(d1) - np.array(d0))[None, :]
            acc += pt * cross[:, None]
            num += cross
        local = acc / np.maximum(num, 1)[:, None]
        verts = self.lo[None, :] + (np.stack([ci, cj, ck], axis=1) + local) * self.cell
        # gradient -> normals (pointing into the air = towards decreasing f)
        g = np.stack(np.gradient(f.astype(np.float32)), axis=-1)
        gi = np.clip(np.round(np.stack([ci, cj, ck], axis=1) + local).astype(int), 0, self.n - 1)
        nrm = -g[gi[:, 0], gi[:, 1], gi[:, 2]].astype(np.float64)
        nrm /= np.maximum(np.linalg.norm(nrm, axis=1, keepdims=True), 1e-9)
        quads = []
        for axis in range(3):
            a1, a2 = (axis + 1) % 3, (axis + 2) % 3
            sl0 = [slice(None)] * 3
            sl1 = [slice(None)] * 3
            sl0[axis], sl1[axis] = slice(0, -1), slice(1, None)
            change = neg[tuple(sl0)] != neg[tuple(sl1)]
            # edges need the four neighbouring cells to exist
            m = np.zeros_like(change)
            inner = [slice(None)] * 3
            inner[a1], inner[a2] = slice(1, -1), slice(1, -1)
            m[tuple(inner)] = change[tuple(inner)]
            e = np.stack(np.nonzero(m), axis=1)
            if not len(e):
                continue
            def cell(da1, da2):
                p = e.copy()
                p[:, a1] += da1
                p[:, a2] += da2
                return vid[p[:, 0], p[:, 1], p[:, 2]]
            q = np.stack([cell(-1, -1), cell(0, -1), cell(0, 0), cell(-1, 0)], axis=1)
            q = q[(q >= 0).all(axis=1)]
            quads.append(q)
        q = np.concatenate(quads)
        # orient: face normal must agree with the vertex normals
        fn = np.cross(verts[q[:, 1]] - verts[q[:, 0]], verts[q[:, 3]] - verts[q[:, 0]])
        flip = (fn * nrm[q[:, 0]]).sum(axis=1) < 0
        q[flip] = q[flip][:, ::-1]
        return verts, nrm, q

    def to_parts(self, material, uv_scale=0.3, chunk=24.0, clip=None):
        """Mesh split into chunks on the XZ grid. clip(centres) -> bool mask of quads to drop.
        Returns {(ix, iz): Part}."""
        verts, nrm, q = self.mesh(material)
        centre = verts[q].mean(axis=1)
        if clip is not None:
            keep = ~clip(centre, verts[q])
            q, centre = q[keep], centre[keep]
        fn = np.cross(verts[q[:, 1]] - verts[q[:, 0]], verts[q[:, 3]] - verts[q[:, 0]])
        axis = np.argmax(np.abs(fn), axis=1)
        out = {}
        keys = np.floor(centre[:, [0, 2]] / chunk).astype(int)
        for key in np.unique(keys, axis=0):
            sel = (keys == key).all(axis=1)
            qq, ax = q[sel], axis[sel]
            p = verts[qq].reshape(-1, 3)
            n = nrm[qq].reshape(-1, 3)
            a4 = np.repeat(ax, 4)
            uv = np.where((a4 == 1)[:, None], p[:, [0, 2]], np.where((a4 == 0)[:, None], p[:, [2, 1]], p[:, [0, 1]])) * uv_scale
            uv = uv * np.array([1.0, -1.0])
            base = np.arange(len(qq)) * 4
            idx = np.stack([base, base + 1, base + 2, base, base + 2, base + 3], axis=1).reshape(-1)
            out[(int(key[0]), int(key[1]))] = Part(p, n, uv, idx, material)
        return out


def noise3(shape, freq, seed):
    """Smooth value noise on a 3D grid, 0..1. freq = lattice cells per grid step."""
    rng = np.random.default_rng(seed)
    size = [int(np.ceil(s * freq)) + 2 for s in shape]
    lat = rng.random(size, dtype=np.float32)
    ax = [np.arange(s, dtype=np.float32) * freq for s in shape]
    i0 = [np.floor(a).astype(int) for a in ax]
    fr = [a - np.floor(a) for a in ax]
    fr = [x * x * (3 - 2 * x) for x in fr]
    out = np.zeros(shape, dtype=np.float32)
    for dx in (0, 1):
        wx = (fr[0] if dx else 1 - fr[0])[:, None, None]
        for dy in (0, 1):
            wy = (fr[1] if dy else 1 - fr[1])[None, :, None]
            for dz in (0, 1):
                wz = (fr[2] if dz else 1 - fr[2])[None, None, :]
                out += lat[np.ix_(i0[0] + dx, i0[1] + dy, i0[2] + dz)] * wx * wy * wz
    return out
