"""Architecture toolkit: rooms with openings, doorways, stairs and simple furniture built from quads.

Coordinates: meters, +Y up. Rooms are axis aligned boxes of air; faces look into the room.
"""
import numpy as np
from meshutil import Builder, Part


def rect_minus(a0, a1, h0, h1, holes):
    """Sub-rectangles of [a0,a1]x[h0,h1] that are not covered by holes (each: a0, a1, h0, h1)."""
    xs = sorted(set([a0, a1] + [min(a1, max(a0, v)) for h in holes for v in h[:2]]))
    ys = sorted(set([h0, h1] + [min(h1, max(h0, v)) for h in holes for v in h[2:4]]))
    out = []
    for j in range(len(ys) - 1):
        run = None
        for i in range(len(xs) - 1):
            cx, cy = (xs[i] + xs[i + 1]) * 0.5, (ys[j] + ys[j + 1]) * 0.5
            covered = any(h[0] < cx < h[1] and h[2] < cy < h[3] for h in holes)
            if covered:
                if run:
                    out.append(run)
                run = None
            elif run:
                run = (run[0], xs[i + 1], run[2], run[3])
            else:
                run = (xs[i], xs[i + 1], ys[j], ys[j + 1])
        if run:
            out.append(run)
    return out


class Arch:
    def __init__(self, uv_scale=None):
        self.b = Builder()
        self.uv = uv_scale or {}

    def _s(self, material):
        return self.uv.get(material, 0.5)

    # ------------------------------------------------------------------ single faces
    def face(self, material, axis, sign, pos, u0, u1, v0, v1):
        """Axis aligned rectangle. axis 'x': u = z, v = y; axis 'z': u = x, v = y; axis 'y': u = x, v = z.
        sign: direction of the normal along the axis."""
        if u1 - u0 < 1e-6 or v1 - v0 < 1e-6:
            return
        s = self._s(material)
        if axis == 'x':
            q = [(pos, v0, u1), (pos, v0, u0), (pos, v1, u0), (pos, v1, u1)] if sign > 0 else [(pos, v0, u0), (pos, v0, u1), (pos, v1, u1), (pos, v1, u0)]
        elif axis == 'z':
            q = [(u0, v0, pos), (u1, v0, pos), (u1, v1, pos), (u0, v1, pos)] if sign > 0 else [(u1, v0, pos), (u0, v0, pos), (u0, v1, pos), (u1, v1, pos)]
        else:
            q = [(u0, pos, v1), (u1, pos, v1), (u1, pos, v0), (u0, pos, v0)] if sign > 0 else [(u0, pos, v0), (u1, pos, v0), (u1, pos, v1), (u0, pos, v1)]
        self.b.quad(material, *q, uv_scale=s)

    def wall(self, material, axis, sign, pos, a0, a1, h0, h1, holes=()):
        for r in rect_minus(a0, a1, h0, h1, list(holes)):
            self.face(material, axis, sign, pos, *r)

    # ------------------------------------------------------------------ rooms
    def room(self, lo, hi, wall, floor, ceil, openings=None, skirting=None, wall_top=None):
        """Box of air lo..hi. openings: {'x-': [(z0, z1, y0, y1)], 'x+': ..., 'z-': [(x0, x1, y0, y1)], 'z+': ...,
        'y-': [(x0, x1, z0, z1)], 'y+': ...}. wall may be a dict per side. skirting: (material, height)."""
        o = openings or {}
        x0, y0, z0 = lo
        x1, y1, z1 = hi
        wm = (lambda s: wall.get(s, wall.get('*'))) if isinstance(wall, dict) else (lambda s: wall)
        sides = (('x-', 'x', 1, x0, z0, z1), ('x+', 'x', -1, x1, z0, z1), ('z-', 'z', 1, z0, x0, x1), ('z+', 'z', -1, z1, x0, x1))
        for key, axis, sign, pos, a0, a1 in sides:
            holes = list(o.get(key, ()))
            if skirting:
                sm, sh = skirting
                self.wall(sm, axis, sign, pos, a0, a1, y0, y0 + sh, holes)
                self.wall(wm(key), axis, sign, pos, a0, a1, y0 + sh, y1, holes)
            else:
                self.wall(wm(key), axis, sign, pos, a0, a1, y0, y1, holes)
        if floor:
            self.wall(floor, 'y', 1, y0, x0, x1, z0, z1, o.get('y-', ()))
        if ceil:
            self.wall(ceil, 'y', -1, y1, x0, x1, z0, z1, o.get('y+', ()))

    def doorway(self, axis, p0, p1, a0, a1, h0, h1, material, floor=None):
        """Passage through a wall between the planes axis = p0 and axis = p1 (p0 < p1)."""
        other = 'z' if axis == 'x' else 'x'
        self.face(material, other, 1, a0, p0, p1, h0, h1)
        self.face(material, other, -1, a1, p0, p1, h0, h1)
        if axis == 'x':
            self.face(material, 'y', -1, h1, p0, p1, a0, a1)
            if floor:
                self.face(floor, 'y', 1, h0, p0, p1, a0, a1)
        else:
            self.face(material, 'y', -1, h1, a0, a1, p0, p1)
            if floor:
                self.face(floor, 'y', 1, h0, a0, a1, p0, p1)

    # ------------------------------------------------------------------ solids
    def box(self, material, lo, hi, faces='xXyYzZ', materials=None):
        self.b.box(material, lo, hi, self._s(material), faces, materials)

    def obox(self, material, centre, half, yaw_deg=0.0, pitch_deg=0.0):
        """Oriented solid box (yaw around Y, then pitch around the local X axis)."""
        tmp = Builder()
        h = np.asarray(half, dtype=np.float64)
        tmp.box(material, -h, h, self._s(material))
        part = tmp.to_parts()[0]
        cy, sy = np.cos(np.radians(yaw_deg)), np.sin(np.radians(yaw_deg))
        cp, sp = np.cos(np.radians(pitch_deg)), np.sin(np.radians(pitch_deg))
        ry = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]])
        rx = np.array([[1, 0, 0], [0, cp, -sp], [0, sp, cp]])
        m = np.eye(4)
        m[:3, :3] = ry @ rx
        m[:3, 3] = centre
        part.transform(m)
        self.b.add_part(part)

    def cylinder(self, material, base, radius, height, segs=14, top_radius=None, caps=True):
        base = np.asarray(base, dtype=np.float64)
        r1 = radius if top_radius is None else top_radius
        s = self._s(material)
        ang = np.linspace(0, 2 * np.pi, segs + 1)
        ring = np.stack([np.cos(ang), np.zeros_like(ang), np.sin(ang)], axis=1)
        pos = np.concatenate([base + ring * radius, base + ring * r1 + [0, height, 0]])
        nrm = np.concatenate([ring, ring])
        u = ang / (2 * np.pi) * (2 * np.pi * radius) * s
        uv = np.concatenate([np.stack([u, np.zeros_like(u)], axis=1), np.stack([u, np.full_like(u, -height * s)], axis=1)])
        idx = []
        for i in range(segs):
            a, b, c, d = i, i + 1, segs + 1 + i + 1, segs + 1 + i
            idx += [a, d, c, a, c, b]
        self.b.add_part(Part(pos, nrm, uv, idx, material))
        if caps:
            for y, r, up in ((height, r1, True), (0.0, radius, False)):
                if r < 1e-4:
                    continue
                centre = base + [0, y, 0]
                p = np.concatenate([[centre], centre + ring[:-1] * r])
                n = np.tile([0.0, 1.0 if up else -1.0, 0.0], (len(p), 1))
                tri = []
                for i in range(segs):
                    a, b = 1 + i, 1 + (i + 1) % segs
                    tri += [0, b, a] if up else [0, a, b]
                self.b.add_part(Part(p, n, p[:, [0, 2]] * s, tri, material))

    def stairs(self, material, start, direction, steps, rise, run, width, solid_to=None, tread=None):
        """Steps going DOWN from `start` (top edge, centre of the first tread's upper end) along `direction`
        ('x+', 'x-', 'z+', 'z-'). Each step is a solid block down to solid_to (or one rise deep)."""
        sx, sy, sz = start
        d = {'x+': (1, 0), 'x-': (-1, 0), 'z+': (0, 1), 'z-': (0, -1)}[direction]
        for i in range(steps):
            top = sy - (i + 1) * rise
            a0, a1 = i * run, (i + 1) * run
            bottom = solid_to if solid_to is not None else top - rise
            if d[0]:
                x0, x1 = sorted((sx + d[0] * a0, sx + d[0] * a1))
                lo, hi = (x0, bottom, sz - width / 2), (x1, top, sz + width / 2)
            else:
                z0, z1 = sorted((sz + d[1] * a0, sz + d[1] * a1))
                lo, hi = (sx - width / 2, bottom, z0), (sx + width / 2, top, z1)
            self.b.box(material, lo, hi, self._s(material), 'xXYzZ', {'Y': tread or material})

    # ------------------------------------------------------------------ furniture (all static decoration)
    def table(self, wood, x, y, z, w=1.4, d=0.8, h=0.78, yaw=0.0):
        self.obox(wood, (x, y + h - 0.025, z), (w / 2, 0.025, d / 2), yaw)
        c, s = np.cos(np.radians(yaw)), np.sin(np.radians(yaw))
        for lx, lz in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            ox, oz = lx * (w / 2 - 0.07), lz * (d / 2 - 0.07)
            self.obox(wood, (x + ox * c + oz * s, y + (h - 0.05) / 2, z - ox * s + oz * c), (0.035, (h - 0.05) / 2, 0.035), yaw)

    def chair(self, wood, x, y, z, yaw=0.0, fallen=False):
        if fallen:
            self.obox(wood, (x, y + 0.22, z), (0.22, 0.02, 0.22), yaw, 78)
            self.obox(wood, (x, y + 0.05, z + 0.3), (0.22, 0.02, 0.25), yaw, 8)
            return
        self.obox(wood, (x, y + 0.45, z), (0.22, 0.02, 0.22), yaw)
        c, s = np.cos(np.radians(yaw)), np.sin(np.radians(yaw))
        for lx, lz in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            ox, oz = lx * 0.19, lz * 0.19
            self.obox(wood, (x + ox * c + oz * s, y + 0.215, z - ox * s + oz * c), (0.02, 0.215, 0.02), yaw)
        self.obox(wood, (x - 0.2 * s, y + 0.75, z - 0.2 * c), (0.22, 0.28, 0.018), yaw)

    def shelf(self, wood, x, y, z, w=1.2, h=1.9, d=0.35, yaw=0.0, boards=4):
        c, s = np.cos(np.radians(yaw)), np.sin(np.radians(yaw))
        for side in (-1, 1):
            ox = side * (w / 2 - 0.02)
            self.obox(wood, (x + ox * c, y + h / 2, z - ox * s), (0.02, h / 2, d / 2), yaw)
        for k in range(boards + 1):
            self.obox(wood, (x, y + 0.05 + k * (h - 0.1) / boards, z), (w / 2, 0.018, d / 2), yaw)
        self.obox(wood, (x - (d / 2 - 0.01) * s, y + h / 2, z - (d / 2 - 0.01) * c), (w / 2, h / 2, 0.01), yaw)

    def crate(self, wood, x, y, z, size=0.7, yaw=0.0):
        self.obox(wood, (x, y + size / 2, z), (size / 2, size / 2, size / 2), yaw)
        for k in (-1, 1):
            self.obox(wood, (x, y + size / 2 + k * (size / 2 - 0.05), z), (size / 2 + 0.015, 0.04, size / 2 + 0.015), yaw)

    def barrel(self, wood, metal, x, y, z, r=0.33, h=0.95):
        self.cylinder(wood, (x, y, z), r * 0.88, h * 0.5, 12, r, caps=False)
        self.cylinder(wood, (x, y + h * 0.5, z), r, h * 0.5, 12, r * 0.88, caps=True)
        for k in (0.16, 0.5, 0.84):
            rr = r * (0.88 + 0.12 * (1 - abs(k - 0.5) * 2)) + 0.012
            self.cylinder(metal, (x, y + h * k - 0.025, z), rr, 0.05, 12, caps=False)

    def beam(self, wood, a, b, thick=0.2):
        a, b = np.asarray(a, dtype=np.float64), np.asarray(b, dtype=np.float64)
        d = b - a
        length = float(np.linalg.norm(d))
        yaw = np.degrees(np.arctan2(d[0], d[2]))
        pitch = -np.degrees(np.arcsin(d[1] / max(length, 1e-9)))
        self.obox(wood, (a + b) * 0.5, (thick / 2, thick / 2, length / 2), yaw, pitch)

    def parts(self):
        return self.b.to_parts()
