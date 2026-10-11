"""
Suburban house generator (Hollowmere). A House is a worldkit.Building with a rectangular main
block: foundation, sided exterior walls with windows and doors, roof, porch, chimney - and for
houses that can be entered, floors, rooms (wall linings, skirting, floor and ceiling finishes),
partitions and stairs.

Local frame: front facade on y = 0 facing -Y, the house extends to +Y (depth d), x from -w/2 to
w/2, z = 0 at yard level. Floor i (0 = ground) is at base + i * fh; the basement floor at
base - basement_h.
"""
import math

import worldkit as K
from worldkit import T_EXT, T_INT, TRIM

SIDES = ('front', 'back', 'left', 'right')


class House(K.Building):
    def __init__(self, name, matrix, w, d, floors=2, base=0.6, fh=2.9, siding='MI_Siding_White',
                 foundation='MI_Brick_Red', trim=TRIM, wall_in='MI_Paint_Cream', basement_h=0.0, interior=False,
                 window_backdrop='curtain', glow=True, glow_tags=None):
        super().__init__(name, matrix)
        self.w, self.d = w, d
        self.floors = floors
        self.base, self.fh = base, fh
        self.siding, self.foundation, self.trim_mat, self.wall_in = siding, foundation, trim, wall_in
        self.basement_h = basement_h
        self.interior = interior
        self.window_backdrop = window_backdrop
        self.glow = glow
        self.glow_tags = list(glow_tags or [])
        self.ze = base + floors * fh            # eave / top of the walls
        self.walls = {s: [] for s in SIDES}     # openings per exterior side
        self.inner = []                         # all openings incl. partitions: dict(axis, c0, c1, ...)

    # -- geometry helpers ---------------------------------------------------------------
    def side_frame(self, side):
        """(axis, a0, a1, c_ext, c_int) of an exterior wall."""
        w2, d = self.w / 2, self.d
        if side == 'front':
            return 'X', -w2, w2, 0.0, T_EXT
        if side == 'back':
            return 'X', -w2, w2, d, d - T_EXT
        if side == 'left':
            return 'Y', T_EXT, d - T_EXT, -w2, -w2 + T_EXT
        return 'Y', T_EXT, d - T_EXT, w2, w2 - T_EXT

    def floor_z(self, i):
        if i < 0:
            return self.base - self.basement_h
        return self.base + i * self.fh

    # -- openings -----------------------------------------------------------------------
    def window(self, side, a, w=0.9, h=1.4, floor=0, sill=0.85, **kw):
        z0 = self.floor_z(floor) + sill
        kw.setdefault('trim', self.trim_mat)
        o = dict(kind='window', a0=a - w / 2, a1=a + w / 2, z0=z0, z1=z0 + h, **kw)
        if not self.interior:
            o.setdefault('backdrop', self.window_backdrop)
            if self.glow and floor >= 0:
                o.setdefault('glow', True)
                o.setdefault('glow_tags', self.glow_tags)
        self.walls[side].append(o)
        return o

    def door(self, side, a, w=0.92, h=2.06, floor=0, **kw):
        z0 = self.floor_z(floor)
        kw.setdefault('trim', self.trim_mat)
        o = dict(kind='door', a0=a - w / 2, a1=a + w / 2, z0=z0, z1=z0 + h, **kw)
        self.walls[side].append(o)
        axis, _, _, c_ext, c_int = self.side_frame(side)
        # Door actor transform (hinge at a0 on the wall centre line, leaf along the wall).
        c = (c_ext + c_int) / 2
        if axis == 'X':
            hinge = (o['a0'] + 0.0, c, z0)
            rot = 0 if side == 'front' else 180
            if side == 'back':
                hinge = (o['a1'], c, z0)
        else:
            hinge = (c, o['a1'] if side == 'left' else o['a0'], z0)
            rot = -90 if side == 'left' else 90
        o['hinge'] = hinge
        o['rot'] = rot
        return o

    def hole(self, side, a0, a1, z0, z1):
        """Plain opening without trim (e.g. for a basement bulkhead)."""
        o = dict(kind='hole', a0=a0, a1=a1, z0=z0, z1=z1)
        self.walls[side].append(o)
        return o

    # -- exterior -----------------------------------------------------------------------
    def build_exterior(self, corner_boards=True):
        b = self.shell
        bottom = -0.35 if self.basement_h <= 0 else self.floor_z(-1) - 0.3
        for side in SIDES:
            axis, a0, a1, c_ext, c_int = self.side_frame(side)
            inward = 1 if c_int > c_ext else -1
            ops = self.walls[side]
            # Foundation band (and basement walls) up to the ground floor.
            K.wall(b, axis, a0, a1, c_ext, c_int, bottom, self.base, ops, mat_c0=self.foundation,
                   mat_c1='MI_Concrete_Raw' if self.basement_h > 0 else self.foundation)
            # Water table board on top of the foundation.
            K._box(self.trim, axis, a0 - (0.03 if axis == 'X' else 0), a1 + (0.03 if axis == 'X' else 0),
                   c_ext, c_ext - inward * 0.03, self.base - 0.02, self.base + 0.14, self.trim_mat)
            K.wall(b, axis, a0, a1, c_ext, c_int, self.base, self.ze, ops, mat_c0=self.siding, mat_c1=self.wall_in,
                   split=c_ext + inward * 0.04)
            # Frieze board under the eaves.
            K._box(self.trim, axis, a0 - (0.03 if axis == 'X' else 0), a1 + (0.03 if axis == 'X' else 0),
                   c_ext, c_ext - inward * 0.025, self.ze - 0.28, self.ze, self.trim_mat)
            for o in ops:
                if o['kind'] != 'hole':
                    self.add_opening_trim(axis, o, c_ext, c_int, inside_finish=self.interior)
                self.inner.append(dict(o, axis=axis, c0=min(c_ext, c_int), c1=max(c_ext, c_int)))
        if not self.interior:
            # Hollow house: dim floors/ceilings so oblique looks past the curtains read as rooms.
            w2 = self.w / 2
            for i in range(self.floors + 1):
                z = self.floor_z(i)
                self.shell.box_minmax((-w2 + T_EXT, T_EXT, z - 0.25), (w2 - T_EXT, self.d - T_EXT, z),
                                      'MI_Wood_Floor_Dark' if i < self.floors else 'MI_Plaster')
        if corner_boards:
            w2, d = self.w / 2, self.d
            z0, z1 = self.base + 0.14, self.ze - 0.28
            for x in (-w2, w2):
                for y in (0.0, d):
                    sx = -1 if x < 0 else 1          # outward along X
                    sy = -1 if y == 0 else 1         # outward along Y
                    # Board on the X-facing side (lies on x, 12 cm along Y) and on the Y-facing side.
                    self.trim.box_minmax((min(x, x + sx * 0.03), min(y, y - sy * 0.12), z0),
                                         (max(x, x + sx * 0.03), max(y, y - sy * 0.12), z1), self.trim_mat)
                    self.trim.box_minmax((min(x + sx * 0.03, x - sx * 0.12), min(y, y + sy * 0.03), z0),
                                         (max(x + sx * 0.03, x - sx * 0.12), max(y, y + sy * 0.03), z1),
                                         self.trim_mat)

    def build_roof(self, kind='gable', pitch=35.0, roof='MI_Roof_Shingles_Dark', ridge_axis='X', overhang=0.4,
                   gable_mat=None):
        w2 = self.w / 2
        if kind == 'gable':
            return K.gable_roof(self.shell, -w2, w2, 0.0, self.d, self.ze, pitch, roof, self.trim_mat,
                                gable_mat or self.siding, overhang=overhang, ridge_axis=ridge_axis)
        return K.hip_roof(self.shell, -w2, w2, 0.0, self.d, self.ze, pitch, roof, self.trim_mat, overhang=overhang)

    def attic_floor(self):
        """Ceiling of the top floor (seen from inside) and attic floor."""
        w2 = self.w / 2
        self.shell.box_minmax((-w2 + T_EXT, T_EXT, self.ze - 0.02), (w2 - T_EXT, self.d - T_EXT, self.ze + 0.2),
                              'MI_Plaster')

    def porch(self, x0, x1, depth, roof=True, steps=(None, None), rail=True, posts=None, roof_mat=None,
              deck='MI_Wood_Planks', step_side='front'):
        """Front porch from y = -depth to 0 between x0 and x1, deck at floor level."""
        b = self.shell
        zt = self.base
        b.box_minmax((x0, -depth, -0.3), (x1, 0.0, zt - 0.04), self.foundation)
        for i in range(int((x1 - x0) / 0.14)):
            xa = x0 + i * 0.14
            b.box_minmax((xa + 0.003, -depth - 0.03, zt - 0.04), (min(x1, xa + 0.137), 0.0, zt), deck)
        s0, s1 = steps
        if s0 is None:
            s0, s1 = (x0 + x1) / 2 - 0.7, (x0 + x1) / 2 + 0.7
        n = max(2, int(round(zt / 0.17)))
        run = 0.28
        for i in range(n):
            z = zt - (i + 1) * zt / n
            y_front = -depth - (i + 1) * run
            b.box_minmax((s0, y_front, -0.3), (s1, -depth, z + zt / n - 0.03), self.foundation)
            b.box_minmax((s0 - 0.02, y_front - 0.025, z + zt / n - 0.03), (s1 + 0.02, -depth, z + zt / n), deck)
        if rail:
            h = zt
            pts_l = [(x0 + 0.05, -0.05, h), (x0 + 0.05, -depth + 0.05, h), (s0, -depth + 0.05, h)]
            pts_r = [(s1, -depth + 0.05, h), (x1 - 0.05, -depth + 0.05, h), (x1 - 0.05, -0.05, h)]
            for pts in (pts_l, pts_r):
                if abs(pts[-1][0] - pts[0][0]) > 0.1 or abs(pts[-1][1] - pts[0][1]) > 0.1:
                    K.railing(self.trim, pts, height=0.9, m_post=self.trim_mat, m_rail=self.trim_mat,
                              baluster_mat=self.trim_mat)
        if roof:
            zr = self.base + self.fh - 0.25
            for x in (posts or (x0 + 0.12, x1 - 0.12)):
                K.column(self.trim, x, -depth + 0.12, zt, zr, m=self.trim_mat)
            self.trim.box_minmax((x0, -depth, zr - 0.25), (x1, -depth + 0.24, zr), self.trim_mat)
            K.shed_roof(self.shell, x0 - 0.15, x1 + 0.15, -depth - 0.35, 0.0, zr, zr + 0.55,
                        roof_mat or 'MI_Roof_Shingles_Dark', self.trim_mat)
            # Porch ceiling.
            self.shell.box_minmax((x0, -depth, zr - 0.03), (x1, 0.0, zr), 'MI_Wood_Planks')
        return (s0 + s1) / 2, -depth - n * run

    # -- interior -----------------------------------------------------------------------
    def slab(self, rects, z, t=0.25, under='MI_Plaster', top='MI_Wood_Raw'):
        """Structural floor slab at level z (top), as rectangles (x0, y0, x1, y1)."""
        for x0, y0, x1, y1 in rects:
            self.shell.box_minmax((x0, y0, z - t), (x1, y1, z - 0.02), under)
            self.shell.box_minmax((x0, y0, z - 0.02), (x1, y1, z), top)

    def partition(self, axis, a0, a1, c, z0, z1, openings=(), mat_lo='MI_Paint_White', mat_hi=None, t=T_INT):
        """Interior wall centred on c. openings: dicts with a0/a1/z0/z1 (+ kind 'idoor'/'arch'/'hole')."""
        c0, c1 = c - t / 2, c + t / 2
        K.wall(self.shell, axis, a0, a1, c0, c1, z0, z1, openings, mat_c0=mat_lo, mat_c1=mat_hi or mat_lo)
        for o in openings:
            if o.get('kind', 'hole') != 'hole':
                self.add_opening_trim(axis, dict(o, kind=o.get('kind', 'idoor')), c1, c0)
            self.inner.append(dict(o, axis=axis, c0=c0, c1=c1))

    def room(self, x0, x1, y0, y1, z0, z1, walls='MI_Paint_White', floor='MI_Wood_Floor', ceiling='MI_Plaster',
             skirting=TRIM, crown=False, wainscot=None, lining=True, open_sides=()):
        """Finishes for a room whose INNER faces are x0..x1, y0..y1 between floor z0 and ceiling z1:
        floor/ceiling surfaces, wall linings (cut around openings), skirting boards. open_sides
        ('x0', 'x1', 'y0', 'y1') are sides without a wall (the room continues into a hall)."""
        b = self.shell
        b.box_minmax((x0, y0, z0), (x1, y1, z0 + 0.012), floor)
        if ceiling:
            b.box_minmax((x0, y0, z1 - 0.012), (x1, y1, z1), ceiling)
        sides = (('X', x0, x1, y0, 1, 'y0'), ('X', x0, x1, y1, -1, 'y1'), ('Y', y0, y1, x0, 1, 'x0'),
                 ('Y', y0, y1, x1, -1, 'x1'))
        for axis, a0, a1, c, inward, key in sides:
            if key in open_sides:
                continue
            ops = [o for o in self.inner
                   if o['axis'] == axis and o['c0'] - 0.03 <= c <= o['c1'] + 0.03 and o['a1'] > a0 and o['a0'] < a1
                   and o['z1'] > z0 and o['z0'] < z1]
            if lining:
                if wainscot:
                    wm, wh = wainscot
                    K.wall(b, axis, a0, a1, c, c + inward * 0.012, z0, z0 + wh, ops, mat_c0=wm)
                    K.wall(b, axis, a0, a1, c, c + inward * 0.012, z0 + wh, z1, ops, mat_c0=walls)
                    K.wall(self.trim, axis, a0, a1, c + inward * 0.012, c + inward * 0.03, z0 + wh - 0.02, z0 + wh + 0.04,
                           [o for o in ops if o['z0'] < z0 + wh + 0.04 and o['z1'] > z0 + wh - 0.02], mat_c0=skirting)
                else:
                    K.wall(b, axis, a0, a1, c, c + inward * 0.012, z0, z1, ops, mat_c0=walls)
            if skirting:
                doors = [o for o in ops if o['z0'] <= z0 + 0.05]
                K.wall(self.trim, axis, a0, a1, c + inward * 0.012, c + inward * 0.03, z0, z0 + 0.12, doors,
                       mat_c0=skirting)
            if crown:
                K.wall(self.trim, axis, a0, a1, c + inward * 0.012, c + inward * 0.05, z1 - 0.08, z1,
                       [o for o in ops if o['z1'] > z1 - 0.08], mat_c0=skirting)


def place_glows(building, static_m):
    """Lit-window cards (hidden by day, switched by the clock via the HH_WindowLight tag).
    static_m(name, matrix, collection, tags, shadow) places one static mesh actor."""
    for m, w, h, tags in building.glows:
        static_m('SM_Window_Glow', building.M @ m @ K.mat(scale=(w, 1.0, h)), 'Windows',
                 ['HH_WindowLight'] + list(tags), False)
