"""
Architecture kit for the town of Hollowmere: walls with openings, windows, doors, roofs, stairs,
porches and railings. Everything is built into hhblend.Builder meshes in a building's local frame
(metres, +Y = into the building from its front, Z up) and placed by build_world.py.

Conventions
  * A Building keeps three builders: the opaque shell (walls, floors, roof - Nanite), the trim
    (window/door casings, sashes, glass, curtains - not Nanite, translucent glass) and nothing else;
    furniture and gameplay actors are placed separately as their own actors.
  * Openings are dicts: kind ('window', 'door', 'arch', 'shop', 'garage', 'hole'), a0/a1 along the
    wall, z0/z1 heights. Windows may carry 'backdrop' ('curtain' = closed curtains behind the glass,
    'shop' = shop interior image, None = see into a real room) and 'glow' (True = a lit window card
    behind the glass that the clock switches on in the evening).
"""
import math

from mathutils import Euler, Matrix, Vector

from hhblend import Builder, _box_uv
from props import prop

T_EXT = 0.25        # exterior walls
T_INT = 0.12        # partitions
TRIM = 'MI_Trim_White'


def mat(loc=(0, 0, 0), rot=(0, 0, 0), scale=1.0):
    if not isinstance(rot, (tuple, list)):
        rot = (0, 0, rot)
    s = scale if isinstance(scale, (tuple, list)) else (scale, scale, scale)
    return (Matrix.Translation(loc) @ Euler([math.radians(a) for a in rot], 'XYZ').to_matrix().to_4x4()
            @ Matrix.Diagonal((*s, 1.0)))


# =======================================================================================
# Free-form faces with planar UVs (roofs, gables, ramps)

def faces(b, polys, mat_name, u_dir, v_dir, origin=(0, 0, 0), smooth=False):
    """Add planar polygons (lists of 3D points) with UVs projected on (u_dir, v_dir) in metres."""
    pb, uv = b._begin()
    o = Vector(origin)
    u = Vector(u_dir).normalized()
    v = Vector(v_dir).normalized()
    for poly in polys:
        verts = [pb.verts.new(Vector(p)) for p in poly]
        f = pb.faces.new(verts)
        for loop in f.loops:
            d = loop.vert.co - o
            loop[uv].uv = (d.dot(u), d.dot(v))
    pb.normal_update()
    b._commit(pb, mat_name, smooth=smooth)


def slab(b, poly, thickness, top_mat, under_mat=None, edge_mat=None, u_dir=(1, 0, 0), v_dir=None):
    """A planar polygon (counter-clockwise seen from above / from its outside) extruded by
    'thickness' against its normal. The top gets planar UVs along u_dir / v_dir."""
    pts = [Vector(p) for p in poly]
    n = (pts[1] - pts[0]).cross(pts[2] - pts[0]).normalized()
    u = Vector(u_dir).normalized()
    u = (u - n * u.dot(n)).normalized()
    v = Vector(v_dir).normalized() if v_dir else n.cross(u)
    low = [p - n * thickness for p in pts]
    faces(b, [pts], top_mat, u, v, pts[0])
    faces(b, [list(reversed(low))], under_mat or top_mat, u, v, pts[0])
    sides = []
    k = len(pts)
    for i in range(k):
        j = (i + 1) % k
        sides.append([pts[j], pts[i], low[i], low[j]])
    pb, uv = b._begin()
    for poly_ in sides:
        verts = [pb.verts.new(p) for p in poly_]
        pb.faces.new(verts)
    pb.normal_update()
    _box_uv(pb.faces, uv)
    b._commit(pb, edge_mat or under_mat or top_mat, smooth=False)


def tri_prism_x(b, x0, x1, y0, y1, z0, z1, mat_name, ym=None):
    """Gable-end triangle (base y0..y1 at z0, apex at ym, z1) extruded along X from x0 to x1."""
    ym = (y0 + y1) / 2 if ym is None else ym
    poly = [(y0, z0), (y1, z0), (ym, z1)]
    b.prism(poly, x1 - x0, loc=((x0 + x1) / 2, 0, 0), mat=mat_name, axis='X')


def tri_prism_y(b, y0, y1, x0, x1, z0, z1, mat_name, xm=None):
    """Gable-end triangle in the XZ plane (base x0..x1 at z0, apex xm, z1) extruded along Y."""
    xm = (x0 + x1) / 2 if xm is None else xm
    poly = [(x0, z0), (x1, z0), (xm, z1)]
    b.prism(poly, y1 - y0, loc=(0, (y0 + y1) / 2, 0), mat=mat_name, axis='Y')


# =======================================================================================
# Walls

def _spans(a0, a1, z0, z1, openings):
    """Split a wall into solid boxes around rectangular holes. Yields (s0, s1, zz0, zz1)."""
    cuts = sorted({a0, a1, *[o['a0'] for o in openings], *[o['a1'] for o in openings]})
    cuts = [c for c in cuts if a0 - 1e-6 <= c <= a1 + 1e-6]
    for s0, s1 in zip(cuts, cuts[1:]):
        if s1 - s0 < 1e-4:
            continue
        holes = sorted((o['z0'], o['z1']) for o in openings if o['a0'] <= s0 + 1e-4 and o['a1'] >= s1 - 1e-4)
        z = z0
        for h0, h1 in holes:
            if h0 > z + 1e-4:
                yield s0, s1, z, min(h0, z1)
            z = max(z, h1)
        if z < z1 - 1e-4:
            yield s0, s1, z, z1


def _box(b, axis, a0, a1, c0, c1, z0, z1, m, **kw):
    if axis == 'X':
        b.box_minmax((a0, min(c0, c1), z0), (a1, max(c0, c1), z1), m, **kw)
    else:
        b.box_minmax((min(c0, c1), a0, z0), (max(c0, c1), a1, z1), m, **kw)


def wall(b, axis, a0, a1, c0, c1, z0, z1, openings=(), mat_c0='MI_Paint_White', mat_c1=None, split=None,
         base=None, base_h=0.12):
    """Wall along 'axis' ('X' or 'Y') from a0 to a1; thickness from c0 to c1 on the other axis.
    mat_c0 faces the c0 side, mat_c1 (optional) the c1 side, split at 'split' (default: middle, or
    4 cm from c0 when the c0 side is an exterior skin - pass split explicitly).
    base: (mat, side) adds a skirting board on that side ('c0'/'c1'/'both') except under doors."""
    openings = [o for o in openings if o['a1'] > a0 and o['a0'] < a1]
    if mat_c1 is None or mat_c1 == mat_c0:
        layers = [(c0, c1, mat_c0)]
    else:
        sp = (c0 + c1) / 2 if split is None else split
        layers = [(c0, sp, mat_c0), (sp, c1, mat_c1)]
    for s0, s1, zz0, zz1 in _spans(a0, a1, z0, z1, openings):
        for l0, l1, m in layers:
            _box(b, axis, s0, s1, l0, l1, zz0, zz1, m)
    if base:
        bmat, side = base
        sides = (c0, c1) if side == 'both' else ((c0,) if side == 'c0' else (c1,))
        doors = [o for o in openings if o['z0'] <= z0 + 0.05]
        for c in sides:
            out = -1 if c == min(c0, c1) else 1
            for s0, s1, _, _ in _spans(a0, a1, z0, z0 + base_h, doors):
                _box(b, axis, s0, s1, c, c + out * 0.018, z0, z0 + base_h, bmat)


# =======================================================================================
# Openings: casings, windows, shop fronts

def _frame_box(b, axis, a0, a1, c0, c1, z0, z1, m, bevel=0.0):
    _box(b, axis, a0, a1, c0, c1, z0, z1, m, bevel=bevel)


def casing(b, axis, o, c_face, out, width=0.09, proud=0.022, m=TRIM, head=0.12, sill=False, cap=True):
    """Flat trim around an opening on the wall face at c_face; 'out' = +1/-1 direction of the room
    or street that face looks at."""
    a0, a1, z0, z1 = o['a0'], o['a1'], o['z0'], o['z1']
    c1 = c_face + out * proud
    bot = z0 - (0.0 if not sill else 0.06)
    for aa0, aa1 in ((a0 - width, a0), (a1, a1 + width)):
        _frame_box(b, axis, aa0, aa1, c_face, c1, bot, z1 + head * 0.15, m, bevel=0.003)
    _frame_box(b, axis, a0 - width - 0.015, a1 + width + 0.015, c_face, c1, z1, z1 + head, m, bevel=0.003)
    if cap:
        _frame_box(b, axis, a0 - width - 0.03, a1 + width + 0.03, c_face, c1 + out * 0.03, z1 + head,
                   z1 + head + 0.03, m, bevel=0.004)
    if sill:
        _frame_box(b, axis, a0 - width - 0.03, a1 + width + 0.03, c_face - out * 0.02, c_face + out * 0.065,
                   z0 - 0.045, z0, m, bevel=0.004)
        _frame_box(b, axis, a0 - width * 0.6, a1 + width * 0.6, c_face, c_face + out * 0.02, z0 - 0.11, z0 - 0.045,
                   m, bevel=0.002)


def reveal(b, axis, o, c0, c1, m=TRIM, t=0.02, bottom=True):
    """Line the inside of an opening (jambs, head, optional stool) across the wall depth."""
    a0, a1, z0, z1 = o['a0'], o['a1'], o['z0'], o['z1']
    _frame_box(b, axis, a0, a0 + t, c0, c1, z0, z1, m)
    _frame_box(b, axis, a1 - t, a1, c0, c1, z0, z1, m)
    _frame_box(b, axis, a0, a1, c0, c1, z1 - t, z1, m)
    if bottom:
        _frame_box(b, axis, a0, a1, c0, c1, z0, z0 + t, m)


def sash_window(b, axis, o, c_mid, out, m=TRIM, glass='MI_Glass_Window', lites=(2, 2)):
    """Double-hung window: two sashes with muntins and glass, centred in the wall at c_mid."""
    a0, a1, z0, z1 = o['a0'] + 0.02, o['a1'] - 0.02, o['z0'] + 0.02, o['z1'] - 0.02
    zm = (z0 + z1) / 2
    fw, fd = 0.05, 0.035
    for k, (sz0, sz1) in enumerate(((z0, zm + 0.02), (zm - 0.02, z1))):
        cc = c_mid + out * (0.025 if k == 0 else -0.025)
        _frame_box(b, axis, a0, a1, cc - fd / 2, cc + fd / 2, sz0, sz0 + fw, m)
        _frame_box(b, axis, a0, a1, cc - fd / 2, cc + fd / 2, sz1 - fw, sz1, m)
        _frame_box(b, axis, a0, a0 + fw, cc - fd / 2, cc + fd / 2, sz0, sz1, m)
        _frame_box(b, axis, a1 - fw, a1, cc - fd / 2, cc + fd / 2, sz0, sz1, m)
        nx, nz = lites
        for i in range(1, nx):
            a = a0 + fw + (a1 - a0 - 2 * fw) * i / nx
            _frame_box(b, axis, a - 0.011, a + 0.011, cc - 0.012, cc + 0.012, sz0 + fw, sz1 - fw, m)
        for j in range(1, nz):
            z = sz0 + fw + (sz1 - sz0 - 2 * fw) * j / nz
            _frame_box(b, axis, a0 + fw, a1 - fw, cc - 0.012, cc + 0.012, z - 0.011, z + 0.011, m)
        _frame_box(b, axis, a0 + fw, a1 - fw, cc - 0.003, cc + 0.003, sz0 + fw, sz1 - fw, glass)


def picture_window(b, axis, o, c_mid, m=TRIM, glass='MI_Glass_Window', mullions=0, frame=0.06):
    a0, a1, z0, z1 = o['a0'], o['a1'], o['z0'], o['z1']
    fd = 0.05
    _frame_box(b, axis, a0, a1, c_mid - fd / 2, c_mid + fd / 2, z0, z0 + frame, m)
    _frame_box(b, axis, a0, a1, c_mid - fd / 2, c_mid + fd / 2, z1 - frame, z1, m)
    _frame_box(b, axis, a0, a0 + frame, c_mid - fd / 2, c_mid + fd / 2, z0, z1, m)
    _frame_box(b, axis, a1 - frame, a1, c_mid - fd / 2, c_mid + fd / 2, z0, z1, m)
    for i in range(1, mullions + 1):
        a = a0 + (a1 - a0) * i / (mullions + 1)
        _frame_box(b, axis, a - frame / 2, a + frame / 2, c_mid - fd / 2, c_mid + fd / 2, z0, z1, m)
    _frame_box(b, axis, a0 + frame, a1 - frame, c_mid - 0.003, c_mid + 0.003, z0 + frame, z1 - frame, glass)


def curtain_backdrop(b, axis, o, c_back, out, m='MI_Fabric_Curtain_Light', folds=7):
    """Closed curtains just inside a window (for houses without interiors)."""
    a0, a1, z0, z1 = o['a0'] - 0.2, o['a1'] + 0.2, o['z0'] - 0.1, o['z1'] + 0.15
    n = folds * 2
    w = (a1 - a0) / n
    for i in range(n):
        depth = 0.05 if i % 2 == 0 else 0.0
        _frame_box(b, axis, a0 + i * w, a0 + (i + 1) * w, c_back + out * depth, c_back + out * (depth + 0.012),
                   z0, z1, m)
    _frame_box(b, axis, a0 - 0.05, a1 + 0.05, c_back + out * 0.06, c_back + out * 0.08, z1 + 0.02, z1 + 0.05,
               'MI_Brass')


def image_backdrop(b, axis, o, c_back, out, m='MI_Img_Shop_Interior', depth=0.9):
    """A painted 'room' behind shop glass: image on the back, dark sides and floor."""
    a0, a1, z0, z1 = o['a0'], o['a1'], o['z0'], o['z1']
    cb = c_back + out * depth
    if axis == 'X':
        b.plane(a1 - a0, z1 - z0, ((a0 + a1) / 2, cb, (z0 + z1) / 2), m, rot=(0, 0, 0 if out > 0 else 180))
    else:
        b.plane(a1 - a0, z1 - z0, (cb, (a0 + a1) / 2, (z0 + z1) / 2), m, rot=(0, 0, 90 if out < 0 else -90))
    _frame_box(b, axis, a0, a1, c_back, cb, z0 - 0.05, z0, 'MI_Wood_Dark')
    _frame_box(b, axis, a0 - 0.02, a0, c_back, cb, z0, z1, 'MI_Trim_Dark')
    _frame_box(b, axis, a1, a1 + 0.02, c_back, cb, z0, z1, 'MI_Trim_Dark')
    _frame_box(b, axis, a0, a1, c_back, cb, z1, z1 + 0.02, 'MI_Trim_Dark')


def shop_front(b, axis, o, c_face, out, m='MI_Trim_Dark', glass='MI_Glass_Shop', bulkhead=0.55, transom=0.55,
               mullions=1, door=None):
    """Storefront in an opening: bulkhead panel, frame, transom bar, big glass. door=(a0, a1) leaves a
    glazed (closed) door in the frame."""
    a0, a1, z0, z1 = o['a0'], o['a1'], o['z0'], o['z1']
    cc = c_face - out * 0.12
    fd = 0.07
    zb = z0 + bulkhead
    zt = z1 - transom
    segs = [(a0, a1)]
    if door:
        segs = [(a0, door[0]), (door[1], a1)]
    for s0, s1 in segs:
        if s1 - s0 < 0.05:
            continue
        _frame_box(b, axis, s0, s1, cc - 0.08, cc + 0.08, z0, zb, m, bevel=0.004)
        _frame_box(b, axis, s0, s1, cc - 0.1, cc + 0.1, zb - 0.04, zb, m, bevel=0.004)
        for i in range(mullions + 2):
            a = s0 + (s1 - s0) * i / (mullions + 1)
            _frame_box(b, axis, max(s0, a - 0.035), min(s1, a + 0.035), cc - fd / 2, cc + fd / 2, zb, zt, m)
        _frame_box(b, axis, s0, s1, cc - 0.003, cc + 0.003, zb, zt, glass)
    _frame_box(b, axis, a0, a1, cc - fd / 2, cc + fd / 2, zt, zt + 0.07, m)
    for i in range(4):
        a = a0 + (a1 - a0) * i / 3
        _frame_box(b, axis, max(a0, a - 0.03), min(a1, a + 0.03), cc - fd / 2, cc + fd / 2, zt, z1, m)
    _frame_box(b, axis, a0, a1, cc - 0.003, cc + 0.003, zt + 0.07, z1, glass)
    if door:
        d0, d1 = door
        _frame_box(b, axis, d0, d0 + 0.07, cc - fd / 2, cc + fd / 2, z0, zt, m)
        _frame_box(b, axis, d1 - 0.07, d1, cc - fd / 2, cc + fd / 2, z0, zt, m)
        _frame_box(b, axis, d0 + 0.07, d1 - 0.07, cc - 0.025, cc + 0.025, z0, z0 + 0.25, m)
        _frame_box(b, axis, d0 + 0.07, d1 - 0.07, cc - 0.025, cc + 0.025, zt - 0.1, zt, m)
        _frame_box(b, axis, d0 + 0.07, d1 - 0.07, cc - 0.003, cc + 0.003, z0 + 0.25, zt - 0.1, glass)
        _frame_box(b, axis, d1 - 0.2, d1 - 0.17, cc + out * 0.05, cc + out * 0.07, z0 + 0.8, z0 + 1.3, 'MI_Chrome')


# =======================================================================================
# Roofs

def gable_roof(b, x0, x1, y0, y1, ze, pitch, roof_mat, trim_mat=TRIM, gable_mat=None, overhang=0.35,
               gable_overhang=0.25, thickness=0.14, ridge_axis='X', wall_t=T_EXT):
    """Gable roof over the rectangle; eaves at height ze on the wall line. Returns the ridge height."""
    tp = math.tan(math.radians(pitch))
    if ridge_axis == 'X':
        ym = (y0 + y1) / 2
        zr = ze + (ym - y0) * tp
        ez = ze - overhang * tp
        xa, xb = x0 - gable_overhang, x1 + gable_overhang
        slab(b, [(xa, y0 - overhang, ez), (xb, y0 - overhang, ez), (xb, ym, zr), (xa, ym, zr)], thickness,
             roof_mat, 'MI_Wood_Planks', trim_mat, u_dir=(1, 0, 0))
        slab(b, [(xb, y1 + overhang, ez), (xa, y1 + overhang, ez), (xa, ym, zr), (xb, ym, zr)], thickness,
             roof_mat, 'MI_Wood_Planks', trim_mat, u_dir=(-1, 0, 0))
        if gable_mat:
            for gx in (x0, x1 - wall_t):
                tri_prism_x(b, gx, gx + wall_t, y0, y1, ze, zr - 0.02, gable_mat)
        # Fascia and ridge cap.
        for yy in (y0 - overhang, y1 + overhang):
            b.box_minmax((xa - 0.02, min(yy, yy + 0.03 * (1 if yy > ym else -1)), ez - 0.2),
                         (xb + 0.02, max(yy, yy + 0.03 * (1 if yy > ym else -1)), ez + 0.02), trim_mat)
        b.box_minmax((xa, ym - 0.09, zr - 0.02), (xb, ym + 0.09, zr + 0.06), roof_mat, bevel=0.02)
    else:
        xm = (x0 + x1) / 2
        zr = ze + (xm - x0) * tp
        ez = ze - overhang * tp
        ya, yb = y0 - gable_overhang, y1 + gable_overhang
        slab(b, [(x0 - overhang, yb, ez), (x0 - overhang, ya, ez), (xm, ya, zr), (xm, yb, zr)], thickness,
             roof_mat, 'MI_Wood_Planks', trim_mat, u_dir=(0, -1, 0))
        slab(b, [(x1 + overhang, ya, ez), (x1 + overhang, yb, ez), (xm, yb, zr), (xm, ya, zr)], thickness,
             roof_mat, 'MI_Wood_Planks', trim_mat, u_dir=(0, 1, 0))
        if gable_mat:
            for gy in (y0, y1 - wall_t):
                tri_prism_y(b, gy, gy + wall_t, x0, x1, ze, zr - 0.02, gable_mat)
        for xx in (x0 - overhang, x1 + overhang):
            s = 1 if xx > xm else -1
            b.box_minmax((min(xx, xx + 0.03 * s), ya - 0.02, ez - 0.2), (max(xx, xx + 0.03 * s), yb + 0.02, ez + 0.02),
                         trim_mat)
        b.box_minmax((xm - 0.09, ya, zr - 0.02), (xm + 0.09, yb, zr + 0.06), roof_mat, bevel=0.02)
    return zr


def hip_roof(b, x0, x1, y0, y1, ze, pitch, roof_mat, trim_mat=TRIM, overhang=0.4, thickness=0.14):
    """Hip roof over a rectangle (ridge along the longer side). Returns the ridge height."""
    tp = math.tan(math.radians(pitch))
    ez = ze - overhang * tp
    xa, xb, ya, yb = x0 - overhang, x1 + overhang, y0 - overhang, y1 + overhang
    w, d = xb - xa, yb - ya
    h = min(w, d) / 2
    zr = ez + h * tp
    if w >= d:
        r0, r1 = (xa + h, (ya + yb) / 2, zr), (xb - h, (ya + yb) / 2, zr)
        slab(b, [(xa, ya, ez), (xb, ya, ez), r1, r0], thickness, roof_mat, 'MI_Wood_Planks', trim_mat, (1, 0, 0))
        slab(b, [(xb, yb, ez), (xa, yb, ez), r0, r1], thickness, roof_mat, 'MI_Wood_Planks', trim_mat, (-1, 0, 0))
        slab(b, [(xa, yb, ez), (xa, ya, ez), r0], thickness, roof_mat, 'MI_Wood_Planks', trim_mat, (0, -1, 0))
        slab(b, [(xb, ya, ez), (xb, yb, ez), r1], thickness, roof_mat, 'MI_Wood_Planks', trim_mat, (0, 1, 0))
        if r1[0] - r0[0] > 0.05:
            b.box_minmax((r0[0], r0[1] - 0.09, zr - 0.02), (r1[0], r0[1] + 0.09, zr + 0.06), roof_mat, bevel=0.02)
    else:
        r0, r1 = ((xa + xb) / 2, ya + h, zr), ((xa + xb) / 2, yb - h, zr)
        slab(b, [(xa, ya, ez), (xb, ya, ez), r0], thickness, roof_mat, 'MI_Wood_Planks', trim_mat, (1, 0, 0))
        slab(b, [(xb, yb, ez), (xa, yb, ez), r1], thickness, roof_mat, 'MI_Wood_Planks', trim_mat, (-1, 0, 0))
        slab(b, [(xa, yb, ez), (xa, ya, ez), r0, r1], thickness, roof_mat, 'MI_Wood_Planks', trim_mat, (0, -1, 0))
        slab(b, [(xb, ya, ez), (xb, yb, ez), r1, r0], thickness, roof_mat, 'MI_Wood_Planks', trim_mat, (0, 1, 0))
    for yy, s in ((ya, -1), (yb, 1)):
        b.box_minmax((xa - 0.02, min(yy, yy + 0.03 * s), ez - 0.2), (xb + 0.02, max(yy, yy + 0.03 * s), ez + 0.02),
                     trim_mat)
    for xx, s in ((xa, -1), (xb, 1)):
        b.box_minmax((min(xx, xx + 0.03 * s), ya, ez - 0.2), (max(xx, xx + 0.03 * s), yb, ez + 0.02), trim_mat)
    return zr


def flat_roof(b, x0, x1, y0, y1, z, parapet=0.6, mat_top='MI_Concrete_Raw', mat_wall='MI_Brick',
              coping='MI_Concrete_Raw', t=0.25):
    b.box_minmax((x0, y0, z - 0.25), (x1, y1, z), mat_top)
    if parapet > 0:
        for lo, hi in (((x0, y0, z), (x1, y0 + t, z + parapet)), ((x0, y1 - t, z), (x1, y1, z + parapet)),
                       ((x0, y0, z), (x0 + t, y1, z + parapet)), ((x1 - t, y0, z), (x1, y1, z + parapet))):
            b.box_minmax(lo, hi, mat_wall)
        for lo, hi in (((x0 - 0.04, y0 - 0.04, z + parapet), (x1 + 0.04, y0 + t + 0.04, z + parapet + 0.06)),
                       ((x0 - 0.04, y1 - t - 0.04, z + parapet), (x1 + 0.04, y1 + 0.04, z + parapet + 0.06)),
                       ((x0 - 0.04, y0, z + parapet), (x0 + t + 0.04, y1, z + parapet + 0.06)),
                       ((x1 - t - 0.04, y0, z + parapet), (x1 + 0.04, y1, z + parapet + 0.06))):
            b.box_minmax(lo, hi, coping, bevel=0.01)


def shed_roof(b, x0, x1, y_low, y_high, z_low, z_high, roof_mat, trim_mat=TRIM, thickness=0.1):
    """Single-slope roof (porches, awnings); slopes down towards y_low."""
    slab(b, [(x0, y_low, z_low), (x1, y_low, z_low), (x1, y_high, z_high), (x0, y_high, z_high)]
         if y_high > y_low else
         [(x1, y_low, z_low), (x0, y_low, z_low), (x0, y_high, z_high), (x1, y_high, z_high)],
         thickness, roof_mat, 'MI_Wood_Planks', trim_mat, u_dir=(1, 0, 0) if y_high > y_low else (-1, 0, 0))
    s = 1 if y_low < y_high else -1
    b.box_minmax((x0 - 0.02, min(y_low, y_low - s * 0.03), z_low - 0.18), (x1 + 0.02, max(y_low, y_low - s * 0.03),
                 z_low + 0.02), trim_mat)


def chimney(b, x, y, z0, z1, w=0.7, d=0.55, m='MI_Brick_Red'):
    b.box_minmax((x - w / 2, y - d / 2, z0), (x + w / 2, y + d / 2, z1), m)
    b.box_minmax((x - w / 2 - 0.05, y - d / 2 - 0.05, z1), (x + w / 2 + 0.05, y + d / 2 + 0.05, z1 + 0.1),
                 'MI_Concrete_Raw', bevel=0.01)
    for dx in (-0.14, 0.14):
        b.cyl(0.09, 0.35, (x + dx, y, z1 + 0.27), 'MI_Concrete_Raw', segs=10)


# =======================================================================================
# Stairs, railings, porches

def stairs(b, axis, a0, a1, c0, c1, z0, z1, tread='MI_Wood_Floor', riser=TRIM, steps=None, nosing=0.025,
           solid=True, stringer=None):
    """A straight flight rising from a0 (at z0) to a1 (at z1) along 'axis', width c0..c1."""
    rise_total = z1 - z0
    n = steps or max(2, int(round(abs(rise_total) / 0.18)))
    run = (a1 - a0) / n
    rise = rise_total / n
    for i in range(n):
        za = z0 + rise * (i + 1)
        sa0 = a0 + run * i
        sa1 = a0 + run * (i + 1)
        lo, hi = (min(sa0, sa1), max(sa0, sa1))
        if solid:
            _box(b, axis, lo, hi, c0, c1, z0 if i == 0 or solid else za - 0.2, za - 0.03, riser)
        else:
            _box(b, axis, lo, hi, c0, c1, za - 0.06, za - 0.03, riser)
        n0 = lo - (nosing if run > 0 else 0)
        n1 = hi + (nosing if run < 0 else 0)
        _box(b, axis, n0, n1, c0 - 0.01, c1 + 0.01, za - 0.03, za, tread)
    if stringer:
        for c in (c0, c1):
            lo_c, hi_c = (c - 0.04, c) if c == min(c0, c1) else (c, c + 0.04)
            pts = []
            for i in range(n + 1):
                a = a0 + run * i
                pts.append((a, z0 + rise * i))
            # Sloped board approximated by one rotated box.
            length = math.hypot(a1 - a0, rise_total)
            ang = math.degrees(math.atan2(rise_total, a1 - a0))
            mid = ((a0 + a1) / 2, (z0 + z1) / 2 + 0.05)
            if axis == 'X':
                b.box((length, 0.04, 0.28), (mid[0], (lo_c + hi_c) / 2, mid[1]), stringer, rot=(0, -ang, 0))
            else:
                b.box((0.04, length, 0.28), ((lo_c + hi_c) / 2, mid[0], mid[1]), stringer, rot=(ang, 0, 0))
    return n


def railing(b, pts, height=0.9, post_every=1.2, m_post=TRIM, m_rail='MI_Wood_Dark', balusters=0.12,
            baluster_mat=TRIM):
    """A railing along a polyline of (x, y, z) floor points: posts, handrail and balusters."""
    for p, q in zip(pts, pts[1:]):
        p, q = Vector(p), Vector(q)
        d = q - p
        length = d.length
        if length < 1e-3:
            continue
        n_bal = max(1, int(length / balusters))
        for i in range(n_bal + 1):
            t = i / n_bal
            c = p + d * t
            b.box((0.035, 0.035, height - 0.08), (c.x, c.y, c.z + (height - 0.08) / 2), baluster_mat)
        n_post = max(1, int(round(length / post_every)))
        for i in range(n_post + 1):
            c = p + d * (i / n_post)
            b.box((0.09, 0.09, height + 0.08), (c.x, c.y, c.z + (height + 0.08) / 2), m_post, bevel=0.008)
        top_p = p + Vector((0, 0, height))
        top_q = q + Vector((0, 0, height))
        b.tube([tuple(top_p), tuple(top_q)], 0.03, m_rail, segs=8)
        b.tube([tuple(p + Vector((0, 0, 0.08))), tuple(q + Vector((0, 0, 0.08)))], 0.022, baluster_mat, segs=6)


def column(b, x, y, z0, z1, w=0.18, m=TRIM):
    b.box_minmax((x - w / 2 - 0.03, y - w / 2 - 0.03, z0), (x + w / 2 + 0.03, y + w / 2 + 0.03, z0 + 0.12), m,
                 bevel=0.01)
    b.box_minmax((x - w / 2, y - w / 2, z0 + 0.12), (x + w / 2, y + w / 2, z1 - 0.12), m, bevel=0.01)
    b.box_minmax((x - w / 2 - 0.03, y - w / 2 - 0.03, z1 - 0.12), (x + w / 2 + 0.03, y + w / 2 + 0.03, z1), m,
                 bevel=0.01)


# =======================================================================================
# Doors (leaf meshes for HHDoorActor / static doors). Hinge at the origin, leaf along +X,
# centred on y = 0, bottom at z = 0.

def _panel(b, x0, x1, z0, z1, t, m, y=0.0):
    for s in (-1, 1):
        b.box_minmax((x0, y + s * t / 2 - 0.004, z0), (x1, y + s * t / 2 + 0.004, z1), m, bevel=0.006)
        b.box_minmax((x0 + 0.03, y + s * (t / 2 + 0.004) - 0.003, z0 + 0.03),
                     (x1 - 0.03, y + s * (t / 2 + 0.004) + 0.003, z1 - 0.03), m, bevel=0.004)


def _leaf(b, w, h, t, m, rails, stiles=0.11, glass_rows=(), lites=(1, 1), glass='MI_Glass_Window'):
    """Rail-and-stile leaf. rails: z heights of rail centres (bottom/top included); panels between
    them are raised unless their index is in glass_rows (then glazed with muntins)."""
    b.box_minmax((0, -t / 2, 0), (stiles, t / 2, h), m, bevel=0.004)
    b.box_minmax((w - stiles, -t / 2, 0), (w, t / 2, h), m, bevel=0.004)
    rail_w = 0.1
    for zc in rails:
        b.box_minmax((stiles, -t / 2, max(0, zc - rail_w)), (w - stiles, t / 2, min(h, zc + rail_w)), m,
                     bevel=0.004)
    for i, (za, zb) in enumerate(zip(rails, rails[1:])):
        z0, z1 = za + rail_w, zb - rail_w
        if z1 - z0 < 0.05:
            continue
        if i in glass_rows:
            b.box_minmax((stiles, -0.004, z0), (w - stiles, 0.004, z1), glass)
            nx, nz = lites
            for k in range(1, nx):
                x = stiles + (w - 2 * stiles) * k / nx
                b.box_minmax((x - 0.012, -t / 2 + 0.008, z0), (x + 0.012, t / 2 - 0.008, z1), m)
            for k in range(1, nz):
                z = z0 + (z1 - z0) * k / nz
                b.box_minmax((stiles, -t / 2 + 0.008, z - 0.012), (w - stiles, t / 2 - 0.008, z + 0.012), m)
        else:
            mid = (stiles + w - stiles) / 2
            if w - 2 * stiles > 0.5 and (z1 - z0) < 0.9:
                _panel(b, stiles, mid - 0.04, z0, z1, t * 0.6, m)
                _panel(b, mid + 0.04, w - stiles, z0, z1, t * 0.6, m)
                b.box_minmax((mid - 0.04, -t / 2, z0), (mid + 0.04, t / 2, z1), m)
            else:
                _panel(b, stiles, w - stiles, z0, z1, t * 0.6, m)


def _hardware(b, w, t, knob_z=0.95, m='MI_Brass', deadbolt=False, plate=False):
    for s in (-1, 1):
        y = s * (t / 2 + 0.03)
        b.sphere(0.028, (w - 0.07, y, knob_z), m, scale=(1, 0.8, 1), segs=12, rings=8)
        b.cyl(0.01, 0.03, (w - 0.07, s * (t / 2 + 0.012), knob_z), m, rot=(90, 0, 0), segs=8)
        b.cyl(0.03, 0.006, (w - 0.07, s * (t / 2 + 0.003), knob_z), m, rot=(90, 0, 0), segs=12)
        if deadbolt:
            b.cyl(0.024, 0.012, (w - 0.07, s * (t / 2 + 0.006), knob_z + 0.16), m, rot=(90, 0, 0), segs=12)
        if plate:
            b.box_minmax((0.08, s * t / 2 - 0.002, 0.03), (w - 0.08, s * t / 2 + 0.002, 0.22), m)
    for z in (0.22, 1.0, 1.8):
        b.cyl(0.011, 0.11, (0.0, 0, z), m, segs=10)


@prop('SM_Door_House_Front', 'Architecture/Doors', 'box')
def door_house_front():
    b = Builder('SM_Door_House_Front')
    w, h, t = 0.92, 2.06, 0.045
    _leaf(b, w, h, t, 'MI_Paint_Wood_Red', rails=(0.1, 0.78, 1.36, 1.96), glass_rows=(2,), lites=(3, 1))
    _hardware(b, w, t, deadbolt=True, plate=True)
    b.box_minmax((0.33, -t / 2 - 0.006, 1.1), (0.59, -t / 2, 1.16), 'MI_Brass', bevel=0.003)
    return b


@prop('SM_Door_House_Back', 'Architecture/Doors', 'box')
def door_house_back():
    b = Builder('SM_Door_House_Back')
    w, h, t = 0.86, 2.04, 0.042
    _leaf(b, w, h, t, TRIM, rails=(0.1, 0.95, 1.94), glass_rows=(1,), lites=(3, 3))
    _hardware(b, w, t, knob_z=0.92, deadbolt=True)
    return b


@prop('SM_Door_Interior', 'Architecture/Doors', 'box')
def door_interior():
    b = Builder('SM_Door_Interior')
    w, h, t = 0.82, 2.02, 0.035
    _leaf(b, w, h, t, TRIM, rails=(0.1, 0.62, 1.12, 1.92))
    _hardware(b, w, t, knob_z=0.92)
    return b


@prop('SM_Door_Interior_Dark', 'Architecture/Doors', 'box')
def door_interior_dark():
    b = Builder('SM_Door_Interior_Dark')
    w, h, t = 0.82, 2.02, 0.035
    _leaf(b, w, h, t, 'MI_Wood_Dark', rails=(0.1, 0.62, 1.12, 1.92))
    _hardware(b, w, t, knob_z=0.92, m='MI_Brass')
    return b


@prop('SM_Door_Cellar', 'Architecture/Doors', 'box')
def door_cellar():
    b = Builder('SM_Door_Cellar')
    w, h, t = 0.82, 2.0, 0.035
    for i in range(5):
        b.box_minmax((i * w / 5 + 0.002, -t / 2, 0.01), ((i + 1) * w / 5 - 0.002, t / 2, h), 'MI_Wood_Planks',
                     bevel=0.003)
    for z in (0.25, 1.0, 1.75):
        b.box_minmax((0.03, t / 2, z - 0.06), (w - 0.03, t / 2 + 0.02, z + 0.06), 'MI_Wood_Raw', bevel=0.004)
    _hardware(b, w, t, knob_z=0.95, m='MI_Metal_Black')
    return b


@prop('SM_Door_Victorian', 'Architecture/Doors', 'box')
def door_victorian():
    b = Builder('SM_Door_Victorian')
    w, h, t = 0.96, 2.3, 0.055
    _leaf(b, w, h, t, 'MI_Wood_Dark', rails=(0.1, 0.9, 1.5, 2.2), glass_rows=(2,), lites=(1, 1), glass='MI_Glass_Dirty')
    _hardware(b, w, t, knob_z=1.0, m='MI_Brass', deadbolt=True)
    return b


# =======================================================================================
# Window glow card (scaled per window by the level: X = width, Z = height, 1 x 1 m)

@prop('SM_Window_Glow', 'Architecture/Windows', 'none')
def window_glow():
    b = Builder('SM_Window_Glow')
    b.plane(1.0, 1.0, (0, 0, 0), 'MI_Emit_Window_Warm')
    return b


@prop('SM_Screen_Quad', 'Architecture/Windows', 'none')
def screen_quad():
    """1 x 1 m screen surface facing -Y (haunted TVs and monitors scale it to their screen)."""
    b = Builder('SM_Screen_Quad')
    b.plane(1.0, 1.0, (0, 0, 0), 'MI_Screen_TV')
    return b


@prop('SM_Key_Spare', 'Props/Loot', 'none')
def key_spare():
    """A worn brass house key on a split ring with a paper tag, lying flat."""
    b = Builder('SM_Key_Spare')
    b.torus(0.012, 0.0015, (0.0, 0, 0.0016), 'MI_Steel', segs=16, rsegs=5)
    b.cyl(0.011, 0.003, (0.022, 0, 0.0016), 'MI_Brass', rot=(0, 0, 0), segs=14)
    b.box((0.004, 0.006, 0.0025), (0.033, 0, 0.0016), 'MI_Brass')
    b.box((0.032, 0.007, 0.0022), (0.052, 0, 0.0016), 'MI_Brass')
    for i, dz in enumerate((0.003, 0.0045, 0.002, 0.004)):
        b.box((0.004, 0.004 + dz, 0.0022), (0.044 + i * 0.007, -0.004 - dz / 2, 0.0016), 'MI_Brass')
    b.box((0.03, 0.018, 0.0008), (-0.03, 0.004, 0.0006), 'MI_Paper', rot=(0, 0, 20))
    return b


# =======================================================================================
# Building container

class Building:
    """One building: shell + trim meshes in a local frame placed at 'matrix' in the world."""

    def __init__(self, name, matrix):
        self.name = name
        self.M = matrix
        self.shell = Builder(f'SM_{name}_Shell')
        self.trim = Builder(f'SM_{name}_Trim')
        self.glows = []         # (local matrix, w, h) for SM_Window_Glow actors
        self.doors = []         # dicts for door actors / static doors (local matrix)

    def world(self, loc=(0, 0, 0), rot=(0, 0, 0)):
        return self.M @ mat(loc, rot)

    def add_opening_trim(self, axis, o, c_out, c_in, inside_finish=True):
        """Casings, sash/glass and backdrops for one opening in a wall whose exterior face is c_out
        and interior face c_in (both on the axis perpendicular to the wall)."""
        out = 1 if c_out > c_in else -1          # direction pointing out of the building
        kind = o['kind']
        tm = o.get('trim', TRIM)
        if kind == 'window':
            casing(self.trim, axis, o, c_out, out, m=tm, sill=True)
            reveal(self.trim, axis, o, c_in, c_out, m=tm, bottom=True)
            c_mid = (c_out + c_in) / 2 + out * 0.03
            if o.get('style', 'sash') == 'sash':
                sash_window(self.trim, axis, o, c_mid, out, m=tm, lites=o.get('lites', (2, 2)),
                            glass=o.get('glass', 'MI_Glass_Window'))
            else:
                picture_window(self.trim, axis, o, c_mid, m=tm, mullions=o.get('mullions', 0),
                               glass=o.get('glass', 'MI_Glass_Window'))
            if inside_finish and o.get('backdrop') is None:
                casing(self.trim, axis, o, c_in, -out, width=0.07, proud=0.018, m=tm, head=0.08, cap=False)
                _frame_box(self.trim, axis, o['a0'] - 0.1, o['a1'] + 0.1, c_in, c_in - out * 0.05, o['z0'] - 0.035,
                           o['z0'], tm, bevel=0.004)
            if o.get('boarded'):
                for k in range(3):
                    z = o['z0'] + (o['z1'] - o['z0']) * (0.2 + 0.3 * k)
                    tilt = (-6, 4, -2)[k]
                    rot = (0, tilt, 0) if axis == 'X' else (tilt, 0, 0)
                    w = o['a1'] - o['a0'] + 0.3
                    am = (o['a0'] + o['a1']) / 2
                    size = (w, 0.025, 0.16) if axis == 'X' else (0.025, w, 0.16)
                    loc = (am, c_out + out * 0.04, z) if axis == 'X' else (c_out + out * 0.04, am, z)
                    self.trim.box(size, loc, 'MI_Wood_Raw', rot=rot, bevel=0.004)
            back = o.get('backdrop')
            if back == 'curtain':
                curtain_backdrop(self.trim, axis, o, c_in, -out,
                                 m=o.get('curtain', 'MI_Fabric_Curtain_Light'))
            elif back == 'shop':
                image_backdrop(self.trim, axis, o, c_in, -out, m=o.get('image', 'MI_Img_Shop_Interior'))
            elif back == 'dark':
                _frame_box(self.trim, axis, o['a0'], o['a1'], c_in - out * 0.3, c_in - out * 0.32, o['z0'], o['z1'],
                           'MI_Fabric_Curtain')
            if o.get('glow'):
                cg = c_mid - out * 0.06
                a = (o['a0'] + o['a1']) / 2
                z = (o['z0'] + o['z1']) / 2
                if axis == 'X':
                    m = mat((a, cg, z), (0, 0, 0 if out < 0 else 180))
                else:
                    m = mat((cg, a, z), (0, 0, 90 if out > 0 else -90))
                self.glows.append((m, o['a1'] - o['a0'] - 0.06, o['z1'] - o['z0'] - 0.06, o.get('glow_tags', [])))
        elif kind == 'door':
            casing(self.trim, axis, o, c_out, out, m=tm, head=0.14)
            reveal(self.trim, axis, o, c_in, c_out, m=tm, bottom=False)
            if inside_finish:
                casing(self.trim, axis, o, c_in, -out, width=0.07, proud=0.018, m=tm, head=0.08, cap=False)
            _frame_box(self.trim, axis, o['a0'], o['a1'], c_in, c_out, o['z0'], o['z0'] + 0.02, 'MI_Wood_Dark')
        elif kind == 'idoor':           # interior door between two rooms
            for c, s in ((c_out, out), (c_in, -out)):
                casing(self.trim, axis, o, c, s, width=0.07, proud=0.018, m=tm, head=0.08, cap=False)
            reveal(self.trim, axis, o, c_in, c_out, m=tm, bottom=False)
        elif kind == 'arch':
            for c, s in ((c_out, out), (c_in, -out)):
                casing(self.trim, axis, o, c, s, width=0.08, proud=0.02, m=tm, head=0.1, cap=False)
            reveal(self.trim, axis, o, c_in, c_out, m=tm, bottom=False)
        elif kind == 'shop':
            shop_front(self.trim, axis, o, c_out, out, m=o.get('trim', 'MI_Trim_Dark'), door=o.get('door'),
                       mullions=o.get('mullions', 1))
            if o.get('backdrop', 'shop') == 'shop':
                image_backdrop(self.trim, axis, dict(o, z0=o['z0'] + 0.55, z1=o['z1'] - 0.55), c_in, -out,
                               m=o.get('image', 'MI_Img_Shop_Interior'), depth=1.2)
        elif kind == 'garage':
            casing(self.trim, axis, o, c_out, out, width=0.12, m=tm, head=0.16)
