"""
Prop library for the town of Hollowmere and the houses the crew breaks into: loot, interior
furniture and fixtures, street furniture, fences and hedges, trees, parked cars and yard props.

Registers into the shared prop registry exactly like items.py does:

    import props_world            # adds the SM_* below to props.PROPS
    props.build('SM_Sofa_3Seat').object()

Pivot conventions (Blender, metres, Z up) - same as props.py:
  floor props    bottom centre (z = 0 is the floor), front faces -Y
  wall props     back face on y = 0, the prop extends towards -Y, bottom at z = 0 unless noted
  ceiling props  mount point at the origin, hanging down -Z
  fence/hedge    segments span x = 0 .. length (start post at x = 0), centred on y = 0, bottom at
                 z = 0, so they tile along +X; gates have their hinge at the origin, leaf along +X
  cars           bottom centre, the FRONT of the car faces -Y

QA: preview_world_props.py (contact sheets + bounding box / pivot / material checks).
Inventory icons for the loot: render_loot_icons.py.
"""
import math
import os
import random
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402,F401  (registers bmesh/mathutils)
import bmesh  # noqa: E402
from mathutils import Matrix, Vector, noise  # noqa: E402

from hhblend import Builder, _box_uv, _mat4  # noqa: E402
from props import prop  # noqa: E402

# name -> dict(group, kind, size) - used by the QA script. kind: floor | wall | ceiling | fence | gate
WORLD = {}
GROUPS = ('loot', 'house', 'town', 'nature', 'yard')


def wprop(name, folder, collision='box', group='house', kind='floor', size=None):
    """props.prop() plus the QA metadata (target W x D x H in metres)."""
    def deco(fn):
        prop(name, folder, collision)(fn)
        WORLD[name] = dict(group=group, kind=kind, size=size)
        return fn
    return deco


def names(group=None):
    return [n for n, w in WORLD.items() if group in (None, 'all', w['group'])]


def _rng(name):
    """Deterministic RNG (props.build seeds 'random' with hash(), which changes per process)."""
    return random.Random(zlib.crc32(name.encode()))


# =======================================================================================
# Geometry helpers (all go through Builder internals so materials / UVs stay consistent)

def _rrect(w, h, r, n=4, cx=0.0, cy=0.0):
    """Rounded rectangle outline (CCW) for prism()."""
    r = max(1e-4, min(r, w / 2 - 1e-4, h / 2 - 1e-4))
    pts = []
    for sx, sy, a0 in ((1, -1, -90), (1, 1, 0), (-1, 1, 90), (-1, -1, 180)):
        ox, oy = cx + sx * (w / 2 - r), cy + sy * (h / 2 - r)
        for k in range(n + 1):
            a = math.radians(a0 + 90 * k / n)
            pts.append((ox + r * math.cos(a), oy + r * math.sin(a)))
    return pts


def _ellipse(a, b, n=20, cx=0.0, cy=0.0):
    return [(cx + a * math.cos(2 * math.pi * i / n), cy + b * math.sin(2 * math.pi * i / n)) for i in range(n)]


def _geo(b, verts, faces, mat, smooth=True):
    """Arbitrary polygons (outward winding), metre box-UVs."""
    pb, uv = b._begin()
    vs = [pb.verts.new(v) for v in verts]
    for f in faces:
        pb.faces.new([vs[i] for i in f])
    pb.normal_update()
    _box_uv(pb.faces, uv)
    b._commit(pb, mat, smooth)
    return b


def _disc(b, rx, rz, loc, mat, rot=(0, 0, 0), segs=32, uv_rect=(0, 0, 1, 1)):
    """Flat (elliptical) disc in the local XZ plane facing -Y; its bounding box maps onto uv_rect
    (image top = +Z), like hhblend's plane()."""
    m = _mat4(loc, rot)
    pb, uv = b._begin()
    u0, v0, u1, v1 = uv_rect
    c = pb.verts.new(m @ Vector((0, 0, 0)))
    pts = [(math.cos(2 * math.pi * i / segs), math.sin(2 * math.pi * i / segs)) for i in range(segs)]
    ring = [pb.verts.new(m @ Vector((rx * x, 0, rz * z))) for x, z in pts]
    for i in range(segs):
        j = (i + 1) % segs
        f = pb.faces.new((c, ring[i], ring[j]))
        for loop, (x, z) in zip(f.loops, ((0, 0), pts[i], pts[j])):
            loop[uv].uv = (u0 + (x * 0.5 + 0.5) * (u1 - u0), v0 + (z * 0.5 + 0.5) * (v1 - v0))
    b._commit(pb, mat, smooth=False)
    return b


def _blob(b, radius, loc, mat, scale=(1, 1, 1), subdiv=2, amp=0.2, freq=1.6, seed=0, zmin=None, rot=(0, 0, 0)):
    """Noise-displaced icosphere: foliage clumps, soil, lumpy cushions."""
    pb, uv = b._begin()
    bmesh.ops.create_icosphere(pb, subdivisions=subdiv, radius=1.0)
    off = Vector((seed * 3.17 + 0.5, seed * 1.71 + 0.2, seed * 2.39 + 0.9))
    m = _mat4(loc, rot)
    for v in pb.verts:
        n = v.co.normalized()
        d = noise.noise(n * freq + off) + 0.5 * noise.noise(n * freq * 2.3 + off * 1.7)
        r = radius * (1 + amp * d)
        p = m @ Vector((n.x * r * scale[0], n.y * r * scale[1], n.z * r * scale[2]))
        if zmin is not None and p.z < zmin:
            p.z = zmin
        v.co = p
    pb.normal_update()
    _box_uv(pb.faces, uv)
    b._commit(pb, mat)
    return b


def _sheet(b, fn, nu, nv, mat, thickness=0.0, size=(1.0, 1.0)):
    """Parametric cloth / panel: fn(u, v) -> (x, y, z) for u, v in 0..1. With 'thickness' it is
    solidified (two-sided), otherwise the winding follows (u, v) (front = du x dv)."""
    pb, uv = b._begin()
    rows = [[pb.verts.new(Vector(fn(i / nu, j / nv))) for i in range(nu + 1)] for j in range(nv + 1)]
    for j in range(nv):
        for i in range(nu):
            f = pb.faces.new((rows[j][i], rows[j][i + 1], rows[j + 1][i + 1], rows[j + 1][i]))
            for loop, (a, c) in zip(f.loops, ((i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1))):
                loop[uv].uv = (a / nu * size[0], c / nv * size[1])
    if thickness:
        bmesh.ops.solidify(pb, geom=list(pb.faces), thickness=thickness)
        bmesh.ops.recalc_face_normals(pb, faces=pb.faces)
    pb.normal_update()
    b._commit(pb, mat)
    return b


def _frame_ring(b, ow, oh, iw, ih, y0, y1, mat, cx=0.0, cz=0.0, bevel=0.0):
    """Picture-frame moulding tier: four mitred bars between an outer (ow x oh) and an inner
    (iw x ih) rectangle in the XZ plane, from y0 (front) to y1 (back)."""
    hw, hh, iw2, ih2 = ow / 2, oh / 2, iw / 2, ih / 2
    for q in ([(-hw, -hh), (hw, -hh), (iw2, -ih2), (-iw2, -ih2)],
              [(hw, -hh), (hw, hh), (iw2, ih2), (iw2, -ih2)],
              [(hw, hh), (-hw, hh), (-iw2, ih2), (iw2, ih2)],
              [(-hw, hh), (-hw, -hh), (-iw2, -ih2), (-iw2, ih2)]):
        b.prism([(cx + x, cz + z) for x, z in q], y1 - y0, (0, (y0 + y1) / 2, 0), mat, axis='Y', bevel=bevel)
    return b


def _attach(b, sub, loc=(0, 0, 0), rot=(0, 0, 0)):
    """Merge a sub-builder authored around its own pivot (e.g. a hinge) at loc / rot (degrees)."""
    b.merge(sub, _mat4(loc, rot))
    sub.bm.free()
    return b


def _bounds(builder):
    vs = [v.co for v in builder.bm.verts]
    lo = Vector((min(v.x for v in vs), min(v.y for v in vs), min(v.z for v in vs)))
    hi = Vector((max(v.x for v in vs), max(v.y for v in vs), max(v.z for v in vs)))
    return lo, hi


def _ground(b, sub, rot=(0, 0, 0), xy=(0.0, 0.0)):
    """Rotate a sub-builder, then drop it onto z = 0 with its footprint centred on xy."""
    bmesh.ops.transform(sub.bm, matrix=_mat4((0, 0, 0), rot), verts=sub.bm.verts)
    lo, hi = _bounds(sub)
    _attach(b, sub, (xy[0] - (lo.x + hi.x) / 2, xy[1] - (lo.y + hi.y) / 2, -lo.z))
    return b


def _recentre(b, z0=0.0, x=True, y=True):
    """Shift the whole builder so its footprint is centred on the origin and its bottom is z0."""
    lo, hi = _bounds(b)
    d = Vector(((-(lo.x + hi.x) / 2) if x else 0, (-(lo.y + hi.y) / 2) if y else 0, z0 - lo.z))
    bmesh.ops.translate(b.bm, vec=d, verts=b.bm.verts)
    return b


def _torus_m(b, major, minor, matrix, mat, segs=8, rsegs=4, scale=(1, 1, 1)):
    """Torus with an arbitrary orientation matrix (chain links)."""
    loc, rot, _ = matrix.decompose()
    eul = rot.to_euler('XYZ')
    b.torus(major, minor, tuple(loc), mat, rot=tuple(math.degrees(a) for a in eul), segs=segs, rsegs=rsegs,
            scale=scale)


def _resample(pts, step):
    pts = [Vector(p) for p in pts]
    out = [pts[0]]
    carry = 0.0
    for a, c in zip(pts, pts[1:]):
        seg = (c - a).length
        t = step - carry
        while t <= seg:
            out.append(a + (c - a) * (t / seg))
            t += step
        carry = seg - (t - step)
    return out


def _chain(b, pts, link=0.008, wire=0.0012, mat='MI_Gold', segs=6, rsegs=4):
    """Interlocking oval links along a polyline (alternately rotated 90 degrees)."""
    ps = _resample(pts, link * 0.78)
    for i, p in enumerate(ps):
        q = ps[min(i + 1, len(ps) - 1)] - ps[max(i - 1, 0)]
        t = q.normalized() if q.length > 1e-9 else Vector((1, 0, 0))
        up = Vector((0, 0, 1)) if abs(t.z) < 0.9 else Vector((1, 0, 0))
        n = (up - t * up.dot(t)).normalized()
        if i % 2:
            n = t.cross(n)
        bn = t.cross(n)
        m = Matrix((t, n, bn)).transposed().to_4x4()
        m.translation = p
        _torus_m(b, link * 0.5, wire, m, mat, segs, rsegs, scale=(1, 0.62, 1))


def _knob(b, loc, mat='MI_Brass', r=0.012, axis='-Y', segs=12):
    """Small mushroom knob sticking out of a front (-Y) or top (+Z) face."""
    rot = (90, 0, 0) if axis == '-Y' else (0, 0, 0)
    sgn = -1 if axis == '-Y' else 1
    prof = [(0, 0), (r * 0.45, 0), (r * 0.35, sgn * 0 + r * 0.8), (r, r * 1.3), (r * 0.9, r * 1.9), (0, r * 2.0)]
    if axis == '-Y':
        b.lathe(prof, loc, mat, rot=rot, segs=segs)
    else:
        b.lathe(prof, loc, mat, segs=segs)


def _bar_handle(b, x, z, length, y_face, mat='MI_Chrome', vertical=False, r=0.006, stand=0.03):
    """D-shaped bar pull on a front face at y_face (sticks out towards -Y)."""
    if vertical:
        pts = [(x, y_face, z - length / 2), (x, y_face - stand, z - length / 2), (x, y_face - stand, z + length / 2),
               (x, y_face, z + length / 2)]
    else:
        pts = [(x - length / 2, y_face, z), (x - length / 2, y_face - stand, z), (x + length / 2, y_face - stand, z),
               (x + length / 2, y_face, z)]
    b.tube(pts, r, mat, segs=8, bend=stand * 0.45)


# =======================================================================================
# 1. LOOT (Props/Loot) - stolen during heists; pivot bottom centre, front -Y

@wprop('SM_Loot_TV', 'Props/Loot', 'box', 'loot', size=(1.0, 0.22, 0.64))
def loot_tv():
    b = Builder('SM_Loot_TV')
    w, h, z0 = 1.0, 0.575, 0.065
    zc = z0 + h / 2
    b.prism(_rrect(0.44, 0.22, 0.04), 0.014, (0, 0, 0.007), 'MI_Plastic_Black', axis='Z', bevel=0.003)
    b.prism(_rrect(0.16, 0.06, 0.02), 0.01, (0, 0.03, 0.018), 'MI_Plastic_Grey', axis='Z', bevel=0.002)
    b.box((0.07, 0.03, 0.1), (0, 0.032, 0.07), 'MI_Plastic_Black', rot=(-8, 0, 0), bevel=0.006)
    b.box((w, 0.026, h), (0, 0, zc), 'MI_Plastic_Black', bevel=0.004)
    b.box((0.64, 0.045, 0.36), (0, 0.03, z0 + 0.21), 'MI_Plastic_Black', bevel=0.014)
    b.plane(w - 0.026, h - 0.042, (0, -0.0131, zc + 0.005), 'MI_Screen_Black')
    b.box((w - 0.004, 0.004, 0.016), (0, -0.0125, z0 + 0.009), 'MI_Plastic_Grey', bevel=0.0015)
    b.box((0.06, 0.003, 0.006), (0, -0.0145, z0 + 0.009), 'MI_Chrome')
    b.box((0.005, 0.003, 0.003), (0.42, -0.0145, z0 + 0.009), 'MI_Emit_Red')
    for k in range(9):
        b.box((0.3, 0.004, 0.006), (0, 0.0535, z0 + 0.27 + k * 0.012), 'MI_Plastic_Grey')
    for i, x in enumerate((-0.22, -0.17, -0.12)):
        b.box((0.03, 0.006, 0.012), (x, 0.0535, z0 + 0.1), 'MI_Plastic_Grey')
    return b


@wprop('SM_Loot_Laptop', 'Props/Loot', 'box', 'loot', size=(0.34, 0.24, 0.025))
def loot_laptop():
    b = Builder('SM_Loot_Laptop')
    w, d = 0.34, 0.24
    b.prism(_rrect(w, d, 0.012), 0.012, (0, 0, 0.008), 'MI_Plastic_Laptop', axis='Z', bevel=0.0025)
    b.prism(_rrect(w - 0.004, d - 0.004, 0.011), 0.002, (0, 0, 0.015), 'MI_Plastic_Black', axis='Z')
    b.prism(_rrect(w, d, 0.012), 0.008, (0, 0, 0.0195), 'MI_Aluminium', axis='Z', bevel=0.0025)
    b.torus(0.009, 0.0012, (0, 0.02, 0.0235), 'MI_Plastic_Laptop', segs=20, rsegs=4, scale=(1, 1, 0.4))
    b.cyl(0.006, w - 0.08, (0, d / 2 - 0.004, 0.0145), 'MI_Plastic_Black', rot=(0, 90, 0), segs=10)
    b.box((0.06, 0.008, 0.003), (0, -d / 2 + 0.002, 0.0148), 'MI_Plastic_Black', bevel=0.001)
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.cyl(0.007, 0.002, (sx * (w / 2 - 0.03), sy * (d / 2 - 0.03), 0.001), 'MI_Rubber_Black', segs=10)
    for k, y in enumerate((-0.05, -0.025, 0.0)):
        b.box((0.004, 0.012 if k < 2 else 0.007, 0.0045), (-w / 2 - 0.0005, y, 0.0085), 'MI_Plastic_Black')
    b.cyl(0.0025, 0.004, (w / 2, -0.04, 0.0085), 'MI_Plastic_Black', rot=(0, 90, 0), segs=8)
    b.box((0.004, 0.02, 0.0045), (w / 2, 0.0, 0.0085), 'MI_Plastic_Black')
    return b


def _box_shell(b, w, d, h, t, z0, mat, floor=True, bevel=0.002):
    """Open-topped box from walls (so the inside can be lined)."""
    for s in (-1, 1):
        b.box((w, t, h), (0, s * (d / 2 - t / 2), z0 + h / 2), mat, bevel=bevel, segs=1)
        b.box((t, d - 2 * t + 0.001, h), (s * (w / 2 - t / 2), 0, z0 + h / 2), mat, bevel=bevel, segs=1)
    if floor:
        b.box((w - 0.002, d - 0.002, t), (0, 0, z0 + t / 2), mat, bevel=bevel, segs=1)


@wprop('SM_Loot_JewelryBox', 'Props/Loot', 'convex', 'loot', size=(0.22, 0.15, 0.11))
def loot_jewelry_box():
    b = Builder('SM_Loot_JewelryBox')
    w, d, h, z0, t = 0.22, 0.15, 0.068, 0.008, 0.012
    _box_shell(b, w, d, h, t, z0, 'MI_Wood_Dark')
    b.box((w + 0.006, d + 0.006, 0.01), (0, 0, z0 + 0.005), 'MI_Wood_Dark', bevel=0.003)
    top = z0 + h
    b.box((w - 2 * t, d - 2 * t, 0.03), (0, 0, z0 + 0.025), 'MI_Velvet_Blue', bevel=0.003)
    b.box((0.008, d - 2 * t, 0.045), (0.02, 0, z0 + 0.03), 'MI_Wood_Dark', bevel=0.001)
    # Left compartment: ring rolls with a ruby ring standing between them.
    for y in (-0.03, 0.0, 0.03):
        b.cyl(0.011, 0.074, (-0.052, y, z0 + 0.044), 'MI_Velvet_Blue', rot=(0, 90, 0), segs=10)
    b.torus(0.0085, 0.0015, (-0.07, -0.015, z0 + 0.06), 'MI_Gold', rot=(90, 0, 0), segs=16, rsegs=5)
    b.sphere(0.0035, (-0.07, -0.015, z0 + 0.071), 'MI_Plastic_Red', segs=8, rings=6)
    b.torus(0.008, 0.0013, (-0.035, 0.015, z0 + 0.059), 'MI_Gold', rot=(90, 0, 0), segs=16, rsegs=5)
    b.sphere(0.0028, (-0.035, 0.015, z0 + 0.069), 'MI_Porcelain', segs=8, rings=6)
    # Right compartment: a gold chain spilling over the velvet.
    zc = z0 + 0.041
    _chain(b, [(0.035, 0.05, zc), (0.06, 0.035, zc), (0.085, 0.02, zc), (0.07, -0.01, zc), (0.045, -0.015, zc),
               (0.04, -0.04, zc), (0.065, -0.05, zc), (0.09, -0.045, zc)], link=0.009, wire=0.0012, segs=4, rsegs=3)
    b.sphere(0.006, (0.09, -0.045, zc + 0.002), 'MI_Gold', scale=(1, 0.7, 0.45), segs=10, rings=6)
    # Brass fittings: feet, corners, clasp, hinges.
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.sphere(0.007, (sx * (w / 2 - 0.012), sy * (d / 2 - 0.012), 0.005), 'MI_Brass', scale=(1, 1, 0.75),
                     segs=10, rings=6)
            b.box((0.016, 0.016, 0.016), (sx * (w / 2 - 0.006), sy * (d / 2 - 0.006), top - 0.008), 'MI_Brass',
                  bevel=0.002, segs=1)
    b.box((0.026, 0.004, 0.022), (0, -d / 2 - 0.002, top - 0.014), 'MI_Brass', bevel=0.001)
    b.cyl(0.0025, 0.004, (0, -d / 2 - 0.004, top - 0.016), 'MI_Plastic_Black', rot=(90, 0, 0), segs=8)
    for x in (-0.06, 0.06):
        b.cyl(0.0035, 0.03, (x, d / 2 + 0.002, top), 'MI_Brass', rot=(0, 90, 0), segs=8)
    # Lid (hinge at the back top edge), open 32 degrees.
    lid = Builder('lid')
    lid.box((w, d, 0.014), (0, -d / 2, 0.007), 'MI_Wood_Dark', bevel=0.003, segs=1)
    lid.prism(_rrect(w - 0.04, d - 0.04, 0.012), 0.012, (0, -d / 2, 0.018), 'MI_Wood_Dark', axis='Z', bevel=0.004)
    lid.prism(_ellipse(0.032, 0.02, 20), 0.002, (0, -d / 2, 0.0245), 'MI_Brass', axis='Z')
    lid.box((w - 2 * t, d - 2 * t, 0.003), (0, -d / 2, -0.0015), 'MI_Velvet_Blue')
    lid.box((0.02, 0.004, 0.016), (0, -d - 0.002, -0.002), 'MI_Brass', bevel=0.001)
    for sx in (-1, 1):
        for sy in (0, 1):
            lid.box((0.018, 0.018, 0.016), (sx * (w / 2 - 0.007), -0.007 - sy * (d - 0.014), 0.008), 'MI_Brass',
                    bevel=0.002, segs=1)
    _attach(b, lid, (0, d / 2, top), (-32, 0, 0))
    return b


@wprop('SM_Loot_Watch', 'Props/Loot', 'convex', 'loot', size=(0.1, 0.08, 0.05))
def loot_watch():
    b = Builder('SM_Loot_Watch')
    w, d, h = 0.1, 0.08, 0.034
    b.box((w, d, h), (0, 0, h / 2), 'MI_Leather_Black', bevel=0.004)
    b.box((w - 0.01, d - 0.01, 0.004), (0, 0, h - 0.001), 'MI_Velvet_Blue')
    b.box((w + 0.001, d + 0.001, 0.002), (0, 0, h - 0.006), 'MI_Gold')
    # Velvet pillow along X with the watch strapped around it.
    px, py, pz, pr = 0.0, -0.004, h + 0.007, 0.016
    b.lathe([(0, -0.032), (0.012, -0.031), (pr, -0.026), (pr, 0.026), (0.012, 0.031), (0, 0.032)], (px, py, pz),
            'MI_Velvet_Blue', rot=(0, 90, 0), segs=16)
    b.lathe([(pr + 0.0002, -0.009), (pr + 0.0028, -0.009), (pr + 0.0028, 0.009), (pr + 0.0002, 0.009)], (px, py, pz),
            'MI_Leather', rot=(0, 90, 0), segs=24, closed=True)
    phi = 48.0
    n = Vector((0, -math.sin(math.radians(phi)), math.cos(math.radians(phi))))
    c = Vector((px, py, pz)) + n * (pr + 0.0075)
    b.cyl(0.019, 0.009, tuple(c), 'MI_Gold', rot=(phi, 0, 0), segs=28)
    b.torus(0.0178, 0.0022, tuple(c + n * 0.0045), 'MI_Gold', rot=(phi, 0, 0), segs=28, rsegs=6)
    _disc(b, 0.0158, 0.0158, tuple(c + n * 0.0046), 'MI_Img_ClockFace', rot=(phi - 90, 0, 0), segs=24)
    b.cyl(0.016, 0.0012, tuple(c + n * 0.0056), 'MI_Glass', rot=(phi, 0, 0), segs=24)
    b.cyl(0.0022, 0.005, tuple(c + Vector((0.021, 0, 0))), 'MI_Gold', rot=(0, 90, 0), segs=8)
    for sy in (-1, 1):
        t = Vector((0, math.cos(math.radians(phi)), math.sin(math.radians(phi)))) * sy
        for sx in (-1, 1):
            b.box((0.004, 0.009, 0.004), tuple(c + t * 0.019 + Vector((sx * 0.0075, 0, 0)) - n * 0.001), 'MI_Gold',
                  rot=(phi, 0, 0), bevel=0.001)
    # Lid, hinged at the back top edge and standing open.
    lid = Builder('lid')
    lid.box((w, d, 0.016), (0, -d / 2, 0.008), 'MI_Leather_Black', bevel=0.004)
    lid.box((w - 0.01, d - 0.01, 0.002), (0, -d / 2, -0.0005), 'MI_Velvet_Blue')
    lid.box((0.05, 0.012, 0.0006), (0, -d / 2 - 0.01, -0.0018), 'MI_Gold')
    _attach(b, lid, (0, d / 2, h), (-100, 0, 0))
    b.cyl(0.002, w - 0.03, (0, d / 2, h), 'MI_Gold', rot=(0, 90, 0), segs=8)
    return b


@wprop('SM_Loot_Ring', 'Props/Loot', 'convex', 'loot', size=(0.06, 0.06, 0.06))
def loot_ring():
    b = Builder('SM_Loot_Ring')
    s, h = 0.06, 0.032
    b.box((s, s, h), (0, 0, h / 2), 'MI_Fabric_Curtain', bevel=0.007, segs=3)
    b.box((s - 0.008, s - 0.008, 0.003), (0, 0, h - 0.001), 'MI_Fabric_Bedding', bevel=0.001)
    for sy in (-1, 1):
        b.cyl(0.0075, s - 0.012, (0, sy * 0.0078, h + 0.0005), 'MI_Fabric_Bedding', rot=(0, 90, 0), segs=12)
    b.box((s + 0.0006, s + 0.0006, 0.0015), (0, 0, h - 0.004), 'MI_Gold')
    # Solitaire ring standing in the slit.
    rc = Vector((0, 0, h + 0.0085))
    b.torus(0.0088, 0.0013, tuple(rc), 'MI_Gold', rot=(90, 0, 0), segs=24, rsegs=6)
    top = rc + Vector((0, 0, 0.0095))
    for i in range(4):
        a = math.pi / 4 + i * math.pi / 2
        b.tube([tuple(top + Vector((math.cos(a) * 0.0012, math.sin(a) * 0.0012, -0.0005))),
                tuple(top + Vector((math.cos(a) * 0.0028, math.sin(a) * 0.0028, 0.0032)))], 0.00045, 'MI_Silver', segs=4)
    b.lathe([(0, -0.0012), (0.0032, 0.0018), (0.0034, 0.0022), (0.0019, 0.0038), (0, 0.0038)], tuple(top), 'MI_Mirror',
            segs=8)
    b.lathe([(0, -0.001), (0.0016, 0.0), (0.0018, 0.0012), (0, 0.0012)], tuple(top + Vector((0, 0, -0.0012))), 'MI_Silver',
            segs=8)
    lid = Builder('lid')
    lid.box((s, s, 0.024), (0, -s / 2, 0.012), 'MI_Fabric_Curtain', bevel=0.007, segs=3)
    lid.box((s - 0.008, s - 0.008, 0.003), (0, -s / 2, 0.0005), 'MI_Fabric_Bedding', bevel=0.001)
    lid.box((s + 0.0006, s + 0.0006, 0.0015), (0, -s / 2, 0.004), 'MI_Gold')
    _attach(b, lid, (0, s / 2, h), (-102, 0, 0))
    b.cyl(0.0022, s - 0.02, (0, s / 2 + 0.0005, h + 0.001), 'MI_Gold', rot=(0, 90, 0), segs=8)
    return b


@wprop('SM_Loot_Necklace', 'Props/Loot', 'convex', 'loot', size=(0.18, 0.14, 0.03))
def loot_necklace():
    b = Builder('SM_Loot_Necklace')
    b.prism(_rrect(0.18, 0.14, 0.012), 0.007, (0, 0, 0.0035), 'MI_Wood_Dark', axis='Z', bevel=0.002)
    b.box((0.168, 0.128, 0.016), (0, 0, 0.007 + 0.008), 'MI_Velvet_Blue', bevel=0.005, segs=3)
    top = 0.023
    pr = 0.0046
    pts = []
    for i in range(121):
        t = 2 * math.pi * i / 120
        y = 0.044 * math.cos(t) + 0.004 - 0.012 * math.exp(-((t - math.pi) / 0.45) ** 2)
        x = 0.062 * math.sin(t) * (1 - 0.12 * math.cos(t))
        pts.append((x, y, top + pr))
    beads = _resample(pts[3:-3], pr * 2.08)
    for i, p in enumerate(beads):
        b.sphere(pr, tuple(p), 'MI_Porcelain', segs=7, rings=4)
    # Clasp at the back, pendant pearl at the front.
    b.cyl(0.0035, 0.009, (0, 0.048, top + pr), 'MI_Gold', rot=(0, 90, 0), segs=10)
    b.torus(0.003, 0.0008, (0.006, 0.048, top + pr), 'MI_Gold', rot=(90, 0, 0), segs=10, rsegs=4)
    b.torus(0.0022, 0.0007, (0, -0.0565, top + 0.002), 'MI_Gold', segs=10, rsegs=4)
    b.sphere(0.0068, (0, -0.0625, top + 0.0068), 'MI_Porcelain', scale=(1, 1.15, 1), segs=12, rings=8)
    return b


@wprop('SM_Loot_Cash', 'Props/Loot', 'convex', 'loot', size=(0.16, 0.07, 0.04))
def loot_cash():
    b = Builder('SM_Loot_Cash')
    w, d, t = 0.156, 0.066, 0.0195

    def stack(band):
        s = Builder('stack')
        for k in range(3):
            s.box((w - (k % 2) * 0.0015, d - (k % 2) * 0.001, t / 3 - 0.0002), ((k - 1) * 0.0006, 0, t / 6 + k * t / 3),
                  'MI_Cash', bevel=0.0007, segs=1)
        z = t + 0.0002
        for sy in (-1, 1):
            s.box((w - 0.012, 0.0022, 0.0004), (0, sy * (d / 2 - 0.006), z), 'MI_Metal_Green')
        for sx in (-1, 1):
            s.box((0.0022, d - 0.012, 0.0004), (sx * (w / 2 - 0.006), 0, z), 'MI_Metal_Green')
            s.cyl(0.006, 0.0004, (sx * 0.054, 0.012 * sx, z), 'MI_Metal_Green', segs=10)
        s.prism(_ellipse(0.016, 0.021, 16), 0.0004, (0, 0, z), 'MI_Metal_Green', axis='Z')
        s.box((0.036, d + 0.0012, t + 0.0012), (0.0, 0, t / 2), band, bevel=0.0003, segs=1)
        return s

    _attach(b, stack('MI_Paper'), (0, 0, 0), (0, 0, 0))
    _attach(b, stack('MI_Plastic_Yellow'), (0.002, 0.002, t + 0.0007), (0, 0, 6))
    return _recentre(b)


def _fork(b, x, y, z, rot_z, mat='MI_Silver'):
    pts = [(-0.006, 0), (0.006, 0), (0.0095, 0.015), (0.0085, 0.07), (0.003, 0.105), (0.0035, 0.12), (0.011, 0.135),
           (0.011, 0.19), (0.0088, 0.19), (0.0085, 0.152), (0.0047, 0.152), (0.0045, 0.19), (0.0015, 0.19),
           (0.0015, 0.152), (-0.0015, 0.152), (-0.0015, 0.19), (-0.0045, 0.19), (-0.0047, 0.152), (-0.0085, 0.152),
           (-0.0088, 0.19), (-0.011, 0.19), (-0.011, 0.135), (-0.0035, 0.12), (-0.003, 0.105), (-0.0085, 0.07),
           (-0.0095, 0.015)]
    b.prism([(px, py - 0.095) for px, py in pts], 0.003, (x, y, z), mat, rot=(0, 0, rot_z), axis='Z')


def _knife(b, x, y, z, rot_z, mat='MI_Silver'):
    pts = [(-0.0065, 0), (0.0065, 0), (0.008, 0.02), (0.007, 0.085), (0.005, 0.09), (0.0075, 0.1), (0.0075, 0.185),
           (0.004, 0.205), (0.0, 0.21), (-0.006, 0.2), (-0.006, 0.1), (-0.005, 0.09), (-0.007, 0.085), (-0.008, 0.02)]
    b.prism([(px, py - 0.105) for px, py in pts], 0.003, (x, y, z), mat, rot=(0, 0, rot_z), axis='Z')


def _spoon(b, x, y, z, rot_z, mat='MI_Silver'):
    s = Builder('spoon')
    pts = [(-0.0065, 0), (0.0065, 0), (0.0095, 0.015), (0.0085, 0.07), (0.0028, 0.115), (0.0028, 0.13), (-0.0028, 0.13),
           (-0.0028, 0.115), (-0.0085, 0.07), (-0.0095, 0.015)]
    s.prism([(px, py - 0.095) for px, py in pts], 0.0025, (0, 0, 0), mat, axis='Z')
    s.sphere(0.0175, (0, 0.058, 0.0005), mat, scale=(1, 1.45, 0.28), segs=10, rings=6)
    _attach(b, s, (x, y, z), (0, 0, rot_z))


@wprop('SM_Loot_Silverware', 'Props/Loot', 'convex', 'loot', size=(0.45, 0.3, 0.09))
def loot_silverware():
    b = Builder('SM_Loot_Silverware')
    w, d, h, t, z0 = 0.45, 0.3, 0.075, 0.014, 0.006
    _box_shell(b, w, d, h, t, z0, 'MI_Wood_Dark', bevel=0.003)
    b.box((w + 0.008, d + 0.008, 0.012), (0, 0, z0 + 0.006), 'MI_Wood_Dark', bevel=0.003)
    b.box((w - 2 * t, d - 2 * t, 0.034), (0, 0, z0 + 0.024), 'MI_Fabric_Curtain', bevel=0.004)
    zf = z0 + 0.041 + 0.0022
    for i in range(18):
        x = -0.186 + i * 0.0219
        r = (i * 37 % 7 - 3) * 0.5
        if i < 6:
            _fork(b, x, 0.0, zf, r)
        elif i < 12:
            _knife(b, x, 0.0, zf, r)
        else:
            _spoon(b, x, -0.004, zf, r)
    top = z0 + h
    for sx in (-1, 1):
        b.torus(0.024, 0.003, (sx * (w / 2 + 0.004), 0, top - 0.025), 'MI_Brass', rot=(90, 0, 90), segs=14, rsegs=5,
                arc=180)
        b.box((0.004, 0.06, 0.012), (sx * (w / 2 + 0.002), 0, top - 0.024), 'MI_Brass', bevel=0.001)
        for sy in (-1, 1):
            b.box((0.024, 0.024, 0.024), (sx * (w / 2 - 0.01), sy * (d / 2 - 0.01), top - 0.011), 'MI_Brass',
                  bevel=0.002)
    b.box((0.03, 0.004, 0.028), (0, -d / 2 - 0.002, top - 0.017), 'MI_Brass', bevel=0.001)
    b.cyl(0.003, 0.004, (0, -d / 2 - 0.004, top - 0.02), 'MI_Plastic_Black', rot=(90, 0, 0), segs=8)
    lid = Builder('lid')
    lid.box((w + 0.004, d + 0.004, 0.022), (0, -d / 2, 0.011), 'MI_Wood_Dark', bevel=0.004)
    lid.prism(_rrect(w - 0.06, d - 0.06, 0.015), 0.008, (0, -d / 2, 0.024), 'MI_Wood_Dark', axis='Z', bevel=0.003)
    lid.box((w - 2 * t, d - 2 * t, 0.006), (0, -d / 2, -0.003), 'MI_Fabric_Curtain', bevel=0.002)
    lid.box((w - 0.08, 0.02, 0.003), (0, -d / 2 + 0.03, -0.0075), 'MI_Leather', bevel=0.001)
    lid.prism(_rrect(0.09, 0.04, 0.006), 0.002, (0, -d / 2, 0.0285), 'MI_Brass', axis='Z')
    _attach(b, lid, (0, d / 2, top), (-100, 0, 0))
    for x in (-0.15, 0.15):
        b.cyl(0.005, 0.05, (x, d / 2 + 0.003, top), 'MI_Brass', rot=(0, 90, 0), segs=8)
    return b


def _profile_r(prof, z):
    for (r0, z0), (r1, z1) in zip(prof, prof[1:]):
        if z0 <= z <= z1 and z1 > z0:
            return r0 + (r1 - r0) * (z - z0) / (z1 - z0)
    return prof[-1][0]


@wprop('SM_Loot_Vase', 'Props/Loot', 'convex', 'loot', size=(0.18, 0.18, 0.42))
def loot_vase():
    b = Builder('SM_Loot_Vase')
    outer = [(0.0, 0.0), (0.05, 0.0), (0.054, 0.004), (0.052, 0.016), (0.056, 0.026), (0.07, 0.06), (0.084, 0.11),
             (0.09, 0.17), (0.088, 0.22), (0.079, 0.27), (0.062, 0.31), (0.043, 0.34), (0.036, 0.36), (0.035, 0.385),
             (0.04, 0.405), (0.045, 0.418)]
    inner = [(0.041, 0.42), (0.036, 0.405), (0.031, 0.385), (0.031, 0.36), (0.0, 0.345)]
    b.lathe(outer + inner, mat='MI_Ceramic', segs=28)
    b.torus(0.0425, 0.0022, (0, 0, 0.419), 'MI_Gold', segs=32, rsegs=6)
    outer_up = outer[1:]

    def band(z0, z1, mat='MI_Car_Paint_Blue', eps=0.0008, n=5):
        b.lathe([(_profile_r(outer_up, z0 + (z1 - z0) * k / (n - 1)) + eps, z0 + (z1 - z0) * k / (n - 1))
                 for k in range(n)], mat=mat, segs=28)

    band(0.03, 0.045)
    band(0.12, 0.126, n=2)
    band(0.228, 0.234, n=2)
    band(0.255, 0.3, n=5)
    band(0.362, 0.38, n=3)
    band(0.392, 0.398, mat='MI_Gold', n=2)
    band(0.026, 0.029, mat='MI_Gold', n=2)
    # Painted scrolling vine around the belly and a ring of lotus petals above the foot.
    vine = []
    for i in range(97):
        a = 2 * math.pi * i / 96
        z = 0.175 + 0.03 * math.sin(a * 5)
        r = _profile_r(outer_up, z) + 0.0012
        vine.append((math.cos(a) * r, math.sin(a) * r, z))
    b.tube(vine, 0.0022, 'MI_Car_Paint_Blue', segs=4, caps=False)
    for i in range(20):
        a = 2 * math.pi * (i + 0.5) / 20
        side = 1 if i % 2 else -1
        z = 0.175 + 0.03 * math.sin(a * 5) + side * 0.016
        r = _profile_r(outer_up, z) + 0.0004
        b.sphere(0.016, (math.cos(a) * r, math.sin(a) * r, z), 'MI_Car_Paint_Blue', scale=(1, 0.08, 0.5),
                 rot=(side * 35, 0, math.degrees(a) + 90), segs=8, rings=4)
    for i in range(12):
        a = 2 * math.pi * i / 12
        r = _profile_r(outer_up, 0.075) + 0.0005
        b.sphere(0.016, (math.cos(a) * r, math.sin(a) * r, 0.075), 'MI_Car_Paint_Blue', scale=(0.5, 0.04, 1.5),
                 rot=(0, -22, math.degrees(a) + 90), segs=8, rings=4)
    return b


def _gamepad(b, loc, rot_z, body='MI_Plastic_Grey'):
    g = Builder('pad')
    outline = [(-0.075, -0.052), (-0.06, -0.062), (-0.042, -0.04), (-0.025, -0.022), (0.025, -0.022), (0.042, -0.04),
               (0.06, -0.062), (0.075, -0.052), (0.08, -0.01), (0.074, 0.022), (0.05, 0.032), (-0.05, 0.032),
               (-0.074, 0.022), (-0.08, -0.01)]
    g.prism(outline, 0.024, (0, 0, 0.014), body, axis='Z', bevel=0.008)
    top = 0.026
    g.box((0.02, 0.0065, 0.004), (-0.047, 0.006, top), 'MI_Plastic_Black', bevel=0.001)
    g.box((0.0065, 0.02, 0.004), (-0.047, 0.006, top), 'MI_Plastic_Black', bevel=0.001)
    for (dx, dy), m in zip(((0, 0.009), (0.009, 0), (0, -0.009), (-0.009, 0)),
                           ('MI_Metal_Green', 'MI_Plastic_Red', 'MI_Metal_Blue', 'MI_Plastic_Yellow')):
        g.cyl(0.0042, 0.004, (0.047 + dx, 0.006 + dy, top), m, segs=10)
    for sx in (-1, 1):
        g.cyl(0.009, 0.006, (sx * 0.02, -0.016, top), 'MI_Plastic_Black', segs=12)
        g.cyl(0.0085, 0.003, (sx * 0.02, -0.016, top + 0.004), 'MI_Rubber_Black', segs=12)
        g.box((0.03, 0.01, 0.008), (sx * 0.05, 0.031, 0.018), 'MI_Plastic_Black', bevel=0.003)
    g.box((0.006, 0.003, 0.002), (-0.006, 0.012, top - 0.001), 'MI_Plastic_Black')
    g.box((0.006, 0.003, 0.002), (0.006, 0.012, top - 0.001), 'MI_Plastic_Black')
    _attach(b, g, loc, (0, 0, rot_z))


@wprop('SM_Loot_Console', 'Props/Loot', 'box', 'loot', size=(0.3, 0.25, 0.07))
def loot_console():
    b = Builder('SM_Loot_Console')
    w, d = 0.3, 0.25
    b.box((w, d, 0.034), (0, 0, 0.005 + 0.017), 'MI_Plastic_Black', bevel=0.004)
    b.box((w, d, 0.03), (0, 0, 0.039 + 0.015), 'MI_Plastic_Laptop', bevel=0.005)
    b.box((w - 0.004, d - 0.004, 0.003), (0, 0, 0.0395), 'MI_Plastic_Grey')
    b.box((0.2, 0.004, 0.003), (-0.02, -d / 2 - 0.0005, 0.03), 'MI_Rubber_Black')
    b.box((0.012, 0.004, 0.008), (0.115, -d / 2 - 0.001, 0.05), 'MI_Plastic_Grey', bevel=0.001)
    b.box((0.012, 0.004, 0.008), (0.115, -d / 2 - 0.001, 0.025), 'MI_Plastic_Grey', bevel=0.001)
    b.box((0.06, 0.002, 0.0025), (-0.02, -d / 2 - 0.0005, 0.0475), 'MI_Emit_Green')
    for x in (-0.13, -0.11):
        b.box((0.013, 0.004, 0.006), (x, -d / 2 - 0.0005, 0.018), 'MI_Plastic_Black')
    for sx in (-1, 1):
        for k in range(9):
            b.box((0.003, 0.18, 0.003), (sx * (w / 2 + 0.0005), 0.0, 0.012 + k * 0.006), 'MI_Plastic_Grey')
        for sy in (-1, 1):
            b.cyl(0.01, 0.005, (sx * 0.12, sy * 0.1, 0.0025), 'MI_Rubber_Black', segs=10)
    _gamepad(b, (0.235, -0.04, 0.0), -24)
    return _recentre(b)


@wprop('SM_Loot_Tablet', 'Props/Loot', 'box', 'loot', size=(0.25, 0.17, 0.008))
def loot_tablet():
    b = Builder('SM_Loot_Tablet')
    w, d = 0.25, 0.17
    b.prism(_rrect(w, d, 0.014), 0.0068, (0, 0, 0.0034), 'MI_Aluminium', axis='Z', bevel=0.0022)
    b.prism(_rrect(w - 0.003, d - 0.003, 0.013), 0.001, (0, 0, 0.0071), 'MI_Plastic_Black', axis='Z')
    b.box((w - 0.024, d - 0.022, 0.0004), (0, 0, 0.0078), 'MI_Screen_Black')
    b.cyl(0.0018, 0.0004, (0, d / 2 - 0.0058, 0.0078), 'MI_Glass_Van', segs=10)
    for x in (0.07, 0.09):
        b.box((0.014, 0.002, 0.0025), (x, d / 2 + 0.0003, 0.0036), 'MI_Aluminium', bevel=0.0006)
    b.box((0.002, 0.012, 0.0025), (w / 2 + 0.0003, 0.05, 0.0036), 'MI_Aluminium', bevel=0.0006)
    b.box((0.01, 0.0015, 0.0018), (0, -d / 2 - 0.0002, 0.0035), 'MI_Plastic_Black')
    return b


@wprop('SM_Loot_Phone', 'Props/Loot', 'box', 'loot', size=(0.075, 0.15, 0.008))
def loot_phone():
    b = Builder('SM_Loot_Phone')
    w, d = 0.075, 0.15
    b.prism(_rrect(w, d, 0.01), 0.0072, (0, 0, 0.0036), 'MI_Plastic_Laptop', axis='Z', bevel=0.002)
    b.prism(_rrect(w - 0.002, d - 0.002, 0.0095), 0.0008, (0, 0, 0.0074), 'MI_Plastic_Black', axis='Z')
    b.prism(_rrect(w - 0.007, d - 0.012, 0.007), 0.0003, (0, 0, 0.00795), 'MI_Screen_Black', axis='Z')
    b.box((0.012, 0.0012, 0.0003), (0, d / 2 - 0.003, 0.00795), 'MI_Plastic_Grey')
    b.cyl(0.0012, 0.0003, (0.009, d / 2 - 0.003, 0.00795), 'MI_Glass_Van', segs=8)
    b.box((0.0015, 0.016, 0.002), (-w / 2 - 0.0002, 0.035, 0.0036), 'MI_Plastic_Grey', bevel=0.0005)
    b.box((0.0015, 0.01, 0.002), (-w / 2 - 0.0002, 0.015, 0.0036), 'MI_Plastic_Grey', bevel=0.0005)
    b.box((0.0015, 0.014, 0.002), (w / 2 + 0.0002, 0.03, 0.0036), 'MI_Plastic_Grey', bevel=0.0005)
    b.box((0.009, 0.0015, 0.0022), (0, -d / 2 - 0.0002, 0.0036), 'MI_Plastic_Black')
    return b


@wprop('SM_Loot_Perfume', 'Props/Loot', 'convex', 'loot', size=(0.07, 0.04, 0.11))
def loot_perfume():
    b = Builder('SM_Loot_Perfume')
    w, d, hb = 0.07, 0.04, 0.072
    cut = 0.009
    oct_ = [(-w / 2 + cut, -d / 2), (w / 2 - cut, -d / 2), (w / 2, -d / 2 + cut), (w / 2, d / 2 - cut), (w / 2 - cut, d / 2),
            (-w / 2 + cut, d / 2), (-w / 2, d / 2 - cut), (-w / 2, -d / 2 + cut)]
    b.prism(oct_, hb, (0, 0, hb / 2), 'MI_Glass', axis='Z', bevel=0.002)
    inner = [(x * 0.8, y * 0.7) for x, y in oct_]
    b.prism(inner, 0.05, (0, 0, 0.011 + 0.025), 'MI_Plastic_Yellow', axis='Z')
    b.prism(_rrect(0.032, 0.016, 0.002), 0.0012, (0, -d / 2 - 0.0004, 0.034), 'MI_Gold', axis='Y')
    b.prism(_rrect(0.026, 0.01, 0.0015), 0.0006, (0, -d / 2 - 0.0012, 0.034), 'MI_Plastic_Black', axis='Y')
    b.cyl(0.008, 0.008, (0, 0, hb + 0.004), 'MI_Glass', segs=16)
    b.lathe([(0, 0), (0.011, 0), (0.0115, 0.002), (0.0105, 0.006), (0, 0.006)], (0, 0, hb + 0.007), 'MI_Gold', segs=20)
    cap = [(math.cos(math.pi * 2 * i / 8 + math.pi / 8) * 0.017, math.sin(math.pi * 2 * i / 8 + math.pi / 8) * 0.017)
           for i in range(8)]
    b.prism(cap, 0.022, (0, 0, hb + 0.013 + 0.011), 'MI_Gold', axis='Z', bevel=0.002)
    b.lathe([(0.016, 0), (0.012, 0.003), (0.0, 0.004)], (0, 0, hb + 0.035), 'MI_Gold', segs=8)
    return b


@wprop('SM_Loot_Whiskey', 'Props/Loot', 'convex', 'loot', size=(0.09, 0.09, 0.3))
def loot_whiskey():
    b = Builder('SM_Loot_Whiskey')
    # The filled part is drawn with the liquid as the outer surface (reads as amber glass), clear glass above.
    b.lathe([(0.0, 0.004), (0.036, 0.0), (0.042, 0.004), (0.043, 0.012), (0.043, 0.17), (0.0, 0.17)], mat='MI_Bakelite',
            segs=24)
    prof = [(0.043, 0.168), (0.043, 0.185), (0.04, 0.205), (0.028, 0.222), (0.0165, 0.236), (0.0148, 0.25),
            (0.0148, 0.268), (0.0165, 0.272), (0.0, 0.272)]
    b.lathe(prof, mat='MI_Glass', segs=24)

    def label(r, a0, a1, z0, z1, mat):
        _sheet(b, lambda u, v: (r * math.cos(a0 + (a1 - a0) * u), r * math.sin(a0 + (a1 - a0) * u), z0 + (z1 - z0) * v),
               10, 1, mat, size=(r * (a1 - a0), z1 - z0))

    label(0.0434, -math.pi * 0.85, -math.pi * 0.15, 0.06, 0.13, 'MI_Paper')
    label(0.0436, -math.pi * 0.85, -math.pi * 0.15, 0.123, 0.128, 'MI_Gold')
    label(0.0436, -math.pi * 0.85, -math.pi * 0.15, 0.062, 0.065, 'MI_Gold')
    label(0.0434, -math.pi * 0.7, -math.pi * 0.3, 0.14, 0.158, 'MI_Plastic_Black')
    b.cyl(0.011, 0.003, (0, -0.0445, 0.088), 'MI_Plastic_Red', rot=(90, 0, 0), segs=14)
    b.lathe([(0.0158, 0.24), (0.0155, 0.262), (0, 0.262)], mat='MI_Gold', segs=20)
    b.lathe([(0, 0.27), (0.0175, 0.27), (0.0185, 0.274), (0.0185, 0.293), (0.016, 0.3), (0, 0.3)], mat='MI_Wood_Dark',
            segs=16)
    return b


@wprop('SM_Loot_Painting', 'Props/Loot', 'box', 'loot', size=(0.45, 0.04, 0.35))
def loot_painting():
    b = Builder('SM_Loot_Painting')
    p = Builder('painting')
    w, h = 0.45, 0.35
    cz = h / 2
    _frame_ring(p, w, h, w - 0.03, h - 0.03, -0.02, 0.02, 'MI_Gold', cz=cz, bevel=0.003)
    _frame_ring(p, w - 0.026, h - 0.026, w - 0.07, h - 0.07, -0.013, 0.02, 'MI_Gold', cz=cz, bevel=0.004)
    _frame_ring(p, w - 0.066, h - 0.066, w - 0.088, h - 0.088, -0.017, 0.02, 'MI_Gold', cz=cz, bevel=0.002)
    for sx in (-1, 1):
        for sz in (-1, 1):
            _blob(p, 0.012, (sx * (w / 2 - 0.016), -0.02, cz + sz * (h / 2 - 0.016)), 'MI_Gold', scale=(1, 0.45, 1),
                  subdiv=2, amp=0.25, freq=3.0, seed=sx * 3 + sz)
    for sx in (-1, 1):
        p.sphere(0.008, (sx * 0.0, -0.019, cz + h / 2 - 0.013), 'MI_Gold', scale=(2.2, 0.5, 1), segs=10, rings=6)
    p.plane(w - 0.086, h - 0.086, (0, -0.004, cz), 'MI_Img_Painting_Landscape')
    p.box((w - 0.086, 0.012, h - 0.086), (0, 0.003, cz), 'MI_Fabric_Shade')
    p.box((0.04, 0.01, 0.17), (0, 0.055, 0.08), 'MI_Cardboard', rot=(-20, 0, 0))
    p.box((w - 0.01, 0.006, h - 0.01), (0, 0.017, cz), 'MI_Cardboard')
    _ground(b, p, rot=(-6, 0, 0))
    return b


@wprop('SM_Loot_Camera', 'Props/Loot', 'box', 'loot', size=(0.14, 0.08, 0.1))
def loot_camera():
    b = Builder('SM_Loot_Camera')
    w, d = 0.14, 0.036
    b.box((w, d, 0.008), (0, 0, 0.004), 'MI_Chrome', bevel=0.003)
    b.box((w - 0.002, d - 0.002, 0.056), (0, 0, 0.036), 'MI_Leather_Black', bevel=0.004)
    b.box((w, d, 0.02), (0, 0, 0.073), 'MI_Chrome', bevel=0.004)
    b.box((0.05, d - 0.006, 0.006), (0, 0, 0.086), 'MI_Chrome', bevel=0.002)
    b.box((0.022, d - 0.012, 0.004), (0, 0, 0.091), 'MI_Steel')
    for x, wd in ((-0.04, 0.018), (0.036, 0.012)):
        b.box((wd, 0.002, 0.009), (x, -d / 2 - 0.0005, 0.074), 'MI_Glass_Van', bevel=0.0008)
        b.box((wd + 0.004, 0.0015, 0.013), (x, -d / 2, 0.074), 'MI_Plastic_Black', bevel=0.001)
    b.cyl(0.011, 0.012, (-0.048, 0, 0.089), 'MI_Chrome', segs=16)
    b.cyl(0.006, 0.006, (-0.048, 0, 0.098), 'MI_Chrome', segs=12)
    b.cyl(0.012, 0.008, (0.034, 0, 0.087), 'MI_Chrome', segs=16)
    b.cyl(0.0035, 0.007, (0.052, -0.006, 0.0865), 'MI_Chrome', segs=10)
    b.box((0.026, 0.008, 0.003), (0.055, 0.009, 0.0855), 'MI_Chrome', rot=(0, 0, 20), bevel=0.001)
    y0 = -d / 2
    b.cyl(0.023, 0.006, (0.0, y0 - 0.003, 0.04), 'MI_Chrome', rot=(90, 0, 0), segs=24)
    b.cyl(0.0215, 0.01, (0.0, y0 - 0.011, 0.04), 'MI_Plastic_Black', rot=(90, 0, 0), segs=24)
    for k in range(12):
        a = 2 * math.pi * k / 12
        b.box((0.002, 0.009, 0.002), (math.cos(a) * 0.0218, y0 - 0.011, 0.04 + math.sin(a) * 0.0218), 'MI_Plastic_Black',
              rot=(0, -math.degrees(a), 0))
    b.cyl(0.019, 0.02, (0.0, y0 - 0.026, 0.04), 'MI_Chrome', rot=(90, 0, 0), segs=24)
    b.torus(0.0175, 0.0018, (0.0, y0 - 0.036, 0.04), 'MI_Plastic_Black', rot=(90, 0, 0), segs=24, rsegs=5)
    b.cyl(0.0155, 0.002, (0.0, y0 - 0.0355, 0.04), 'MI_Glass_Van', rot=(90, 0, 0), segs=24)
    b.cyl(0.006, 0.003, (0.0, y0 - 0.0375, 0.04), 'MI_Glass', rot=(90, 0, 0), segs=12)
    for sx in (-1, 1):
        b.torus(0.004, 0.001, (sx * (w / 2 + 0.002), 0, 0.072), 'MI_Chrome', rot=(0, 90, 0), segs=10, rsegs=4)
    b.cyl(0.004, 0.003, (0.05, -d / 2 - 0.001, 0.05), 'MI_Chrome', rot=(90, 0, 0), segs=10)
    return _recentre(b)


@wprop('SM_Loot_Candlestick', 'Props/Loot', 'convex', 'loot', size=(0.1, 0.1, 0.3))
def loot_candlestick():
    b = Builder('SM_Loot_Candlestick')
    prof = [(0, 0), (0.05, 0), (0.05, 0.006), (0.046, 0.011), (0.032, 0.019), (0.022, 0.028), (0.013, 0.04),
            (0.0095, 0.058), (0.016, 0.072), (0.016, 0.078), (0.009, 0.09), (0.0075, 0.14), (0.012, 0.152),
            (0.012, 0.158), (0.008, 0.168), (0.0095, 0.184), (0.028, 0.194), (0.031, 0.198), (0.028, 0.201),
            (0.0155, 0.203), (0.0155, 0.218), (0.0125, 0.218), (0.0125, 0.208), (0, 0.208)]
    b.lathe(prof, mat='MI_Silver', segs=24)
    b.lathe([(0, 0.208), (0.011, 0.208), (0.011, 0.292), (0.0085, 0.295), (0.004, 0.291), (0, 0.29)], mat='MI_Wax',
            segs=16)
    for a, z, ln in ((0.6, 0.27, 0.03), (2.6, 0.255, 0.05), (4.4, 0.275, 0.02)):
        b.sphere(0.0028, (math.cos(a) * 0.011, math.sin(a) * 0.011, z), 'MI_Wax', scale=(1, 1, ln / 0.006), segs=8,
                 rings=6)
    b.tube([(0, 0, 0.289), (0.0008, 0, 0.297), (0.002, 0.0005, 0.302)], 0.0009, 'MI_Cable', segs=5, bend=0.003)
    return b


def _ballerina(b, loc, mat='MI_Porcelain'):
    x, y, z = loc
    b.cyl(0.0012, 0.008, (x, y, z + 0.004), 'MI_Steel', segs=6)
    b.cyl(0.0024, 0.0016, (x, y, z + 0.0085), 'MI_Gold', segs=10)
    zb = z + 0.009
    b.tube([(x + 0.0012, y, zb), (x + 0.0012, y, zb + 0.016)], 0.0011, mat, segs=6)
    b.tube([(x - 0.0012, y, zb + 0.016), (x - 0.004, y + 0.006, zb + 0.012), (x - 0.006, y + 0.013, zb + 0.015)], 0.001,
           mat, segs=6, bend=0.002)
    b.lathe([(0.0022, zb + 0.015), (0.0085, zb + 0.0165), (0.0095, zb + 0.0175), (0.003, zb + 0.0195), (0, zb + 0.0195)],
            mat='MI_Fabric_Bedding', segs=16)
    b.sphere(0.0028, (x, y, zb + 0.0235), 'MI_Fabric_Curtain', scale=(1, 0.8, 1.7), segs=10, rings=6)
    b.cyl(0.0008, 0.003, (x, y, zb + 0.03), mat, segs=6)
    b.sphere(0.0026, (x, y, zb + 0.0335), mat, segs=10, rings=6)
    b.sphere(0.0016, (x, y + 0.0015, zb + 0.036), 'MI_Bakelite', segs=8, rings=5)
    for sx in (-1, 1):
        b.tube([(x + sx * 0.002, y, zb + 0.028), (x + sx * 0.0055, y, zb + 0.033), (x + sx * 0.0025, y, zb + 0.039)],
               0.0007, mat, segs=5, bend=0.002)


@wprop('SM_Loot_MusicBox', 'Props/Loot', 'convex', 'loot', size=(0.18, 0.12, 0.1))
def loot_music_box():
    b = Builder('SM_Loot_MusicBox')
    w, d, h, t, z0 = 0.18, 0.12, 0.062, 0.01, 0.01
    _box_shell(b, w, d, h, t, z0, 'MI_Wood_Dark')
    b.box((w + 0.008, d + 0.008, 0.012), (0, 0, z0 + 0.006), 'MI_Wood_Dark', bevel=0.003)
    b.box((w + 0.004, d + 0.004, 0.006), (0, 0, z0 + h - 0.003), 'MI_Wood_Dark', bevel=0.002)
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.lathe([(0, 0), (0.006, 0.0), (0.009, 0.004), (0.007, 0.008), (0.009, 0.01), (0, 0.01)],
                    (sx * (w / 2 - 0.005), sy * (d / 2 - 0.005), 0), 'MI_Brass', segs=10)
    # Brass escutcheon and side winding key.
    b.prism(_ellipse(0.009, 0.013, 14), 0.002, (0, -d / 2 - 0.004, z0 + h * 0.55), 'MI_Brass', axis='Y')
    b.cyl(0.002, 0.003, (0, -d / 2 - 0.0055, z0 + h * 0.55 + 0.003), 'MI_Plastic_Black', rot=(90, 0, 0), segs=8)
    b.cyl(0.0025, 0.016, (w / 2 + 0.012, 0, z0 + 0.03), 'MI_Brass', rot=(0, 90, 0), segs=8)
    for sz in (-1, 1):
        b.sphere(0.008, (w / 2 + 0.02, 0, z0 + 0.03 + sz * 0.007), 'MI_Brass', scale=(0.25, 0.8, 1), segs=10, rings=6)
    # Interior: velvet, ballerina on a mirror disc, comb & cylinder under glass.
    zin = z0 + 0.03
    b.box((w - 2 * t, d - 2 * t, 0.006), (0, 0, zin - 0.003), 'MI_Fabric_Curtain', bevel=0.001)
    b.box(((w - 2 * t) * 0.42, d - 2 * t, 0.003), (0.042, 0, z0 + 0.052), 'MI_Glass')
    b.box((0.004, d - 2 * t, 0.024), (0.0, 0, z0 + 0.04), 'MI_Wood_Dark')
    b.cyl(0.009, 0.05, (0.045, 0.012, zin + 0.01), 'MI_Brass', rot=(0, 90, 0), segs=12)
    b.box((0.05, 0.018, 0.003), (0.045, -0.012, zin + 0.008), 'MI_Steel', rot=(25, 0, 0))
    b.cyl(0.014, 0.003, (0.065, 0.012, zin + 0.01), 'MI_Brass', rot=(0, 90, 0), segs=16)
    b.cyl(0.02, 0.002, (-0.042, 0, zin + 0.001), 'MI_Mirror', segs=20)
    b.torus(0.02, 0.0012, (-0.042, 0, zin + 0.001), 'MI_Gold', segs=20, rsegs=4)
    bal = Builder('ballerina')
    _ballerina(bal, (0, 0, 0))
    b.merge(bal, Matrix.Translation((-0.042, 0, zin + 0.002)) @ Matrix.Scale(1.7, 4))
    bal.bm.free()
    # Lid with mirror, open.
    lid = Builder('lid')
    lid.box((w + 0.004, d + 0.004, 0.016), (0, -d / 2, 0.008), 'MI_Wood_Dark', bevel=0.003)
    lid.prism(_rrect(w - 0.03, d - 0.03, 0.01), 0.006, (0, -d / 2, 0.018), 'MI_Wood_Dark', axis='Z', bevel=0.002)
    lid.torus(0.03, 0.0015, (0, -d / 2, 0.0212), 'MI_Brass', segs=24, rsegs=4, scale=(1.4, 0.9, 1))
    lid.sphere(0.006, (0, -d / 2, 0.021), 'MI_Brass', scale=(1, 1, 0.5), segs=10, rings=6)
    lid.box((w - 2 * t, d - 2 * t, 0.002), (0, -d / 2, -0.001), 'MI_Fabric_Curtain')
    lid.box((w - 0.05, d - 0.05, 0.0015), (0, -d / 2, -0.0025), 'MI_Mirror')
    _frame_ring(lid, w - 0.044, d - 0.044, w - 0.052, d - 0.052, -0.004, 0.0, 'MI_Gold', cz=0)
    _attach(b, lid, (0, d / 2 + 0.002, z0 + h), (-100, 0, 0))
    return b


@wprop('SM_Loot_CoinJar', 'Props/Loot', 'convex', 'loot', size=(0.12, 0.12, 0.18))
def loot_coin_jar():
    b = Builder('SM_Loot_CoinJar')
    rng = _rng('SM_Loot_CoinJar')
    prof = [(0, 0.0), (0.054, 0.0), (0.06, 0.006), (0.06, 0.14), (0.054, 0.156), (0.046, 0.162), (0.046, 0.176),
            (0.048, 0.178), (0.047, 0.18), (0.0445, 0.18), (0.0445, 0.168)]
    b.lathe(prof, mat='MI_Glass', segs=28)
    b.lathe([(0, 0.004), (0.054, 0.004), (0.054, 0.1), (0.04, 0.11), (0.02, 0.115), (0, 0.116)], mat='MI_Metal_Black',
            segs=16)
    mats = ('MI_Copper', 'MI_Silver', 'MI_Brass', 'MI_Copper', 'MI_Silver', 'MI_Copper')
    for i in range(52):
        z = 0.012 + (i / 52) * 0.088 + rng.uniform(-0.004, 0.004)
        a = i * 2.39996 + rng.uniform(-0.2, 0.2)
        b.cyl(0.0085 + rng.random() * 0.004, 0.0018, (math.cos(a) * 0.0553, math.sin(a) * 0.0553, z), mats[i % 6],
              rot=(90 + rng.uniform(-40, 40), 0, math.degrees(a) + 90), segs=8)
    for i in range(22):
        a = i * 2.39996
        rr = math.sqrt((i + 0.5) / 22) * 0.048
        z = 0.1 + (1 - (rr / 0.055) ** 2) * 0.016 + 0.001
        b.cyl(0.009 + rng.random() * 0.003, 0.0018, (math.cos(a) * rr, math.sin(a) * rr, z), mats[(i + 2) % 6],
              rot=(rng.uniform(-30, 30), rng.uniform(-30, 30), 0), segs=8)
    return b


@wprop('SM_Loot_MantelClock', 'Props/Loot', 'box', 'loot', size=(0.3, 0.12, 0.22))
def loot_mantel_clock():
    b = Builder('SM_Loot_MantelClock')
    w, d = 0.3, 0.12
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.sphere(0.009, (sx * 0.13, sy * 0.045, 0.006), 'MI_Brass', scale=(1, 1, 0.7), segs=10, rings=6)
    b.box((w, d, 0.026), (0, 0, 0.008 + 0.013), 'MI_Wood_Dark', bevel=0.004)
    b.box((w - 0.016, d - 0.014, 0.012), (0, 0, 0.034 + 0.006), 'MI_Wood_Dark', bevel=0.003)
    b.box((w - 0.04, 0.002, 0.003), (0, -d / 2, 0.021), 'MI_Brass')
    z0 = 0.046
    hump = []
    n = 28
    for i in range(n + 1):
        x = -0.135 + 0.27 * i / n
        hump.append((x, z0 + 0.024 + 0.15 * math.exp(-(x / 0.07) ** 2) + 0.02 * math.exp(-(x / 0.13) ** 2)))
    outline = [(-0.135, z0)] + hump + [(0.135, z0)]
    b.prism(outline, 0.088, (0, 0.0, 0), 'MI_Wood_Dark', axis='Y', bevel=0.004)
    cz = z0 + 0.1
    yf = -0.044
    b.torus(0.06, 0.005, (0, yf - 0.002, cz), 'MI_Brass', rot=(90, 0, 0), segs=36, rsegs=8)
    _disc(b, 0.056, 0.056, (0, yf - 0.0015, cz), 'MI_Img_ClockFace', segs=32)
    b.cyl(0.057, 0.003, (0, yf - 0.004, cz), 'MI_Glass', rot=(90, 0, 0), segs=32)
    for sx in (-1, 1):
        b.sphere(0.006, (sx * 0.1, yf - 0.001, z0 + 0.035), 'MI_Brass', scale=(1, 0.4, 1), segs=10, rings=6)
    b.cyl(0.004, 0.003, (0.022, yf - 0.006, cz - 0.03), 'MI_Brass', rot=(90, 0, 0), segs=8)
    b.cyl(0.004, 0.003, (-0.022, yf - 0.006, cz - 0.03), 'MI_Brass', rot=(90, 0, 0), segs=8)
    return b


@wprop('SM_Loot_Figurine', 'Props/Loot', 'convex', 'loot', size=(0.08, 0.08, 0.2))
def loot_figurine():
    b = Builder('SM_Loot_Figurine')
    m = 'MI_Porcelain'
    b.lathe([(0, 0), (0.038, 0), (0.0395, 0.005), (0.038, 0.012), (0.036, 0.016), (0, 0.016)], mat=m, segs=20)
    b.torus(0.0388, 0.0014, (0, 0, 0.006), 'MI_Gold', segs=20, rsegs=4)
    g = Builder('girl')
    # Dress with a wavy hem.
    dress = Builder('dress')
    prof = [(0.031, 0.012), (0.03, 0.03), (0.024, 0.06), (0.016, 0.085), (0.0115, 0.1), (0, 0.1)]
    dress.lathe(prof, mat=m, segs=18)
    for v in dress.bm.verts:
        if v.co.z < 0.08:
            a = math.atan2(v.co.y, v.co.x)
            k = 1 + 0.07 * math.sin(a * 7) * (0.08 - v.co.z) / 0.068
            v.co.x *= k
            v.co.y *= k
    _attach(g, dress)
    g.lathe([(0.0205, 0.045), (0.022, 0.07), (0.0155, 0.09), (0.0115, 0.096)], (0, -0.004, 0), 'MI_Car_Paint_Blue',
            segs=20, scale=(1, 0.8, 1))
    g.torus(0.0118, 0.002, (0, 0, 0.098), 'MI_Car_Paint_Blue', segs=20, rsegs=5)
    g.lathe([(0.0115, 0.098), (0.0125, 0.11), (0.0118, 0.125), (0.007, 0.135), (0, 0.137)], mat=m, segs=16)
    g.cyl(0.0045, 0.012, (0, 0, 0.14), m, segs=10)
    # Head, hair and bonnet.
    hz = 0.157
    g.sphere(0.0135, (0, 0, hz), m, scale=(1, 0.95, 1.05), segs=12, rings=8)
    g.sphere(0.0143, (0, 0.003, hz + 0.003), 'MI_Bakelite', scale=(1, 0.95, 1.0), segs=12, rings=8)
    for sx in (-1, 1):
        g.tube([(sx * 0.011, 0.002, hz - 0.004), (sx * 0.014, 0.0, hz - 0.02), (sx * 0.012, -0.002, hz - 0.03)], 0.0035,
               'MI_Bakelite', segs=8, bend=0.006)
        g.sphere(0.0011, (sx * 0.0045, -0.0128, hz + 0.001), 'MI_Plastic_Black', segs=6, rings=4)
        g.sphere(0.0045, (sx * 0.0145, 0, 0.128), m, segs=10, rings=6)
    g.sphere(0.0012, (0, -0.0132, hz - 0.006), 'MI_Plastic_Red', scale=(1.6, 1, 0.8), segs=6, rings=4)
    g.lathe([(0.0, 0.0), (0.019, -0.004), (0.02, -0.0025), (0.016, 0.001), (0.0135, 0.008), (0, 0.012)],
            (0, 0.004, hz + 0.004), m, rot=(-28, 0, 0), segs=20)
    g.torus(0.0145, 0.0016, (0, 0.005, hz + 0.006), 'MI_Car_Paint_Blue', rot=(-28, 0, 0), segs=20, rsegs=4)
    # Arms holding a gilt flower basket in front.
    for sx in (-1, 1):
        g.tube([(sx * 0.0145, 0, 0.126), (sx * 0.017, -0.006, 0.108), (sx * 0.008, -0.016, 0.1)], 0.0032, m, segs=8,
               bend=0.006)
    g.lathe([(0, 0), (0.009, 0.0), (0.012, 0.009), (0.0105, 0.009), (0.0075, 0.002), (0, 0.002)], (0, -0.019, 0.092),
            'MI_Gold', segs=16)
    g.torus(0.009, 0.0009, (0, -0.019, 0.101), 'MI_Gold', rot=(0, 90, 0), segs=12, rsegs=4, arc=180)
    for i, (dx, dy) in enumerate(((0.004, 0.0), (-0.004, 0.002), (0.0, -0.004), (0.0, 0.004))):
        g.sphere(0.0032, (dx, -0.019 + dy, 0.1015), ('MI_Plastic_Red', 'MI_Porcelain', 'MI_Plastic_Yellow',
                                                     'MI_Car_Paint_Blue')[i], segs=8, rings=5)
    b.merge(g, Matrix.Translation((0, 0, 0.016)) @ Matrix.Scale(1.12, 4) @ Matrix.Translation((0, 0, -0.012)))
    g.bm.free()
    return b


@wprop('SM_Loot_Handbag', 'Props/Loot', 'convex', 'loot', size=(0.3, 0.12, 0.22))
def loot_handbag():
    """Trapezoid designer bag: black top flap with a gold turn-lock, arched handle, dangling padlock."""
    b = Builder('SM_Loot_Handbag')
    m, mf = 'MI_Leather', 'MI_Leather_Black'
    z0, zt, tw = 0.004, 0.155, 0.1
    b.prism([(-0.15, z0), (0.15, z0), (tw, zt), (-tw, zt)], 0.11, (0, 0, 0), m, axis='Y', bevel=0.016)
    # Flap over the top, folding down the front.
    b.box((2 * tw - 0.004, 0.082, 0.006), (0, -0.016, zt + 0.001), mf, bevel=0.0025)
    ang = math.atan2(0.15 - tw, zt - z0)
    xb = tw + (0.15 - tw) * (zt - 0.09) / (zt - z0)
    b.prism([(-tw + 0.002, zt + 0.002), (tw - 0.002, zt + 0.002), (xb - 0.004, 0.09), (0.0, 0.074),
             (-xb + 0.004, 0.09)], 0.006, (0, -0.0575, 0), mf, axis='Y', bevel=0.0025)
    b.tube([(-tw, -0.0608, zt), (-xb + 0.002, -0.0608, 0.09), (0.0, -0.0608, 0.073), (xb - 0.002, -0.0608, 0.09),
            (tw, -0.0608, zt)], 0.0009, 'MI_Fabric_Shade', segs=4)
    # Turn-lock.
    b.box((0.03, 0.005, 0.02), (0, -0.0605, 0.086), 'MI_Gold', bevel=0.002)
    b.cyl(0.0058, 0.005, (0, -0.064, 0.086), 'MI_Gold', rot=(90, 0, 0), segs=12)
    b.box((0.003, 0.003, 0.011), (0, -0.0675, 0.086), 'MI_Gold', bevel=0.0008)
    # Contrast stitching along the body edges.
    for y in (-0.0552, 0.0552):
        for sx in (-1, 1):
            b.tube([(sx * 0.141, y, z0 + 0.012), (sx * (tw - 0.006), y, zt - 0.01)], 0.0007, 'MI_Fabric_Shade', segs=4)
        b.tube([(-0.138, y, z0 + 0.01), (0.138, y, z0 + 0.01)], 0.0007, 'MI_Fabric_Shade', segs=4)
    # Arched rolled handle with gold fittings.
    arch = [(math.cos(math.pi * k / 10) * 0.058, 0, zt + 0.006 + math.sin(math.pi * k / 10) * 0.06) for k in range(11)]
    b.tube(arch, 0.0065, mf, segs=10)
    for sx in (-1, 1):
        b.torus(0.0075, 0.0018, (sx * 0.058, 0, zt + 0.008), 'MI_Gold', rot=(90, 0, 0), segs=12, rsegs=4)
        b.box((0.016, 0.03, 0.004), (sx * 0.058, 0, zt + 0.005), 'MI_Gold', bevel=0.001)
    # Padlock and key bell hanging from the handle.
    b.torus(0.006, 0.0012, (-0.045, -0.008, zt + 0.038), 'MI_Gold', rot=(90, 0, 0), segs=10, rsegs=4)
    b.box((0.016, 0.007, 0.015), (-0.045, -0.008, zt + 0.024), 'MI_Gold', bevel=0.003)
    b.cyl(0.0016, 0.002, (-0.045, -0.0118, zt + 0.022), 'MI_Plastic_Black', rot=(90, 0, 0), segs=6)
    b.tube([(0.05, 0.004, zt + 0.035), (0.068, 0.006, zt + 0.012)], 0.0018, m, segs=5)
    b.lathe([(0, 0.0), (0.0075, 0.0), (0.0065, 0.012), (0.003, 0.02), (0, 0.021)], (0.07, 0.006, zt - 0.01), m,
            segs=12)
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.cyl(0.005, 0.005, (sx * 0.11, sy * 0.035, 0.0025), 'MI_Gold', segs=10)
    return b


@wprop('SM_Loot_Locket', 'Props/Loot', 'convex', 'loot', size=(0.08, 0.08, 0.03))
def loot_locket():
    """Cursed lore loot: an antique oval locket lying open like a book on a velvet cushion - the
    family photo in the body, the portrait from the cursed painting inside the lid - with a
    blood-red stone set in the bail."""
    b = Builder('SM_Loot_Locket')
    s, h = 0.074, 0.017
    b.box((s, s, h), (0, 0, h / 2), 'MI_Fabric_Curtain', bevel=0.0065, segs=3)
    b.sphere(0.0032, (0, 0, h + 0.0002), 'MI_Gold', scale=(1, 1, 0.5), segs=8, rings=5)
    loop = _rrect(s - 0.003, s - 0.003, 0.0075, 3)
    b.tube([(x, y, h * 0.5) for x, y in loop + loop[:1]], 0.0011, 'MI_Gold', segs=5)
    for sx in (-1, 1):
        for sy in (-1, 1):
            cx, cy = sx * (s / 2 + 0.0005), sy * (s / 2 + 0.0005)
            b.sphere(0.0022, (cx, cy, h * 0.5), 'MI_Gold', segs=8, rings=5)
            b.lathe([(0, 0.0), (0.0024, 0.0), (0.0026, 0.004), (0.0012, 0.0068), (0, 0.007)], (cx, cy, 0.0005),
                    'MI_Gold', segs=8)
    zt = h + 0.0004
    rx, ry = 0.0134, 0.0178
    halves = ((-0.0152, 'MI_Img_Photo_Family', (0.19, 0.0, 0.81, 1.0)), (0.0152, 'MI_Img_Painting_Lore', (0, 0, 1, 1)))
    for cx, img, uvr in halves:
        hb = Builder('half')
        hb.sphere(1.0, (0, 0, 0.0018), 'MI_Gold', scale=(rx + 0.0018, ry + 0.0018, 0.0028), segs=18, rings=6)
        hb.torus(1.0, 0.001 / rx, (0, 0, 0.004), 'MI_Gold', segs=24, rsegs=4, scale=(rx, ry, rx))
        _disc(hb, rx - 0.0012, ry - 0.0012, (0, 0, 0.0042), img, rot=(-90, 0, 0), segs=24, uv_rect=uvr)
        for k in range(10):
            a = 2 * math.pi * k / 10
            hb.sphere(0.0009, (math.cos(a) * (rx + 0.0019), math.sin(a) * (ry + 0.0019), 0.0034), 'MI_Gold', segs=5,
                      rings=3)
        if cx < 0:
            _attach(b, hb, (cx, 0, zt))
        else:
            # The lid half rests slightly raised on its hinge (asymmetric, so it reads as a locket).
            b.merge(hb, Matrix.Translation((0, 0, zt + 0.0016)) @ Matrix.Rotation(math.radians(-12), 4, 'Y') @
                    Matrix.Translation((cx, 0, -0.0016)))
            hb.bm.free()
    # Hinge knuckles between the halves.
    for y in (-0.008, 0.0, 0.008):
        b.cyl(0.0014, 0.0055, (0, y, zt + 0.0016), 'MI_Gold', rot=(90, 0, 0), segs=8)
    # Bail on top of the body half, set with a blood-red stone, and the chain.
    bx, by = -0.0152, ry + 0.0035
    b.sphere(0.0028, (bx, by - 0.0008, zt + 0.0022), 'MI_Gold', scale=(1, 1, 0.8), segs=10, rings=6)
    b.lathe([(0, -0.0012), (0.0019, 0.0), (0.0021, 0.0006), (0.001, 0.0014), (0, 0.0014)], (bx, by - 0.0008, zt + 0.0044),
            'MI_Emit_Red', segs=8)
    b.torus(0.0028, 0.0008, (bx, by + 0.0035, zt + 0.0012), 'MI_Gold', segs=12, rsegs=4)
    _chain(b, [(bx, by + 0.0062, zt + 0.0008), (-0.022, 0.0315, zt + 0.0004), (-0.0335, 0.024, zt - 0.0004),
               (-0.0345, -0.004, zt - 0.0006), (-0.03, -0.029, zt - 0.0004), (-0.006, -0.0335, zt - 0.0002),
               (0.016, -0.0325, zt - 0.0002), (0.032, -0.024, zt - 0.0004), (0.0345, 0.006, zt - 0.0006),
               (0.029, 0.029, zt - 0.0004), (0.006, 0.0335, zt + 0.0002), (bx + 0.002, by + 0.0068, zt + 0.0008)],
           link=0.006, wire=0.00065, segs=4, rsegs=3)
    return b


@wprop('SM_Loot_CoinCollection', 'Props/Loot', 'box', 'loot', size=(0.36, 0.26, 0.06))
def loot_coin_collection():
    """A coin collector's display case (first job's primary target): 5 x 4 old coins in velvet
    recesses with little paper labels, under a hinged glass lid."""
    b = Builder('SM_Loot_CoinCollection')
    rng = _rng('SM_Loot_CoinCollection')
    w, d, t = 0.36, 0.26, 0.016
    hb = 0.042
    _box_shell(b, w, d, hb, t, 0.0, 'MI_Wood_Dark', bevel=0.003)
    b.box((w + 0.006, d + 0.006, 0.008), (0, 0, 0.004), 'MI_Wood_Dark', bevel=0.003)
    zv = 0.03
    b.box((w - 2 * t, d - 2 * t, zv - 0.008), (0, 0, 0.008 + (zv - 0.008) / 2), 'MI_Velvet_Blue', bevel=0.002)
    mats = ('MI_Silver', 'MI_Gold', 'MI_Copper', 'MI_Silver', 'MI_Gold')
    for j in range(4):
        for i in range(5):
            x = -0.13 + i * 0.065
            y = 0.075 - j * 0.052
            r = 0.0145 + rng.random() * 0.0045
            m = mats[(i + j * 2) % 5]
            th = 0.0024
            b.lathe([(0, 0), (r, 0), (r, th), (r - 0.0013, th), (r - 0.0016, th - 0.0008), (0, th - 0.0008)],
                    (x, y, zv - 0.0006), m, rot=(0, 0, rng.uniform(0, 360)), segs=12)
            b.sphere(r * 0.5, (x + rng.uniform(-0.001, 0.001), y, zv + th - 0.0008), m, scale=(0.8, 1, 0.12),
                     segs=7, rings=3)
            b.box((0.026, 0.008, 0.0006), (x, y - r - 0.0075, zv + 0.0003), 'MI_Paper')
    # Glass lid in a wooden frame (authored upright in XZ, laid flat), closed; brass hinges and clasp.
    fw, ft = 0.022, 0.016
    lid = Builder('lid')
    _frame_ring(lid, w, d, w - 2 * fw, d - 2 * fw, -ft / 2, ft / 2, 'MI_Wood_Dark', bevel=0.003)
    lid.box((w - 2 * fw + 0.01, 0.004, d - 2 * fw + 0.01), (0, 0, 0), 'MI_Glass')
    _frame_ring(lid, w - 2 * fw + 0.004, d - 2 * fw + 0.004, w - 2 * fw - 0.003, d - 2 * fw - 0.003, -ft / 2 - 0.0008,
                -ft / 2 + 0.004, 'MI_Gold')
    lid.box((0.06, 0.002, 0.012), (0, -ft / 2 - 0.0005, -d / 2 + fw / 2), 'MI_Brass', bevel=0.0006)
    _attach(b, lid, (0, 0, hb + ft / 2), (-90, 0, 0))
    for x in (-0.11, 0.11):
        b.cyl(0.0045, 0.04, (x, d / 2 + 0.002, hb), 'MI_Brass', rot=(0, 90, 0), segs=8)
    b.box((0.022, 0.004, 0.024), (0, -d / 2 - 0.002, hb), 'MI_Brass', bevel=0.001)
    b.cyl(0.0025, 0.003, (0, -d / 2 - 0.0042, hb - 0.004), 'MI_Plastic_Black', rot=(90, 0, 0), segs=8)
    return b


LOOT = [n for n, w in WORLD.items() if w['group'] == 'loot']


# =======================================================================================
# 2. INTERIORS (Props/House)

def _commit_multi(b, pb, face_mats, smooth=True):
    """Like Builder._commit, but with a material per face (patchwork quilts, rugs)."""
    for f, m in zip(pb.faces, face_mats):
        f.material_index = b._slot(m)
        f.smooth = smooth
    tmp = bpy.data.meshes.new('_tmp')
    pb.to_mesh(tmp)
    pb.free()
    b.bm.from_mesh(tmp)
    bpy.data.meshes.remove(tmp)


def _turned_leg(b, x, y, z0, h, r, mat, rings=True):
    """Lathe-turned furniture leg standing from z0, height h."""
    if rings:
        prof = [(0, 0), (r * 0.7, 0), (r * 0.8, h * 0.04), (r * 0.7, h * 0.1), (r * 0.75, h * 0.45), (r * 1.05, h * 0.5),
                (r * 1.15, h * 0.55), (r * 0.8, h * 0.62), (r * 0.85, h * 0.8), (r * 1.1, h * 0.84), (r, h * 0.88),
                (r, h), (0, h)]
    else:
        prof = [(0, 0), (r * 0.65, 0), (r * 0.7, h * 0.05), (r, h * 0.95), (r, h), (0, h)]
    b.lathe(prof, (x, y, z0), mat, segs=10)


def _panel_door(b, x, z, w, h, y_front, mat, t=0.02, frame=0.06, raised=True, bevel=0.003):
    """Frame-and-panel door / drawer front centred at (x, z), its front face at y_front (-Y)."""
    yc = y_front + t / 2
    b.box((w, t, h), (x, yc, z), mat, bevel=bevel)
    if raised and w > 2.5 * frame and h > 2.5 * frame:
        pw, ph = w - 2 * frame, h - 2 * frame
        for sx in (-1, 1):
            b.box((frame * 0.25, 0.006, ph + frame * 0.5), (x + sx * (pw / 2 + frame * 0.125), y_front - 0.003, z), mat,
                  bevel=0.0015)
        for sz in (-1, 1):
            b.box((pw, 0.006, frame * 0.25), (x, y_front - 0.003, z + sz * (ph / 2 + frame * 0.125)), mat, bevel=0.0015)
        b.box((pw - frame * 0.6, 0.008, ph - frame * 0.6), (x, y_front - 0.004, z), mat, bevel=0.004)


def _drawer(b, x, z, w, h, y_front, mat, knob='MI_Brass', knobs=2, pull=False):
    b.box((w, 0.02, h), (x, y_front + 0.01, z), mat, bevel=0.003)
    if pull:
        _bar_handle(b, x, z, min(0.16, w * 0.4), y_front, 'MI_Chrome', r=0.005, stand=0.025)
    elif knobs == 1:
        _knob(b, (x, y_front, z), knob, r=0.011)
    else:
        for sx in (-1, 1):
            _knob(b, (x + sx * w * 0.27, y_front, z), knob, r=0.011)


@wprop('SM_Sofa_3Seat', 'Props/House', 'box', 'house', size=(2.1, 0.9, 0.85))
def sofa_3seat():
    b = Builder('SM_Sofa_3Seat')
    m = 'MI_Fabric_Sofa'
    w, d = 2.1, 0.9
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.cyl(0.026, 0.1, (sx * (w / 2 - 0.1), sy * (d / 2 - 0.09), 0.05), 'MI_Wood_Dark', segs=10, radius2=0.018)
            b.cyl(0.03, 0.012, (sx * (w / 2 - 0.1), sy * (d / 2 - 0.09), 0.006), 'MI_Brass', segs=10)
    b.box((w - 0.06, d - 0.04, 0.24), (0, 0.0, 0.1 + 0.12), m, bevel=0.035, segs=3)
    b.tube([(-w / 2 + 0.2, -d / 2 + 0.03, 0.115), (w / 2 - 0.2, -d / 2 + 0.03, 0.115)], 0.006, m, segs=6)
    # Rolled arms.
    for sx in (-1, 1):
        x = sx * (w / 2 - 0.11)
        b.box((0.2, d - 0.02, 0.36), (x, 0.0, 0.1 + 0.18 + 0.08), m, bevel=0.05, segs=3)
        b.cyl(0.115, d - 0.04, (x + sx * 0.012, -0.0, 0.6), m, rot=(90, 0, 0), segs=16)
        b.cyl(0.085, 0.012, (x + sx * 0.012, -d / 2 + 0.02 - 0.004, 0.6), m, rot=(90, 0, 0), segs=16)
    # Back frame.
    b.box((w - 0.3, 0.2, 0.62), (0, d / 2 - 0.11, 0.1 + 0.31 + 0.12), m, bevel=0.06, segs=3)
    # Seat and back cushions.
    cw = (w - 0.42) / 3
    for i in range(3):
        x = (i - 1) * cw
        b.box((cw - 0.01, 0.66, 0.16), (x, -0.08, 0.42), m, rot=(0, 0, (i - 1) * 0.8), bevel=0.05, segs=3)
        b.box((cw - 0.02, 0.2, 0.42), (x, 0.21, 0.67), m, rot=(-11, (i - 1) * 1.5, 0), bevel=0.07, segs=3)
    # A throw pillow in the corner.
    b.box((0.4, 0.13, 0.4), (-w / 2 + 0.42, 0.07, 0.66), 'MI_Fabric_Sofa_Green', rot=(-18, 0, 22), bevel=0.055, segs=3)
    return b


@wprop('SM_Armchair_Wing', 'Props/House', 'box', 'house', size=(0.85, 0.85, 1.05))
def armchair_wing():
    b = Builder('SM_Armchair_Wing')
    m = 'MI_Fabric_Sofa_Green'
    w, d = 0.85, 0.85
    for sx in (-1, 1):
        b.lathe([(0, 0), (0.02, 0), (0.024, 0.02), (0.018, 0.08), (0.026, 0.15), (0.03, 0.16), (0, 0.16)],
                (sx * 0.33, -0.33, 0), 'MI_Wood_Dark', segs=10)
        b.cyl(0.024, 0.16, (sx * 0.33, 0.34, 0.08), 'MI_Wood_Dark', rot=(-8, 0, 0), segs=10, radius2=0.03)
    b.box((w - 0.08, d - 0.06, 0.22), (0, 0.0, 0.15 + 0.11), m, bevel=0.03, segs=2)
    b.tube([(-w / 2 + 0.06, -d / 2 + 0.035, 0.155), (w / 2 - 0.06, -d / 2 + 0.035, 0.155)], 0.006, m, segs=6)
    b.box((0.55, 0.62, 0.13), (0, -0.07, 0.43), m, bevel=0.045, segs=3)
    # Back with an arched top, slightly reclined, button tufted.
    back = [(-0.33, 0.0)]
    for k in range(9):
        t = k / 8
        back.append((-0.33 + 0.66 * t, 0.62 + 0.06 * math.sin(math.pi * t)))
    back.append((0.33, 0.0))
    b.prism(back, 0.16, (0, d / 2 - 0.1, 0.36), m, rot=(-8, 0, 0), axis='Y', bevel=0.05)
    for row, zz in enumerate((0.52, 0.7, 0.88)):
        for k in range(3 if row != 1 else 2):
            xx = (k - 1) * 0.17 if row != 1 else (k - 0.5) * 0.17
            yy = d / 2 - 0.1 - 0.08 - 0.002 + (zz - 0.36) * math.tan(math.radians(8))
            b.sphere(0.011, (xx, yy, zz), m, scale=(1, 0.5, 1), segs=8, rings=5)
    # Wings and scroll arms.
    for sx in (-1, 1):
        x = sx * (w / 2 - 0.065)
        wing = [(0.36, 0.6), (0.36, 1.0), (0.24, 1.04), (0.06, 1.0), (-0.07, 0.9), (-0.12, 0.78), (-0.1, 0.62)]
        b.prism(wing, 0.11, (x, 0, 0), m, axis='X', bevel=0.035)
        b.box((0.13, d - 0.08, 0.27), (x, 0.0, 0.37 + 0.12), m, bevel=0.04, segs=3)
        b.cyl(0.075, d - 0.1, (x + sx * 0.01, -0.02, 0.62), m, rot=(90, 0, 0), segs=14)
        b.lathe([(0, 0), (0.07, 0), (0.075, 0.006), (0, 0.008)], (x + sx * 0.01, -d / 2 + 0.03, 0.62), m,
                rot=(90, 0, 0), segs=14)
    return b


@wprop('SM_TVStand_Low', 'Props/House', 'box', 'house', size=(1.5, 0.45, 0.5))
def tv_stand_low():
    b = Builder('SM_TVStand_Low')
    m = 'MI_Cabinet_Wood'
    w, d, h = 1.5, 0.45, 0.5
    yf = -d / 2 + 0.02
    b.box((w, d, 0.03), (0, 0, h - 0.015), m, bevel=0.006)
    b.box((w - 0.04, d - 0.04, 0.02), (0, 0.01, 0.1), m)
    for x in (-w / 2 + 0.02, -0.29, 0.29, w / 2 - 0.02):
        b.box((0.025, d - 0.04, h - 0.12), (x, 0.01, 0.1 + (h - 0.12) / 2), m, bevel=0.003)
    b.box((w - 0.04, 0.012, h - 0.12), (0, d / 2 - 0.016, 0.1 + (h - 0.12) / 2), m)
    b.box((0.56, d - 0.06, 0.018), (0, 0.01, 0.27), m)
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.cyl(0.022, 0.09, (sx * (w / 2 - 0.08), sy * (d / 2 - 0.07), 0.045), 'MI_Wood_Dark', segs=10,
                  radius2=0.016)
    for sx in (-1, 1):
        x = sx * 0.515
        _panel_door(b, x, 0.28, 0.43, 0.34, yf - 0.02, m, frame=0.05)
        _knob(b, (x - sx * 0.17, yf - 0.026, 0.3), 'MI_Brass', r=0.01)
    # A VCR and tapes in the open middle bay.
    b.box((0.42, 0.28, 0.085), (0, 0.02, 0.11 + 0.0425), 'MI_Plastic_Black', bevel=0.004)
    b.box((0.2, 0.004, 0.022), (-0.06, -0.122, 0.16), 'MI_Plastic_Grey', bevel=0.001)
    b.box((0.06, 0.003, 0.014), (0.13, -0.121, 0.165), 'MI_Emit_Green')
    for k in range(4):
        b.box((0.018, 0.004, 0.01), (0.09 + k * 0.025, -0.121, 0.13), 'MI_Plastic_Grey')
    for k in range(7):
        b.box((0.026, 0.19, 0.105), (-0.24 + k * 0.03, 0.04, 0.28 + 0.0525),
              ('MI_Plastic_Black', 'MI_Plastic_Black', 'MI_Plastic_Grey', 'MI_Paper')[k % 4], bevel=0.002)
    b.box((0.19, 0.105, 0.026), (0.16, 0.0, 0.28 + 0.013), 'MI_Plastic_Black', bevel=0.002)
    b.box((0.19, 0.105, 0.026), (0.165, 0.004, 0.28 + 0.039), 'MI_Plastic_Black', rot=(0, 0, 6), bevel=0.002)
    return b


BOOK_MATS = ('MI_Leather', 'MI_Leather_Black', 'MI_Fabric_Navy', 'MI_Fabric_Rug', 'MI_Fabric_Olive', 'MI_Fabric_Brown',
             'MI_Fabric_Sofa_Green', 'MI_Fabric_Curtain', 'MI_Cardboard', 'MI_Velvet_Blue')


def _book(b, x, y, z, t, d, h, mat, rot=(0, 0, 0)):
    """Book standing on its bottom at (x, y, z) with the spine facing -Y; pages show on the top."""
    s = Builder('book')
    s.box((t, d, h), (0, 0, h / 2), mat, bevel=0.0015, segs=1)
    s.box((t - 0.005, d - 0.008, 0.003), (0, 0.003, h - 0.0012), 'MI_Paper')
    _attach(b, s, (x, y, z), rot)


def _book_row(b, rng, x0, x1, y_back, z, hmax, depth=0.22):
    x = x0
    while x < x1 - 0.03:
        r = rng.random()
        if r < 0.07 and x1 - x > 0.25:          # a lying stack
            n = rng.randint(2, 4)
            zz = z
            for k in range(n):
                th = rng.uniform(0.025, 0.04)
                dd = rng.uniform(0.15, min(0.21, depth))
                hh = rng.uniform(0.17, 0.24)
                s = Builder('lying')
                s.box((hh, dd, th), (0, 0, th / 2), rng.choice(BOOK_MATS), bevel=0.0015, segs=1)
                _attach(b, s, (x + 0.12, y_back - dd / 2 - 0.01, zz), (0, 0, rng.uniform(-6, 6)))
                zz += th
            x += 0.26
            continue
        if r < 0.11:                             # a gap
            x += rng.uniform(0.04, 0.09)
            continue
        t = rng.uniform(0.018, 0.05)
        hh = rng.uniform(0.6, 1.0) * hmax
        dd = rng.uniform(0.13, depth)
        lean = 0.0
        if x + t > x1 - 0.02:
            break
        if rng.random() < 0.05:
            lean = -rng.uniform(8, 16)
        _book(b, x + t / 2, y_back - dd / 2 - 0.005, z, t, dd, hh, rng.choice(BOOK_MATS), rot=(0, lean, 0))
        x += t + 0.0015 + (abs(lean) * 0.006 if lean else 0)


@wprop('SM_Bookshelf_Tall', 'Props/House', 'box', 'house', size=(0.9, 0.32, 1.9))
def bookshelf_tall():
    b = Builder('SM_Bookshelf_Tall')
    rng = _rng('SM_Bookshelf_Tall')
    m = 'MI_Wood_Dark'
    w, d, h = 0.9, 0.32, 1.9
    for sx in (-1, 1):
        b.box((0.022, d, h - 0.03), (sx * (w / 2 - 0.011), 0, (h - 0.03) / 2), m, bevel=0.003)
    b.box((w + 0.0, d, 0.03), (0, 0, h - 0.015), m, bevel=0.005)
    b.box((w - 0.04, 0.008, h - 0.05), (0, d / 2 - 0.006, h / 2), m)
    b.box((w - 0.044, 0.02, 0.07), (0, -d / 2 + 0.03, 0.035), m, bevel=0.003)
    shelves = (0.08, 0.44, 0.8, 1.16, 1.52)
    for z in shelves:
        b.box((w - 0.044, d - 0.02, 0.022), (0, 0.0, z - 0.011), m, bevel=0.002)
        b.box((w - 0.044, 0.018, 0.03), (0, -d / 2 + 0.019, z - 0.012), m, bevel=0.002)
    for i, z in enumerate(shelves):
        top = (shelves[i + 1] - 0.022) if i + 1 < len(shelves) else h - 0.03
        _book_row(b, rng, -w / 2 + 0.03, w / 2 - 0.03, d / 2 - 0.012, z, min(0.31, top - z - 0.02), depth=0.24)
    return b


@wprop('SM_SideTable', 'Props/House', 'box', 'house', size=(0.5, 0.5, 0.6))
def side_table():
    b = Builder('SM_SideTable')
    m = 'MI_Cabinet_Wood'
    b.box((0.5, 0.5, 0.03), (0, 0, 0.585), m, bevel=0.007)
    for sx in (-1, 1):
        for sy in (-1, 1):
            _turned_leg(b, sx * 0.205, sy * 0.205, 0.0, 0.57, 0.018, m)
    for sx in (-1, 1):
        b.box((0.02, 0.37, 0.1), (sx * 0.205, 0, 0.515), m)
    b.box((0.37, 0.02, 0.1), (0, 0.205, 0.515), m)
    _drawer(b, 0, 0.515, 0.36, 0.085, -0.225, m, knobs=1)
    b.box((0.44, 0.44, 0.015), (0, 0, 0.16), m, bevel=0.003)
    return b


def _frustum_shell(b, r0, z0, r1, z1, outer, inner, segs=24, loc=(0, 0, 0)):
    """Lamp shade: a single cone band with separate outside / inside materials."""
    ox, oy, oz = loc

    def f_out(u, v):
        a = 2 * math.pi * u
        r = r0 + (r1 - r0) * v
        return (ox + r * math.cos(a), oy + r * math.sin(a), oz + z0 + (z1 - z0) * v)

    def f_in(u, v):
        a = -2 * math.pi * u
        r = r0 + (r1 - r0) * v - 0.002
        return (ox + r * math.cos(a), oy + r * math.sin(a), oz + z0 + (z1 - z0) * v)
    _sheet(b, f_out, segs, 2, outer, size=(2 * math.pi * r0, z1 - z0))
    _sheet(b, f_in, segs, 2, inner, size=(2 * math.pi * r0, z1 - z0))


@wprop('SM_Lamp_Table', 'Props/House', 'none', 'house', size=(0.32, 0.32, 0.55))
def lamp_table():
    """Bulb at (0, 0, 0.36)."""
    b = Builder('SM_Lamp_Table')
    b.lathe([(0, 0), (0.07, 0), (0.072, 0.012), (0.06, 0.02), (0.075, 0.07), (0.085, 0.12), (0.075, 0.17),
             (0.04, 0.21), (0.028, 0.23), (0.03, 0.245), (0, 0.245)], mat='MI_Ceramic', segs=24)
    b.torus(0.083, 0.003, (0, 0, 0.12), 'MI_Brass', segs=24, rsegs=4)
    b.cyl(0.032, 0.012, (0, 0, 0.25), 'MI_Brass', segs=14)
    b.cyl(0.006, 0.07, (0, 0, 0.29), 'MI_Brass', segs=8)
    b.cyl(0.016, 0.04, (0, 0, 0.33), 'MI_Brass', segs=12)
    b.sphere(0.03, (0, 0, 0.375), 'MI_Emit_Bulb', scale=(1, 1, 1.2), segs=12, rings=8)
    for sx in (-1, 1):
        b.tube([(sx * 0.016, 0, 0.32), (sx * 0.1, 0, 0.36), (sx * 0.01, 0, 0.535)], 0.0025, 'MI_Brass', segs=5,
               bend=0.04)
    b.lathe([(0, 0.53), (0.012, 0.53), (0.01, 0.545), (0.005, 0.55), (0, 0.551)], mat='MI_Brass', segs=10)
    _frustum_shell(b, 0.16, 0.3, 0.11, 0.53, 'MI_Fabric_Shade', 'MI_Emit_Shade')
    b.torus(0.16, 0.003, (0, 0, 0.3), 'MI_Fabric_Shade', segs=24, rsegs=4)
    b.torus(0.11, 0.003, (0, 0, 0.53), 'MI_Fabric_Shade', segs=24, rsegs=4)
    b.tube([(0.03, 0.06, 0.02), (0.05, 0.09, 0.0), (0.2, 0.15, 0.0)], 0.003, 'MI_Cable', segs=5, bend=0.03)
    return _recentre(b, z0=0.0)


@wprop('SM_Fireplace', 'Props/House', 'complex', 'house', kind='wall', size=(1.6, 0.5, 1.25))
def fireplace():
    """Wall prop: back on y = 0. Fire glow position: (0, -0.2, 0.2)."""
    b = Builder('SM_Fireplace')
    br = 'MI_Brick_Red'
    w, dd = 1.6, 0.36
    ow, oh = 0.78, 0.74
    zb = 0.1
    for sx in (-1, 1):
        x0, x1 = sx * ow / 2, sx * (w / 2 - 0.06)
        b.box_minmax((min(x0, x1), -dd, zb), (max(x0, x1), 0.0, 1.12), br, bevel=0.004)
    b.box_minmax((-ow / 2, -dd, zb + oh), (ow / 2, 0.0, 1.12), br, bevel=0.004)
    b.box((ow + 0.06, 0.012, 0.05), (0, -dd - 0.002, zb + oh + 0.02), 'MI_Metal_Black', bevel=0.002)
    # Firebox: splayed brick cheeks, iron fireback, grate, logs, ash.
    b.box_minmax((-ow / 2 + 0.02, -0.12, zb), (ow / 2 - 0.02, -0.06, zb + oh), 'MI_Metal_Black')
    for sx in (-1, 1):
        b.box((0.06, 0.32, oh), (sx * (ow / 2 - 0.07), -0.21, zb + oh / 2), br, rot=(0, 0, sx * 16))
    b.box_minmax((-ow / 2 + 0.02, -0.12, zb + oh - 0.08), (ow / 2 - 0.02, -0.04, zb + oh + 0.02), br)
    b.box_minmax((-ow / 2, -dd, zb - 0.01), (ow / 2, -0.06, zb + 0.005), 'MI_Brick_Red')
    _blob(b, 0.22, (0, -0.2, zb + 0.0), 'MI_Dirt', scale=(1.4, 0.55, 0.12), subdiv=2, amp=0.25, seed=3, zmin=zb)
    for k in range(5):
        b.box((0.012, 0.22, 0.012), (-0.16 + k * 0.08, -0.2, zb + 0.08), 'MI_Metal_Black')
    for sx in (-1, 1):
        b.box((0.36, 0.014, 0.014), (0, -0.2 + sx * 0.1, zb + 0.08), 'MI_Metal_Black')
        b.tube([(sx * 0.2, -0.3, zb), (sx * 0.2, -0.3, zb + 0.13), (sx * 0.2, -0.27, zb + 0.16)], 0.012,
               'MI_Metal_Black', segs=8, bend=0.03)
        b.sphere(0.02, (sx * 0.2, -0.27, zb + 0.17), 'MI_Brass', segs=10, rings=6)
        b.tube([(sx * 0.2, -0.3, zb + 0.06), (sx * 0.2, -0.12, zb + 0.06)], 0.01, 'MI_Metal_Black', segs=8)
    for (x0, y0, x1, y1, z) in ((-0.2, -0.25, 0.18, -0.16, 0.13), (-0.15, -0.14, 0.2, -0.22, 0.13),
                                (-0.1, -0.2, 0.12, -0.18, 0.2)):
        b.tube([(x0, y0, zb + z), (x1, y1, zb + z)], 0.045, 'MI_Bark', segs=10)
    # Hearth slab and mantel shelf on corbels.
    b.box_minmax((-w / 2 + 0.03, -0.5, 0.0), (w / 2 - 0.03, 0.0, zb), 'MI_Stone_Wall', bevel=0.008)
    b.box_minmax((-w / 2, -0.48, 1.17), (w / 2, 0.0, 1.25), 'MI_Wood_Dark', bevel=0.008)
    b.box_minmax((-w / 2 + 0.03, -dd - 0.03, 1.12), (w / 2 - 0.03, 0.0, 1.17), 'MI_Wood_Dark', bevel=0.005)
    for sx in (-1, 1):
        b.prism([(0, 0), (0.0, 0.12), (-0.08, 0.12), (-0.03, 0.06), (-0.03, 0.0)], 0.08,
                (sx * (w / 2 - 0.16), -dd, 1.0), 'MI_Wood_Dark', axis='X', bevel=0.004)
    return b


@wprop('SM_GrandfatherClock', 'Props/House', 'box', 'house', size=(0.5, 0.3, 2.05))
def grandfather_clock():
    """Clock face centre (0, -0.153, 1.62); pendulum pivot (0, -0.02, 1.38)."""
    b = Builder('SM_GrandfatherClock')
    m = 'MI_Wood_Dark'
    w, d = 0.5, 0.3
    # Base with bracket feet.
    for sx in (-1, 1):
        b.prism([(-0.05, 0), (0.05, 0), (0.05, 0.06), (-0.05, 0.06)], d, (sx * 0.2, 0, 0), m, axis='Y', bevel=0.006)
    b.box((w, d, 0.06), (0, 0, 0.09), m, bevel=0.008)
    b.box((w - 0.04, d - 0.03, 0.26), (0, 0, 0.12 + 0.13), m, bevel=0.005)
    _panel_door(b, 0, 0.25, 0.3, 0.18, -(d - 0.03) / 2 - 0.006, m, t=0.008, frame=0.035)
    b.box((w, d, 0.05), (0, 0, 0.405), m, bevel=0.012)
    # Trunk: hollow case with a glazed door showing the pendulum and weights.
    tw, td = 0.38, 0.24
    z0, z1 = 0.43, 1.35
    b.box((tw, 0.015, z1 - z0), (0, td / 2 - 0.0075, (z0 + z1) / 2), m)
    for sx in (-1, 1):
        b.box((0.018, td, z1 - z0), (sx * (tw / 2 - 0.009), 0, (z0 + z1) / 2), m, bevel=0.003)
        b.box((0.05, 0.025, z1 - z0), (sx * (tw / 2 - 0.025), -td / 2 + 0.012, (z0 + z1) / 2), m, bevel=0.004)
    for z in (z0 + 0.03, z1 - 0.03):
        b.box((tw, 0.025, 0.06), (0, -td / 2 + 0.012, z), m, bevel=0.004)
    b.box((tw - 0.1, 0.003, z1 - z0 - 0.12), (0, -td / 2 + 0.005, (z0 + z1) / 2), 'MI_Glass')
    b.box((0.012, 0.012, 0.035), (tw / 2 - 0.05, -td / 2 - 0.006, 0.95), 'MI_Brass', bevel=0.002)
    b.box((0.008, 0.004, 0.62), (0, -0.02, 1.38 - 0.31), 'MI_Brass')
    b.cyl(0.075, 0.012, (0, -0.02, 0.76), 'MI_Brass', rot=(90, 0, 0), segs=24)
    b.torus(0.075, 0.004, (0, -0.027, 0.76), 'MI_Brass', rot=(90, 0, 0), segs=24, rsegs=4)
    for x, zl in ((-0.1, 0.98), (0.0, 1.08), (0.1, 0.94)):
        b.cyl(0.003, 1.33 - zl - 0.2, (x, 0.04, (1.33 + zl + 0.2) / 2), 'MI_Steel', segs=4)
        b.lathe([(0, 0), (0.028, 0), (0.03, 0.01), (0.03, 0.19), (0.02, 0.2), (0, 0.205)], (x, 0.04, zl), 'MI_Brass',
                segs=14)
    b.box((tw + 0.04, d - 0.02, 0.04), (0, 0, z1 + 0.02), m, bevel=0.008)
    # Hood with columns, dial and arched pediment.
    hz0, hz1 = 1.39, 1.86
    b.box((0.44, d - 0.04, hz1 - hz0), (0, 0.01, (hz0 + hz1) / 2), m, bevel=0.005)
    for sx in (-1, 1):
        b.cyl(0.018, hz1 - hz0 - 0.04, (sx * 0.2, -d / 2 + 0.02, (hz0 + hz1) / 2), m, segs=10)
        for z in (hz0 + 0.02, hz1 - 0.02):
            b.box((0.045, 0.045, 0.025), (sx * 0.2, -d / 2 + 0.02, z), 'MI_Brass', bevel=0.003)
    b.box((0.33, 0.02, 0.4), (0, -d / 2 + 0.01, 1.62), m, bevel=0.004)
    b.box((0.29, 0.004, 0.36), (0, -d / 2 - 0.001, 1.62), 'MI_Brass', bevel=0.001)
    _disc(b, 0.13, 0.13, (0, -d / 2 - 0.0035, 1.6), 'MI_Img_ClockFace', segs=32)
    b.torus(0.133, 0.006, (0, -d / 2 - 0.004, 1.6), 'MI_Brass', rot=(90, 0, 0), segs=32, rsegs=6)
    b.cyl(0.13, 0.002, (0, -d / 2 - 0.009, 1.6), 'MI_Glass', rot=(90, 0, 0), segs=32)
    for sx in (-1, 1):
        _knob(b, (sx * 0.1, -d / 2 - 0.002, 1.76), 'MI_Brass', r=0.006)
    b.box((w, d, 0.04), (0, 0, hz1 + 0.02), m, bevel=0.01)
    ped = [(-0.24, 0.0), (0.24, 0.0), (0.24, 0.03), (0.08, 0.1), (0.04, 0.13), (-0.04, 0.13), (-0.08, 0.1), (-0.24, 0.03)]
    b.prism(ped, 0.05, (0, -d / 2 + 0.03, hz1 + 0.04), m, axis='Y', bevel=0.006)
    for x, s in ((-0.22, 0.8), (0.0, 1.0), (0.22, 0.8)):
        z = hz1 + 0.04 + (0.13 if x == 0 else 0.03)
        b.lathe([(0, 0), (0.012 * s, 0), (0.016 * s, 0.015), (0.008 * s, 0.03), (0.003, 0.045 * s), (0, 0.05 * s)],
                (x, -d / 2 + 0.03, z), 'MI_Brass', segs=10)
    return b


@wprop('SM_Piano_Upright', 'Props/House', 'box', 'house', size=(1.5, 0.6, 1.25))
def piano_upright():
    b = Builder('SM_Piano_Upright')
    m = 'MI_Wood_Dark'
    w = 1.5
    yb = 0.3
    # Case.
    b.box_minmax((-w / 2 + 0.05, -0.02, 0.06), (w / 2 - 0.05, yb, 1.22), m, bevel=0.004)
    b.box_minmax((-w / 2 - 0.0, -0.05, 1.21), (w / 2, yb + 0.0, 1.25), m, bevel=0.008)
    for sx in (-1, 1):
        b.box_minmax((min(sx * (w / 2 - 0.06), sx * w / 2), -0.06, 0.0), (max(sx * (w / 2 - 0.06), sx * w / 2), yb, 1.21),
                     m, bevel=0.006)
        b.box_minmax((min(sx * (w / 2 - 0.07), sx * w / 2), -0.3, 0.64), (max(sx * (w / 2 - 0.07), sx * w / 2), -0.06, 0.8),
                     m, bevel=0.008)
        b.lathe([(0, 0), (0.03, 0), (0.035, 0.05), (0.022, 0.2), (0.03, 0.55), (0.04, 0.6), (0, 0.6)],
                (sx * (w / 2 - 0.035), -0.25, 0.03), m, segs=10)
        b.box((0.06, 0.28, 0.035), (sx * (w / 2 - 0.035), -0.16, 0.0175), m, bevel=0.005)
    # Lower panel, key bed, keys.
    _panel_door(b, 0, 0.36, w - 0.14, 0.52, -0.035, m, t=0.015, frame=0.07)
    b.box_minmax((-w / 2 + 0.07, -0.3, 0.64), (w / 2 - 0.07, -0.02, 0.69), m, bevel=0.004)
    kx0, kx1 = -w / 2 + 0.08, w / 2 - 0.08
    n_white = 52
    kw = (kx1 - kx0) / n_white
    for i in range(n_white):
        x = kx0 + (i + 0.5) * kw
        b.box((kw - 0.0012, 0.15, 0.022), (x, -0.205, 0.701), 'MI_Ceramic', bevel=0.0008, segs=1)
        note = (i + 5) % 7     # the keyboard starts on A
        if note in (0, 1, 3, 4, 5) and i < n_white - 1:
            b.box((kw * 0.55, 0.095, 0.014), (x + kw / 2, -0.17, 0.718), 'MI_Plastic_Black', bevel=0.0015, segs=1)
    b.box_minmax((kx0 - 0.005, -0.3, 0.69), (kx1 + 0.005, -0.28, 0.715), m, bevel=0.003)
    b.box_minmax((kx0, -0.12, 0.69), (kx1, -0.04, 0.73), m, bevel=0.003)
    # Fallboard (open, folded back), music desk, sheet music, candle sconces.
    b.box_minmax((-w / 2 + 0.07, -0.08, 0.73), (w / 2 - 0.07, -0.04, 0.84), m, bevel=0.004)
    b.box((0.7, 0.02, 0.24), (0, -0.07, 0.95), m, rot=(-12, 0, 0), bevel=0.004)
    b.box((0.72, 0.05, 0.02), (0, -0.09, 0.835), m, bevel=0.004)
    b.plane(0.21, 0.29, (-0.12, -0.085, 0.98), 'MI_Img_Notes', rot=(-12, 0, -3), uv_rect=(0.505, 0.51, 0.745, 0.99))
    b.plane(0.21, 0.29, (0.11, -0.085, 0.98), 'MI_Paper', rot=(-12, 0, 2))
    _panel_door(b, 0, 1.03, w - 0.14, 0.32, -0.02, m, t=0.012, frame=0.06)
    for sx in (-1, 1):
        x = sx * 0.58
        b.prism(_ellipse(0.03, 0.05, 14), 0.008, (x, -0.024, 1.0), 'MI_Brass', axis='Y')
        b.tube([(x, -0.03, 1.0), (x + sx * 0.03, -0.12, 0.98), (x + sx * 0.06, -0.15, 1.02)], 0.006, 'MI_Brass', segs=6,
               bend=0.04)
        b.lathe([(0, 0), (0.03, 0.0), (0.035, 0.006), (0.012, 0.012), (0.012, 0.03), (0, 0.03)],
                (x + sx * 0.06, -0.15, 1.02), 'MI_Brass', segs=12)
        b.cyl(0.01, 0.09, (x + sx * 0.06, -0.15, 1.095), 'MI_Wax', segs=10)
    # Pedals.
    for k in (-1, 0, 1):
        b.box((0.03, 0.09, 0.012), (k * 0.07, -0.08, 0.08), 'MI_Brass', rot=(-6, 0, 0), bevel=0.004)
    b.box((0.3, 0.03, 0.06), (0, -0.035, 0.08), m, bevel=0.004)
    return _recentre(b)


@wprop('SM_RockingChair', 'Props/House', 'box', 'house', size=(0.6, 0.9, 1.05))
def rocking_chair():
    b = Builder('SM_RockingChair')
    m = 'MI_Wood_Dark'
    R = 1.1
    for sx in (-1, 1):
        x = sx * 0.26
        outer, inner = [], []
        for k in range(17):
            yy = -0.45 + 0.9 * k / 16
            zz = R - math.sqrt(R * R - yy * yy)
            outer.append((yy, zz))
            inner.append((yy, zz + 0.045))
        poly = outer + list(reversed(inner))
        b.prism(poly, 0.028, (x, 0, 0), m, axis='X', bevel=0.004)
    seat_z = 0.43
    for sx in (-1, 1):
        x = sx * 0.24
        b.cyl(0.018, seat_z - 0.04, (x, -0.17, 0.04 + (seat_z - 0.04) / 2), m, segs=10)
        b.cyl(0.018, seat_z + 0.0, (x, 0.18, 0.045 + seat_z / 2), m, segs=10)
        b.tube([(x, 0.18, 0.06), (x, 0.24, 0.6), (x, 0.31, 1.03)], 0.02, m, segs=10, bend=0.2)
        b.lathe([(0, 0), (0.024, 0.0), (0.02, 0.025), (0, 0.04)], (x, 0.312, 1.025), m, segs=10)
        b.cyl(0.017, 0.24, (x, -0.17, seat_z + 0.12), m, segs=10)
        b.box((0.06, 0.48, 0.025), (x + sx * 0.01, 0.02, seat_z + 0.25), m, rot=(4, 0, 0), bevel=0.008)
        b.cyl(0.012, 0.36, (x, 0.0, 0.17), m, rot=(90, 0, 0), segs=8)
    b.cyl(0.012, 0.47, (0, -0.17, 0.2), m, rot=(0, 90, 0), segs=8)
    b.cyl(0.012, 0.47, (0, 0.19, 0.2), m, rot=(0, 90, 0), segs=8)
    b.box((0.5, 0.44, 0.03), (0, 0.0, seat_z), m, rot=(-4, 0, 0), bevel=0.006)
    b.box((0.44, 0.38, 0.05), (0, -0.01, seat_z + 0.035), 'MI_Fabric_Sofa', rot=(-4, 0, 0), bevel=0.02, segs=2)
    b.box((0.5, 0.05, 0.1), (0, 0.305, 0.97), m, rot=(-14, 0, 0), bevel=0.008)
    for k in range(7):
        x = -0.18 + k * 0.06
        b.tube([(x, 0.2, seat_z + 0.03), (x, 0.25, 0.65), (x, 0.295, 0.93)], 0.008, m, segs=6, bend=0.15)
    return _recentre(b)


@wprop('SM_DiningTable', 'Props/House', 'box', 'house', size=(1.6, 0.9, 0.76))
def dining_table():
    b = Builder('SM_DiningTable')
    m = 'MI_Cabinet_Wood'
    w, d, h = 1.6, 0.9, 0.76
    for i in range(5):
        b.box((w, d / 5 - 0.003, 0.035), (0, -d / 2 + d / 10 + i * d / 5, h - 0.0175), 'MI_Wood_Raw', bevel=0.004,
              uv_offset=(i * 0.31, i * 0.17), uv_rotate=False)
    b.box((w - 0.06, d - 0.06, 0.012), (0, 0, h - 0.04), m)
    for sx in (-1, 1):
        for sy in (-1, 1):
            _turned_leg(b, sx * (w / 2 - 0.08), sy * (d / 2 - 0.08), 0.0, h - 0.035, 0.032, m)
    for sy in (-1, 1):
        b.box((w - 0.2, 0.022, 0.09), (0, sy * (d / 2 - 0.08), h - 0.08), m, bevel=0.003)
    for sx in (-1, 1):
        b.box((0.022, d - 0.2, 0.09), (sx * (w / 2 - 0.08), 0, h - 0.08), m, bevel=0.003)
    return b


@wprop('SM_DiningChair', 'Props/House', 'box', 'house', size=(0.45, 0.5, 0.95))
def dining_chair():
    b = Builder('SM_DiningChair')
    m = 'MI_Cabinet_Wood'
    s, zs = 0.44, 0.45
    for sx in (-1, 1):
        _turned_leg(b, sx * (s / 2 - 0.03), -s / 2 + 0.03 - 0.02, 0.0, zs - 0.02, 0.018, m)
        b.tube([(sx * (s / 2 - 0.03), s / 2 - 0.05, 0.0), (sx * (s / 2 - 0.03), s / 2 - 0.04, zs),
                (sx * (s / 2 - 0.035), s / 2 + 0.01, 0.94)], 0.017, m, segs=8, bend=0.1)
        b.lathe([(0, 0), (0.02, 0), (0.016, 0.02), (0, 0.03)], (sx * (s / 2 - 0.035), s / 2 + 0.011, 0.935), m, segs=8)
        b.cyl(0.01, s - 0.08, (sx * (s / 2 - 0.03), -0.0, 0.16), m, rot=(90, 0, 0), segs=8)
    b.cyl(0.01, s - 0.06, (0, 0.0, 0.2), m, rot=(0, 90, 0), segs=8)
    b.box((s, s, 0.03), (0, -0.02, zs - 0.015), m, bevel=0.006)
    b.box((s - 0.05, s - 0.06, 0.04), (0, -0.025, zs + 0.02), 'MI_Fabric_Sofa', bevel=0.016, segs=2)
    b.box((s - 0.04, 0.03, 0.09), (0, s / 2 + 0.002, 0.86), m, rot=(-7, 0, 0), bevel=0.006)
    b.box((s - 0.06, 0.02, 0.04), (0, s / 2 - 0.02, zs + 0.06), m, rot=(-7, 0, 0), bevel=0.004)
    for k in range(4):
        x = -0.105 + k * 0.07
        b.tube([(x, s / 2 - 0.02, zs + 0.08), (x, s / 2 - 0.005, 0.82)], 0.008, m, segs=6)
    return _recentre(b)


def _kitchen_base(name, sink=False):
    """1.0 m base cabinet module: tiles along X (x = -0.5 .. 0.5), countertop overhangs the doors."""
    b = Builder(name)
    m = 'MI_Cabinet_White'
    w, h = 1.0, 0.92
    yf = -0.27                          # carcass front
    b.box_minmax((-w / 2, yf, 0.1), (w / 2, 0.31, h - 0.04), m)
    b.box_minmax((-w / 2 + 0.005, -0.21, 0.0), (w / 2 - 0.005, 0.3, 0.1), 'MI_Trim_Dark')
    # Countertop with a rounded front edge and a short upstand at the back.
    top = h - 0.04
    if sink:
        sw, sd, sy = 0.62, 0.4, -0.03
        b.box_minmax((-w / 2, -0.31, top), (-sw / 2, 0.31, h), 'MI_Countertop', bevel=0.004)
        b.box_minmax((sw / 2, -0.31, top), (w / 2, 0.31, h), 'MI_Countertop', bevel=0.004)
        b.box_minmax((-sw / 2, -0.31, top), (sw / 2, sy - sd / 2, h), 'MI_Countertop', bevel=0.004)
        b.box_minmax((-sw / 2, sy + sd / 2, top), (sw / 2, 0.31, h), 'MI_Countertop', bevel=0.004)
        # Double steel bowl: flange + walls + floors + drains.
        fl = Builder('flange')
        _frame_ring(fl, sw + 0.02, sd + 0.02, sw - 0.01, sd - 0.01, -0.002, 0.002, 'MI_Steel', bevel=0.001)
        _attach(b, fl, (0, sy, h), (-90, 0, 0))
        for bx in (-0.155, 0.155):
            bw, bd, bh = 0.29, sd - 0.02, 0.18
            zt = h - 0.002
            for sxx in (-1, 1):
                b.box((0.004, bd, bh), (bx + sxx * bw / 2, sy, zt - bh / 2), 'MI_Steel')
            for syy in (-1, 1):
                b.box((bw, 0.004, bh), (bx, sy + syy * bd / 2, zt - bh / 2), 'MI_Steel')
            b.box((bw, bd, 0.004), (bx, sy, zt - bh), 'MI_Steel')
            b.cyl(0.03, 0.006, (bx, sy + 0.05, zt - bh + 0.003), 'MI_Chrome', segs=14)
            b.cyl(0.02, 0.007, (bx, sy + 0.05, zt - bh + 0.003), 'MI_Metal_Black', segs=12)
        b.box((0.02, sd, 0.004), (0, sy, h - 0.001), 'MI_Steel')
        # Faucet with two cross handles.
        fy = 0.22
        b.cyl(0.03, 0.03, (0, fy, h + 0.015), 'MI_Chrome', segs=14)
        b.tube([(0, fy, h + 0.02), (0, fy, h + 0.3), (0, fy - 0.08, h + 0.34), (0, fy - 0.17, h + 0.3),
                (0, fy - 0.19, h + 0.24)], 0.011, 'MI_Chrome', segs=10, bend=0.06)
        for sxx in (-1, 1):
            b.cyl(0.02, 0.03, (sxx * 0.1, fy, h + 0.015), 'MI_Chrome', segs=12)
            for k in range(2):
                b.box((0.06, 0.008, 0.008), (sxx * 0.1, fy, h + 0.034), 'MI_Chrome', rot=(0, 0, 45 + k * 90),
                      bevel=0.003)
            b.sphere(0.008, (sxx * 0.1, fy, h + 0.036), 'MI_Plastic_Red' if sxx < 0 else 'MI_Metal_Blue', segs=8, rings=5)
    else:
        b.box_minmax((-w / 2, -0.31, top), (w / 2, 0.31, h), 'MI_Countertop', bevel=0.004)
    b.box_minmax((-w / 2, 0.29, h), (w / 2, 0.31, h + 0.04), 'MI_Countertop', bevel=0.003)
    b.box((w, 0.012, 0.03), (0, -0.305, top + 0.005), 'MI_Countertop', bevel=0.005)
    # Drawer row (or a false front over the sink) and two doors.
    fz = top - 0.01
    if sink:
        _panel_door(b, 0, fz - 0.09, w - 0.012, 0.16, yf - 0.02, m, frame=0.045)
        b.box((0.002, 0.012, 0.12), (0, yf - 0.02, fz - 0.09), 'MI_Trim_Dark')
    else:
        for sx in (-1, 1):
            _panel_door(b, sx * w / 4, fz - 0.09, w / 2 - 0.008, 0.16, yf - 0.02, m, frame=0.045)
            _bar_handle(b, sx * w / 4, fz - 0.085, 0.13, yf - 0.02, 'MI_Chrome', r=0.005, stand=0.025)
    dz0, dz1 = 0.105, fz - 0.18
    for sx in (-1, 1):
        _panel_door(b, sx * w / 4, (dz0 + dz1) / 2, w / 2 - 0.008, dz1 - dz0, yf - 0.02, m, frame=0.06)
        _bar_handle(b, sx * 0.04, dz1 - 0.08, 0.12, yf - 0.02, 'MI_Chrome', vertical=True, r=0.005, stand=0.025)
    return b


@wprop('SM_Kitchen_Base', 'Props/House', 'box', 'house', size=(1.0, 0.62, 0.92))
def kitchen_base():
    return _kitchen_base('SM_Kitchen_Base')


@wprop('SM_Kitchen_Base_Sink', 'Props/House', 'complex', 'house', size=(1.0, 0.62, 0.92))
def kitchen_base_sink():
    return _kitchen_base('SM_Kitchen_Base_Sink', sink=True)


@wprop('SM_Kitchen_Stove', 'Props/House', 'box', 'house', size=(0.76, 0.65, 0.92))
def kitchen_stove():
    b = Builder('SM_Kitchen_Stove')
    w, d, h = 0.76, 0.65, 0.92
    m = 'MI_Porcelain'
    yf = -d / 2 + 0.03
    b.box_minmax((-w / 2, yf, 0.06), (w / 2, d / 2, h), m, bevel=0.006)
    b.box_minmax((-w / 2 + 0.03, yf + 0.02, 0.0), (w / 2 - 0.03, d / 2 - 0.02, 0.06), 'MI_Metal_Black')
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.cyl(0.015, 0.03, (sx * (w / 2 - 0.05), sy * 0.22, 0.015), 'MI_Metal_Black', segs=8)
    # Cooktop with four coil burners on chrome drip pans.
    b.box_minmax((-w / 2 + 0.005, yf - 0.03, h - 0.012), (w / 2 - 0.005, d / 2 - 0.12, h), m, bevel=0.004)
    for bx, by, r in ((-0.18, -0.13, 0.1), (0.18, -0.13, 0.08), (-0.18, 0.1, 0.08), (0.18, 0.1, 0.1)):
        b.lathe([(0, -0.012), (r * 0.3, -0.012), (r + 0.015, 0.0), (r + 0.02, 0.002), (0, 0.002)], (bx, by, h - 0.004),
                'MI_Chrome', segs=20)
        for k in range(3):
            b.torus(r * (0.35 + k * 0.27), 0.0045, (bx, by, h + 0.004), 'MI_Metal_Black', segs=20, rsegs=4)
        b.box((r * 2 * 0.95, 0.006, 0.006), (bx, by, h + 0.004), 'MI_Metal_Black')
    # Backguard with knobs and a clock.
    b.box_minmax((-w / 2, d / 2 - 0.12, h), (w / 2, d / 2, h + 0.2), m, bevel=0.006)
    b.box_minmax((-w / 2 + 0.02, d / 2 - 0.125, h + 0.03), (w / 2 - 0.02, d / 2 - 0.12, h + 0.17), 'MI_Metal_Cream')
    for k, x in enumerate((-0.3, -0.22, 0.22, 0.3)):
        b.cyl(0.022, 0.025, (x, d / 2 - 0.135, h + 0.1), 'MI_Plastic_Black', rot=(90, 0, 0), segs=14)
        b.box((0.006, 0.006, 0.03), (x, d / 2 - 0.15, h + 0.1), 'MI_Chrome')
    _disc(b, 0.045, 0.045, (0, d / 2 - 0.1255, h + 0.1), 'MI_Img_ClockFace', segs=24)
    b.torus(0.047, 0.004, (0, d / 2 - 0.127, h + 0.1), 'MI_Chrome', rot=(90, 0, 0), segs=24, rsegs=4)
    # Oven door with dark glass, handle bar; storage drawer below.
    b.box_minmax((-w / 2 + 0.01, yf - 0.035, 0.22), (w / 2 - 0.01, yf, h - 0.06), m, bevel=0.008)
    b.box_minmax((-0.25, yf - 0.037, 0.4), (0.25, yf - 0.03, 0.7), 'MI_Glass_Van')
    _frame_ring(b, 0.52, 0.32, 0.5, 0.3, -0.04, -0.03, 'MI_Chrome', cz=0.55)
    _bar_handle(b, 0, 0.8, w - 0.12, yf - 0.035, 'MI_Chrome', r=0.009, stand=0.045)
    b.box_minmax((-w / 2 + 0.01, yf - 0.03, 0.07), (w / 2 - 0.01, yf, 0.2), m, bevel=0.006)
    _bar_handle(b, 0, 0.17, 0.3, yf - 0.03, 'MI_Chrome', r=0.006, stand=0.025)
    return b


@wprop('SM_Fridge', 'Props/House', 'box', 'house', size=(0.75, 0.72, 1.8))
def fridge():
    b = Builder('SM_Fridge')
    w, d, h = 0.75, 0.72, 1.8
    m = 'MI_Metal_Cream'
    yf = -d / 2 + 0.05
    b.box_minmax((-w / 2, yf, 0.08), (w / 2, d / 2, h), m, bevel=0.03)
    b.box_minmax((-w / 2 + 0.02, yf + 0.01, 0.0), (w / 2 - 0.02, d / 2 - 0.05, 0.08), 'MI_Plastic_Black')
    for k in range(6):
        b.box((w - 0.12, 0.006, 0.008), (0, yf - 0.002, 0.018 + k * 0.01), 'MI_Metal_Black')
    b.box_minmax((-w / 2 + 0.03, yf - 0.012, 0.012), (w / 2 - 0.03, yf, 0.075), 'MI_Plastic_Black', bevel=0.003)
    # Doors (freezer above), gaskets, long handles.
    for z0, z1 in ((0.085, 1.22), (1.235, h - 0.01)):
        b.box_minmax((-w / 2 + 0.004, yf - 0.05, z0 + 0.012), (w / 2 - 0.004, yf - 0.012, z1), m, bevel=0.025)
        b.box_minmax((-w / 2 + 0.012, yf - 0.014, z0 + 0.016), (w / 2 - 0.012, yf, z1 - 0.004), 'MI_Rubber_Black',
                     bevel=0.006)
    b.tube([(w / 2 - 0.06, yf - 0.05, 0.75), (w / 2 - 0.06, yf - 0.09, 0.78), (w / 2 - 0.06, yf - 0.09, 1.12),
            (w / 2 - 0.06, yf - 0.05, 1.16)], 0.014, 'MI_Plastic_Grey', segs=10, bend=0.03)
    b.tube([(w / 2 - 0.06, yf - 0.05, 1.27), (w / 2 - 0.06, yf - 0.09, 1.3), (w / 2 - 0.06, yf - 0.09, 1.48),
            (w / 2 - 0.06, yf - 0.05, 1.51)], 0.014, 'MI_Plastic_Grey', segs=10, bend=0.03)
    # Fridge magnets holding a family photo and a note.
    yd = yf - 0.0505
    b.plane(0.15, 0.1, (-0.12, yd, 1.05), 'MI_Img_Photo_Family', rot=(0, 3, 0))
    b.plane(0.1, 0.1, (0.08, yd, 1.12), 'MI_Img_Notes', rot=(0, -6, 0), uv_rect=(0.254, 0.508, 0.496, 0.992))
    b.plane(0.18, 0.24, (-0.05, yd, 0.75), 'MI_Img_Notes', rot=(0, 2, 0), uv_rect=(0.505, 0.51, 0.745, 0.99))
    for (x, z, mm) in ((-0.12, 1.095, 'MI_Plastic_Red'), (0.08, 1.165, 'MI_Plastic_Yellow'), (-0.05, 0.86, 'MI_Metal_Blue'),
                       (0.2, 1.4, 'MI_Metal_Green'), (-0.2, 0.55, 'MI_Plastic_Red')):
        b.cyl(0.013, 0.008, (x, yd - 0.004, z), mm, rot=(90, 0, 0), segs=12)
    return b


@wprop('SM_Kitchen_Wall', 'Props/House', 'box', 'house', kind='wall', size=(1.0, 0.35, 0.7))
def kitchen_wall():
    b = Builder('SM_Kitchen_Wall')
    m = 'MI_Cabinet_White'
    w, d, h = 1.0, 0.35, 0.7
    b.box_minmax((-w / 2, -d + 0.02, 0.0), (w / 2, 0.0, h - 0.03), m)
    b.box_minmax((-w / 2, -d + 0.005, h - 0.035), (w / 2, 0.0, h), m, bevel=0.004)
    b.box_minmax((-w / 2, -d - 0.0, h - 0.06), (w / 2, -d + 0.04, h - 0.03), m, bevel=0.006)
    for sx in (-1, 1):
        _panel_door(b, sx * w / 4, (h - 0.06) / 2 + 0.003, w / 2 - 0.008, h - 0.075, -d + 0.02 - 0.0, m, frame=0.06)
        _bar_handle(b, sx * 0.04, 0.1, 0.12, -d + 0.02, 'MI_Chrome', vertical=True, r=0.005, stand=0.025)
    return b


@wprop('SM_Microwave', 'Props/House', 'box', 'house', size=(0.5, 0.38, 0.3))
def microwave():
    b = Builder('SM_Microwave')
    w, d, h = 0.5, 0.36, 0.29
    b.box_minmax((-w / 2, -d / 2, 0.01), (w / 2, d / 2, h + 0.01), 'MI_Plastic_White', bevel=0.01)
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.cyl(0.012, 0.01, (sx * (w / 2 - 0.04), sy * (d / 2 - 0.04), 0.005), 'MI_Rubber_Black', segs=8)
    b.box_minmax((-w / 2 + 0.008, -d / 2 - 0.018, 0.02), (w / 2 - 0.13, -d / 2 + 0.002, h), 'MI_Plastic_White',
                 bevel=0.006)
    b.box_minmax((-w / 2 + 0.04, -d / 2 - 0.02, 0.06), (w / 2 - 0.18, -d / 2 - 0.016, h - 0.04), 'MI_Glass_Van')
    b.box_minmax((w / 2 - 0.16, -d / 2 - 0.021, 0.035), (w / 2 - 0.145, -d / 2 - 0.017, h - 0.015), 'MI_Plastic_Grey',
                 bevel=0.003)
    b.box_minmax((w / 2 - 0.12, -d / 2 - 0.006, 0.02), (w / 2 - 0.01, -d / 2 + 0.002, h), 'MI_Plastic_Grey', bevel=0.004)
    b.box((0.08, 0.004, 0.025), (w / 2 - 0.065, -d / 2 - 0.007, h - 0.035), 'MI_Screen_Black')
    b.box((0.05, 0.002, 0.012), (w / 2 - 0.065, -d / 2 - 0.009, h - 0.035), 'MI_Emit_Green')
    for r in range(4):
        for c in range(3):
            b.box((0.024, 0.005, 0.016), (w / 2 - 0.095 + c * 0.03, -d / 2 - 0.008, h - 0.08 - r * 0.026),
                  'MI_Plastic_White', bevel=0.002)
    b.cyl(0.02, 0.016, (w / 2 - 0.065, -d / 2 - 0.012, 0.07), 'MI_Plastic_White', rot=(90, 0, 0), segs=14)
    return _recentre(b, z0=0.0)


@wprop('SM_Plate_Stack', 'Props/House', 'none', 'house', size=(0.27, 0.27, 0.07))
def plate_stack():
    b = Builder('SM_Plate_Stack')
    prof = [(0, 0), (0.06, 0.0), (0.065, 0.006), (0.1, 0.009), (0.13, 0.018), (0.133, 0.021), (0.128, 0.022),
            (0.098, 0.013), (0.062, 0.011), (0, 0.011)]
    for k in range(6):
        b.lathe(prof, (0.002 * math.sin(k * 2.1), 0.002 * math.cos(k * 1.7), k * 0.011), 'MI_Porcelain', segs=22)
        b.torus(0.118, 0.0012, (0.002 * math.sin(k * 2.1), 0.002 * math.cos(k * 1.7), k * 0.011 + 0.0185),
                'MI_Car_Paint_Blue', segs=22, rsegs=3, scale=(1, 1, 0.6))
    return _recentre(b)


@wprop('SM_Pot_Cooking', 'Props/House', 'none', 'house', size=(0.36, 0.25, 0.2))
def pot_cooking():
    b = Builder('SM_Pot_Cooking')
    b.lathe([(0, 0), (0.11, 0.0), (0.12, 0.006), (0.12, 0.15), (0.124, 0.152), (0.124, 0.156), (0.116, 0.156),
             (0.116, 0.008), (0, 0.008)], mat='MI_Steel', segs=28)
    b.lathe([(0, 0.18), (0.03, 0.178), (0.1, 0.165), (0.122, 0.157), (0.122, 0.152), (0.0, 0.16)], mat='MI_Steel',
            segs=28)
    b.lathe([(0, 0.0), (0.016, 0.0), (0.02, 0.012), (0.014, 0.022), (0, 0.024)], (0, 0, 0.176), 'MI_Plastic_Black',
            segs=12)
    for sx in (-1, 1):
        b.box((0.05, 0.03, 0.016), (sx * 0.145, 0, 0.13), 'MI_Plastic_Black', bevel=0.006)
        b.box((0.02, 0.02, 0.012), (sx * 0.125, 0, 0.13), 'MI_Steel', bevel=0.002)
    return b


def _drape(b, hx, y0, y1, top, mats, r=0.05, drop=0.25, foot=True, nx=24, ny=26, thickness=0.02, folds=0.012,
           cell=None, seed=0):
    """Bedding draped over a mattress whose top is at 'top' and half-width hx (centred on x = 0),
    running from y0 (foot, -Y) to y1 (head). The cloth rolls over the edges with radius r and hangs
    'drop' further; with 'cell' the faces alternate between the given materials (patchwork)."""
    flat_w = hx - r
    over = math.pi * r / 2 + drop
    span_x = flat_w + over
    span_y_foot = over if foot else 0.0

    def fold(d):
        if d <= 0:
            return d, 0.0
        if d < math.pi * r / 2:
            t = d / r
            return r * math.sin(t), r * (1 - math.cos(t))
        return r, r + (d - math.pi * r / 2)

    def fn(u, v):
        X = -span_x + 2 * span_x * u
        Y = (y0 + r) - span_y_foot + ((y1) - (y0 + r) + span_y_foot) * v
        dx = abs(X) - flat_w
        ox, oz1 = fold(dx)
        x = math.copysign(flat_w + ox, X) if dx > 0 else X
        dy = (y0 + r) - Y if foot else 0.0
        oy, oz2 = fold(dy)
        y = (y0 + r - oy) if dy > 0 else Y
        z = top - oz1 - oz2
        hang = max(oz1, oz2) / (r + drop)
        w = folds * (math.sin(Y * 23.0 + seed) + 0.6 * math.sin(Y * 9.0 + X * 4 + seed)) * hang
        w2 = folds * math.sin(X * 21.0 + seed * 2) * hang
        if dx > 0:
            x += math.copysign(w, X)
        if dy > 0:
            y -= w2
        z += 0.004 * math.sin(X * 7 + seed) * math.sin(Y * 5) * (1 - hang)
        return (x, y, z)

    pb, uv = b._begin()
    rows = [[pb.verts.new(Vector(fn(i / nx, j / ny))) for i in range(nx + 1)] for j in range(ny + 1)]
    slots = [b._slot(mm) for mm in mats]
    for j in range(ny):
        for i in range(nx):
            f = pb.faces.new((rows[j][i], rows[j][i + 1], rows[j + 1][i + 1], rows[j + 1][i]))
            for loop, (a, c) in zip(f.loops, ((i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1))):
                loop[uv].uv = (a / nx * 2 * span_x, c / ny * (y1 - y0 + span_y_foot))
            k = 0
            if cell:
                k = (int(i * 2 * span_x / nx / cell) * 3 + int(j * (y1 - y0) / ny / cell)) % len(mats)
            f.material_index = slots[k]
    if thickness:
        bmesh.ops.solidify(pb, geom=list(pb.faces), thickness=thickness)
        bmesh.ops.recalc_face_normals(pb, faces=pb.faces)
    pb.normal_update()
    _commit_multi(b, pb, [b.mats[f.material_index] for f in pb.faces])


def _pillow(b, w, d, h, loc, mat, rot=(0, 0, 0), seed=0):
    _blob(b, 1.0, loc, mat, scale=(w / 2, d / 2, h / 2), subdiv=3, amp=0.06, freq=1.2, seed=seed, rot=rot)


@wprop('SM_Bed_Double', 'Props/House', 'box', 'house', size=(1.6, 2.1, 1.05))
def bed_double():
    """Headboard at +Y, the foot of the bed faces -Y."""
    b = Builder('SM_Bed_Double')
    m = 'MI_Wood_Dark'
    w, L = 1.6, 2.1
    yh, yfoot = L / 2, -L / 2
    # Posts, head- and footboard.
    for sx in (-1, 1):
        x = sx * (w / 2 - 0.04)
        b.box((0.08, 0.08, 1.0), (x, yh - 0.04, 0.5), m, bevel=0.008)
        b.lathe([(0, 0), (0.045, 0), (0.05, 0.02), (0.03, 0.04), (0.035, 0.06), (0, 0.075)], (x, yh - 0.04, 0.98), m,
                segs=12)
        b.box((0.07, 0.07, 0.55), (x, yfoot + 0.035, 0.275), m, bevel=0.008)
        b.lathe([(0, 0), (0.04, 0), (0.045, 0.02), (0.025, 0.035), (0, 0.05)], (x, yfoot + 0.035, 0.55), m, segs=12)
        b.box((0.04, L - 0.15, 0.18), (x - sx * 0.005, 0.0, 0.26), m, bevel=0.005)
    hb = [(-w / 2 + 0.08, 0.0), (w / 2 - 0.08, 0.0)]
    for k in range(11):
        t = k / 10
        hb.append((w / 2 - 0.08 - (w - 0.16) * t, 0.5 + 0.12 * math.sin(math.pi * t)))
    b.prism(hb, 0.035, (0, yh - 0.04, 0.4), m, axis='Y', bevel=0.006)
    for k in range(4):
        x = -0.48 + k * 0.32
        b.box((0.24, 0.012, 0.32 + 0.08 * math.sin(math.pi * (k + 0.5) / 4)), (x, yh - 0.062, 0.62), m, bevel=0.006)
    b.box((w - 0.12, 0.03, 0.3), (0, yfoot + 0.035, 0.3), m, bevel=0.006)
    b.box((w - 0.12, 0.05, 0.04), (0, yfoot + 0.035, 0.47), m, bevel=0.008)
    # Mattress, sheet, duvet with a turned-down top, pillows.
    b.box((w - 0.12, L - 0.18, 0.06), (0, 0.0, 0.33), 'MI_Wood_Raw')
    b.box((w - 0.14, L - 0.2, 0.22), (0, 0.0, 0.47), 'MI_Fabric_Shade', bevel=0.04, segs=2)
    top = 0.58
    _drape(b, (w - 0.14) / 2 + 0.01, yfoot + 0.1, 0.42, top + 0.012, ['MI_Fabric_Bedding'], r=0.05, drop=0.17,
           thickness=0.025, seed=1)
    b.tube([(-(w - 0.14) / 2 - 0.02, 0.43, top + 0.03), ((w - 0.14) / 2 + 0.02, 0.43, top + 0.03)], 0.04,
           'MI_Fabric_Bedding', segs=10)
    for sx in (-1, 1):
        _pillow(b, 0.62, 0.42, 0.16, (sx * 0.34, 0.74, top + 0.07), 'MI_Fabric_Bedding', rot=(-14, 0, sx * 3), seed=sx)
    return b


@wprop('SM_Bed_Single', 'Props/House', 'box', 'house', size=(1.0, 2.0, 0.95))
def bed_single():
    """Kid's bed, painted spindle frame, patchwork quilt. Headboard at +Y."""
    b = Builder('SM_Bed_Single')
    m = 'MI_Trim_White'
    w, L = 1.0, 2.0
    yh, yfoot = L / 2, -L / 2
    for sy, hh in ((yh - 0.03, 0.95), (yfoot + 0.03, 0.62)):
        for sx in (-1, 1):
            x = sx * (w / 2 - 0.03)
            b.cyl(0.03, hh - 0.04, (x, sy, (hh - 0.04) / 2), m, segs=12)
            b.sphere(0.04, (x, sy, hh - 0.03), m, segs=12, rings=8)
        top = hh - 0.08
        b.box((w - 0.06, 0.04, 0.06), (0, sy, top), m, bevel=0.01)
        b.box((w - 0.06, 0.04, 0.05), (0, sy, 0.3), m, bevel=0.008)
        for k in range(7):
            x = -0.33 + k * 0.11
            b.cyl(0.012, top - 0.33, (x, sy, (top + 0.33) / 2), m, segs=8)
    for sx in (-1, 1):
        b.box((0.035, L - 0.08, 0.14), (sx * (w / 2 - 0.03), 0, 0.25), m, bevel=0.005)
    b.box((w - 0.08, L - 0.1, 0.05), (0, 0, 0.27), 'MI_Wood_Raw')
    b.box((w - 0.1, L - 0.12, 0.17), (0, 0, 0.38), 'MI_Fabric_Shade', bevel=0.035, segs=2)
    top = 0.465
    _drape(b, (w - 0.1) / 2 + 0.01, yfoot + 0.08, 0.48, top + 0.01,
           ['MI_Fabric_Curtain_Light', 'MI_Fabric_Sofa_Green', 'MI_Fabric_Curtain', 'MI_Velvet_Blue'],
           r=0.045, drop=0.13, thickness=0.02, cell=0.18, seed=4)
    b.tube([(-(w - 0.1) / 2 - 0.015, 0.49, top + 0.025), ((w - 0.1) / 2 + 0.015, 0.49, top + 0.025)], 0.035,
           'MI_Fabric_Bedding', segs=10)
    _pillow(b, 0.6, 0.38, 0.15, (0, 0.74, top + 0.07), 'MI_Fabric_Bedding', rot=(-12, 0, -4), seed=7)
    return b


@wprop('SM_Nightstand', 'Props/House', 'box', 'house', size=(0.5, 0.4, 0.55))
def nightstand():
    b = Builder('SM_Nightstand')
    m = 'MI_Cabinet_Wood'
    w, d, h = 0.5, 0.4, 0.55
    b.box((w, d, 0.025), (0, 0, h - 0.0125), m, bevel=0.006)
    for sx in (-1, 1):
        b.box((0.02, d - 0.02, h - 0.1), (sx * (w / 2 - 0.02), 0.0, 0.075 + (h - 0.1) / 2), m, bevel=0.003)
        for sy in (-1, 1):
            b.cyl(0.018, 0.07, (sx * (w / 2 - 0.035), sy * (d / 2 - 0.035), 0.035), m, segs=10, radius2=0.013)
    b.box((w - 0.04, 0.01, h - 0.1), (0, d / 2 - 0.015, 0.075 + (h - 0.1) / 2), m)
    b.box((w - 0.04, d - 0.03, 0.02), (0, 0, 0.08), m, bevel=0.002)
    b.box((w - 0.04, d - 0.03, 0.018), (0, 0, 0.33), m, bevel=0.002)
    b.box((w - 0.04, 0.02, 0.02), (0, -d / 2 + 0.02, 0.528), m)
    _drawer(b, 0, 0.43, w - 0.06, 0.17, -d / 2 + 0.005, m, knobs=1)
    for k in range(3):
        b.box((0.03, 0.2, 0.22 - k * 0.02), (-0.15 + k * 0.035, 0.02, 0.09 + (0.22 - k * 0.02) / 2),
              BOOK_MATS[k * 3 % len(BOOK_MATS)], bevel=0.002)
    return b


@wprop('SM_Dresser', 'Props/House', 'box', 'house', size=(1.2, 0.5, 0.85))
def dresser():
    b = Builder('SM_Dresser')
    m = 'MI_Wood_Dark'
    w, d, h = 1.2, 0.5, 0.85
    b.box((w, d, 0.03), (0, 0, h - 0.015), m, bevel=0.008)
    b.box((w - 0.04, d - 0.04, h - 0.11), (0, 0.01, 0.08 + (h - 0.11) / 2), m, bevel=0.004)
    b.box((w - 0.02, d - 0.03, 0.06), (0, 0.0, 0.05), m, bevel=0.006)
    for sx in (-1, 1):
        b.prism([(-0.04, 0), (0.04, 0), (0.04, 0.03), (0.0, 0.05), (-0.04, 0.05)], 0.06,
                (sx * (w / 2 - 0.06), -d / 2 + 0.04, 0.0), m, axis='Y', bevel=0.004)
    yf = -d / 2 + 0.03
    for sx in (-1, 1):
        _drawer(b, sx * (w - 0.08) / 4, 0.72, (w - 0.09) / 2, 0.16, yf - 0.02, m, knobs=1)
    for z in (0.53, 0.3):
        _drawer(b, 0, z, w - 0.08, 0.21, yf - 0.02, m)
    return b


@wprop('SM_Wardrobe', 'Props/House', 'box', 'house', size=(1.2, 0.6, 2.0))
def wardrobe():
    """Left door stands slightly ajar on a dark interior."""
    b = Builder('SM_Wardrobe')
    m = 'MI_Wood_Dark'
    w, d, h = 1.14, 0.56, 2.0
    yf = -d / 2
    for sx in (-1, 1):
        b.box((0.025, d, h - 0.18), (sx * (w / 2 - 0.0125), 0, 0.09 + (h - 0.18) / 2), m, bevel=0.004)
    b.box((w, 0.012, h - 0.18), (0, d / 2 - 0.006, 0.09 + (h - 0.18) / 2), m)
    b.box((w - 0.05, d - 0.02, 0.025), (0, 0, 0.1), m)
    b.box((w - 0.05, d - 0.02, 0.025), (0, 0, h - 0.1), m)
    b.box((w + 0.02, d + 0.02, 0.09), (0, 0, 0.045), m, bevel=0.008)
    # Crown moulding (stepped).
    for k, (dw, dz) in enumerate(((0.0, 0.03), (0.03, 0.025), (0.06, 0.03))):
        b.box((w + dw, d + dw / 2, dz), (0, -dw / 4, h - 0.09 + 0.015 + sum((0.03, 0.025, 0.03)[:k])), m, bevel=0.006)
    b.box((w - 0.05, 0.01, 0.06), (0, yf + 0.06, h - 0.13), m)
    b.cyl(0.012, w - 0.06, (0, 0.02, h - 0.3), 'MI_Brass', rot=(0, 90, 0), segs=10)
    from props import _garment
    for k, (x, mm) in enumerate(((-0.35, 'MI_Fabric_Navy'), (-0.22, 'MI_Fabric_Brown'))):
        g = _garment('coat', mm, 1.0, shoulders=0.42)
        _attach(b, g, (x, 0.02, h - 0.33), (0, 0, 90 + k * 8))
    dw = (w - 0.03) / 2
    for sx in (-1, 1):
        door = Builder('door')
        _panel_door(door, sx * -dw / 2, (h - 0.2) / 2 + 0.1, dw - 0.004, h - 0.21, -0.02, m, frame=0.08)
        _panel_door(door, sx * -dw / 2, h - 0.38, dw - 0.16, 0.22, -0.026, m, t=0.006, frame=0.04)
        door.prism(_ellipse(0.012, 0.022, 12), 0.004, (sx * -(dw - 0.05), -0.022, 1.1), 'MI_Brass', axis='Y')
        door.cyl(0.003, 0.004, (sx * -(dw - 0.05), -0.025, 1.105), 'MI_Plastic_Black', rot=(90, 0, 0), segs=6)
        _knob(door, (sx * -(dw - 0.05), -0.02, 1.0), 'MI_Brass', r=0.011)
        for z in (0.3, h - 0.3):
            door.cyl(0.008, 0.07, (0.0, -0.01, z), 'MI_Brass', segs=8)
        hinge = (sx * (w / 2 - 0.012), yf, 0)
        _attach(b, door, hinge, (0, 0, -9 if sx < 0 else 0))
    return _recentre(b)


@wprop('SM_Mirror_Wall', 'Props/House', 'box', 'house', kind='wall', size=(0.6, 0.04, 0.9))
def mirror_wall():
    b = Builder('SM_Mirror_Wall')
    w, h = 0.6, 0.9
    _frame_ring(b, w, h, w - 0.05, h - 0.05, -0.04, 0.0, 'MI_Wood_Dark', cz=h / 2, bevel=0.004)
    _frame_ring(b, w - 0.04, h - 0.04, w - 0.08, h - 0.08, -0.034, 0.0, 'MI_Wood_Dark', cz=h / 2, bevel=0.004)
    _frame_ring(b, w - 0.076, h - 0.076, w - 0.09, h - 0.09, -0.028, 0.0, 'MI_Gold', cz=h / 2, bevel=0.002)
    b.plane(w - 0.088, h - 0.088, (0, -0.018, h / 2), 'MI_Mirror')
    b.box((w - 0.06, 0.012, h - 0.06), (0, -0.008, h / 2), 'MI_Wood_Dark')
    return b


@wprop('SM_Teddy', 'Props/House', 'none', 'house', size=(0.22, 0.22, 0.25))
def teddy():
    """Worn teddy bear, sitting: one button eye hangs on a thread, a patch on the belly."""
    b = Builder('SM_Teddy')
    m, pad = 'MI_Fabric_Brown', 'MI_Fabric_Shade'
    _blob(b, 0.06, (0, 0.01, 0.065), m, scale=(1.0, 0.85, 1.15), subdiv=3, amp=0.05, seed=1)
    _blob(b, 0.052, (0, -0.0, 0.17), m, scale=(1.05, 0.95, 0.95), subdiv=3, amp=0.04, seed=2, rot=(0, 0, 8))
    b.sphere(0.024, (0.003, -0.045, 0.158), pad, scale=(1.1, 0.8, 0.8), segs=12, rings=8)
    b.sphere(0.008, (0.003, -0.064, 0.165), 'MI_Plastic_Black', scale=(1.3, 0.8, 0.9), segs=8, rings=5)
    for sx in (-1, 1):
        b.sphere(0.022, (sx * 0.042, 0.005, 0.215), m, scale=(1, 0.55, 1), segs=10, rings=6)
        b.sphere(0.013, (sx * 0.042, -0.004, 0.215), pad, scale=(1, 0.4, 1), segs=8, rings=5)
        b.tube([(sx * 0.05, 0.0, 0.1), (sx * 0.075, -0.03, 0.06), (sx * 0.07, -0.05, 0.035)], 0.021, m, segs=10,
               bend=0.02)
        b.tube([(sx * 0.035, -0.02, 0.035), (sx * 0.05, -0.09, 0.024)], 0.026, m, segs=10)
        b.cyl(0.026, 0.012, (sx * 0.05, -0.115, 0.026), pad, rot=(90, 0, 0), segs=12)
    b.cyl(0.007, 0.004, (-0.021, -0.046, 0.186), 'MI_Plastic_Black', rot=(80, 0, 0), segs=10)
    b.tube([(0.021, -0.047, 0.186), (0.024, -0.054, 0.17), (0.026, -0.055, 0.152)], 0.0008, 'MI_Cable', segs=4)
    b.cyl(0.007, 0.004, (0.026, -0.056, 0.149), 'MI_Plastic_Black', rot=(95, 0, 10), segs=10)
    b.box((0.04, 0.006, 0.036), (0.012, -0.058, 0.07), 'MI_Fabric_Olive', rot=(-10, 0, 8), bevel=0.002)
    b.torus(0.03, 0.004, (0, 0.0, 0.122), 'MI_Fabric_Curtain', segs=16, rsegs=5, scale=(1.3, 1.2, 1))
    b.sphere(0.012, (0.03, -0.035, 0.115), 'MI_Fabric_Curtain', scale=(1.3, 0.6, 0.9), segs=8, rings=5)
    return _recentre(b)


def _sloft(b, rings, mat, n=2.6, segs=32, loc=(0, 0, 0)):
    """Loft through superellipse rings [(half_x, half_y, z), ...]; a 0 half-size closes an end.
    Used for tubs, basins and other rounded-rectangle vessels."""
    pb, uv = b._begin()
    ox, oy, oz = loc
    vr = []
    for a, bb, z in rings:
        if a < 1e-6:
            vr.append([pb.verts.new((ox, oy, oz + z))])
            continue
        ring = []
        for i in range(segs):
            t = 2 * math.pi * i / segs
            c, s = math.cos(t), math.sin(t)
            ring.append(pb.verts.new((ox + a * math.copysign(abs(c) ** (2 / n), c),
                                      oy + bb * math.copysign(abs(s) ** (2 / n), s), oz + z)))
        vr.append(ring)
    for ra, rb in zip(vr, vr[1:]):
        if len(ra) == 1 and len(rb) == 1:
            continue
        for i in range(segs):
            j = (i + 1) % segs
            if len(ra) == 1:
                pb.faces.new((ra[0], rb[i], rb[j]))
            elif len(rb) == 1:
                pb.faces.new((ra[i], rb[0], ra[j]))
            else:
                pb.faces.new((ra[i], ra[j], rb[j], rb[i]))
    bmesh.ops.recalc_face_normals(pb, faces=pb.faces)
    pb.normal_update()
    _box_uv(pb.faces, uv)
    b._commit(pb, mat)
    return b


def _cross_tap(b, x, y, z, mat='MI_Chrome', dot=None):
    b.cyl(0.018, 0.035, (x, y, z + 0.0175), mat, segs=12)
    b.cyl(0.008, 0.02, (x, y, z + 0.045), mat, segs=8)
    for k in range(4):
        a = k * 90 + 45
        b.box((0.032, 0.007, 0.007), (x + 0.016 * math.cos(math.radians(a)), y + 0.016 * math.sin(math.radians(a)),
                                      z + 0.055), mat, rot=(0, 0, a), bevel=0.002)
        b.sphere(0.0055, (x + 0.032 * math.cos(math.radians(a)), y + 0.032 * math.sin(math.radians(a)), z + 0.055), mat,
                 segs=8, rings=5)
    b.sphere(0.009, (x, y, z + 0.06), dot or mat, scale=(1, 1, 0.6), segs=8, rings=5)


@wprop('SM_Toilet', 'Props/House', 'convex', 'house', size=(0.4, 0.7, 0.78))
def toilet():
    """Tank against +Y (the wall), bowl front at -Y."""
    b = Builder('SM_Toilet')
    p = 'MI_Porcelain'
    by = -0.12
    b.lathe([(0, 0), (0.11, 0), (0.115, 0.01), (0.1, 0.05), (0.094, 0.15), (0.12, 0.24), (0.155, 0.32),
             (0.168, 0.36), (0.17, 0.378), (0.162, 0.385), (0.14, 0.382), (0.105, 0.33), (0.07, 0.28), (0.0, 0.26)],
            (0, by, 0), p, segs=28, scale=(1, 1.32, 1))
    b.box((0.2, 0.26, 0.24), (0, 0.06, 0.26), p, bevel=0.05, segs=3)
    for sx in (-1, 1):
        b.sphere(0.017, (sx * 0.095, -0.04, 0.03), p, scale=(1, 1, 0.8), segs=10, rings=6)
    # Tank with lid and flush lever.
    b.box((0.39, 0.17, 0.36), (0, 0.265, 0.39 + 0.18), p, bevel=0.022, segs=3)
    b.box((0.405, 0.185, 0.032), (0, 0.265, 0.766), p, bevel=0.01)
    b.cyl(0.016, 0.012, (-0.15, 0.175, 0.69), 'MI_Chrome', rot=(90, 0, 0), segs=12)
    b.box((0.07, 0.01, 0.014), (-0.12, 0.165, 0.69), 'MI_Chrome', rot=(0, 8, 0), bevel=0.004)
    # Seat and closed lid (hinged at the back).
    b.torus(0.122, 0.024, (0, by + 0.0, 0.398), 'MI_Plastic_White', segs=28, rsegs=6, scale=(1, 1.3, 0.38))
    b.prism(_ellipse(0.165, 0.2, 24, 0, by - 0.002), 0.022, (0, 0, 0.42), 'MI_Plastic_White', axis='Z', bevel=0.008)
    for sx in (-1, 1):
        b.box((0.03, 0.04, 0.022), (sx * 0.07, 0.11, 0.41), 'MI_Plastic_White', bevel=0.006)
    # Supply line and shut-off valve.
    b.tube([(-0.13, 0.26, 0.39), (-0.13, 0.26, 0.2), (-0.13, 0.31, 0.16), (-0.13, 0.34, 0.16)], 0.006, 'MI_Chrome',
           segs=8, bend=0.04)
    b.cyl(0.014, 0.04, (-0.13, 0.33, 0.16), 'MI_Chrome', rot=(90, 0, 0), segs=10)
    b.box((0.03, 0.008, 0.012), (-0.13, 0.31, 0.18), 'MI_Chrome', bevel=0.003)
    return _recentre(b)


@wprop('SM_Bathtub', 'Props/House', 'complex', 'house', size=(1.7, 0.75, 0.55))
def bathtub():
    """Clawfoot tub, taps at the -X end."""
    b = Builder('SM_Bathtub')
    rings = [(0, 0, 0.13), (0.6, 0.2, 0.13), (0.72, 0.29, 0.16), (0.8, 0.345, 0.3), (0.83, 0.362, 0.47),
             (0.845, 0.372, 0.51), (0.85, 0.375, 0.53), (0.843, 0.37, 0.548), (0.825, 0.355, 0.55), (0.812, 0.344, 0.535),
             (0.79, 0.322, 0.45), (0.75, 0.29, 0.26), (0.67, 0.225, 0.18), (0.5, 0.15, 0.165), (0, 0, 0.165)]
    _sloft(b, rings, 'MI_Porcelain', n=2.4, segs=36)
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = sx * 0.6, sy * 0.21
            b.lathe([(0, 0.0), (0.05, 0.0), (0.055, 0.02), (0.035, 0.055), (0.028, 0.1), (0.04, 0.14), (0.0, 0.16)],
                    (x, y, 0), 'MI_Brass', segs=10)
            for k in (-1, 0, 1):
                a = math.atan2(sy, sx * 1.6) + k * 0.6
                b.sphere(0.017, (x + math.cos(a) * 0.045, y + math.sin(a) * 0.045, 0.014), 'MI_Brass',
                         scale=(1.4, 1, 0.8), rot=(0, 0, math.degrees(a)), segs=8, rings=5)
    # Taps and spout on the rim at the -X end, drain with plug chain, overflow.
    for sy in (-1, 1):
        _cross_tap(b, -0.79, sy * 0.1, 0.548, 'MI_Brass', 'MI_Porcelain')
    b.tube([(-0.81, 0.0, 0.55), (-0.81, 0.0, 0.6), (-0.77, 0.0, 0.625), (-0.72, 0.0, 0.59)], 0.014, 'MI_Brass', segs=10,
           bend=0.04)
    b.cyl(0.025, 0.006, (-0.6, 0.0, 0.17), 'MI_Brass', segs=14)
    b.cyl(0.03, 0.008, (-0.785, 0.0, 0.42), 'MI_Brass', rot=(0, 75, 0), segs=14)
    b.tube([(-0.78, 0.0, 0.41), (-0.72, 0.03, 0.3), (-0.62, 0.05, 0.18), (-0.56, 0.04, 0.17)], 0.0018, 'MI_Brass', segs=4,
           bend=0.05)
    b.cyl(0.022, 0.014, (-0.55, 0.04, 0.174), 'MI_Rubber_Black', segs=12)
    return b


@wprop('SM_Sink_Pedestal', 'Props/House', 'convex', 'house', size=(0.55, 0.45, 0.85))
def sink_pedestal():
    """Back against +Y."""
    b = Builder('SM_Sink_Pedestal')
    p = 'MI_Porcelain'
    b.lathe([(0, 0), (0.12, 0), (0.122, 0.015), (0.095, 0.04), (0.075, 0.2), (0.068, 0.5), (0.085, 0.62), (0.0, 0.64)],
            (0, 0.06, 0), p, segs=20, scale=(1, 0.8, 1))
    rings = [(0, 0, 0.62), (0.15, 0.1, 0.63), (0.23, 0.17, 0.7), (0.268, 0.205, 0.79), (0.275, 0.212, 0.83),
             (0.272, 0.21, 0.85), (0.255, 0.193, 0.852), (0.245, 0.18, 0.83), (0.2, 0.14, 0.75), (0.12, 0.08, 0.705),
             (0, 0, 0.7)]
    _sloft(b, rings, p, n=3.0, segs=32, loc=(0, -0.01, 0))
    b.box((0.5, 0.06, 0.04), (0, 0.19, 0.835), p, bevel=0.015)
    b.box((0.5, 0.02, 0.07), (0, 0.215, 0.87), p, bevel=0.008)
    _cross_tap(b, -0.12, 0.185, 0.855, 'MI_Chrome', 'MI_Plastic_Red')
    _cross_tap(b, 0.12, 0.185, 0.855, 'MI_Chrome', 'MI_Metal_Blue')
    b.cyl(0.018, 0.03, (0, 0.185, 0.87), 'MI_Chrome', segs=12)
    b.tube([(0, 0.185, 0.88), (0, 0.17, 0.9), (0, 0.11, 0.89)], 0.011, 'MI_Chrome', segs=10, bend=0.02)
    b.cyl(0.022, 0.004, (0, -0.01, 0.702), 'MI_Chrome', segs=14)
    b.cyl(0.016, 0.005, (0, -0.01, 0.702), 'MI_Metal_Black', segs=12)
    b.cyl(0.012, 0.004, (0, -0.17, 0.79), 'MI_Chrome', rot=(-60, 0, 0), segs=10)
    return _recentre(b)


@wprop('SM_Medicine_Cabinet', 'Props/House', 'box', 'house', kind='wall', size=(0.5, 0.12, 0.65))
def medicine_cabinet():
    b = Builder('SM_Medicine_Cabinet')
    m = 'MI_Cabinet_White'
    w, d, h = 0.5, 0.12, 0.65
    b.box_minmax((-w / 2, -d + 0.02, 0.0), (w / 2, 0.0, h), m, bevel=0.004)
    b.box_minmax((-w / 2 + 0.005, -d, 0.005), (w / 2 - 0.005, -d + 0.02, h - 0.005), m, bevel=0.004)
    _frame_ring(b, w - 0.03, h - 0.03, w - 0.06, h - 0.06, -d - 0.004, -d + 0.002, 'MI_Chrome', cz=h / 2, bevel=0.002)
    b.plane(w - 0.058, h - 0.058, (0, -d - 0.0015, h / 2), 'MI_Mirror')
    b.box((0.012, 0.01, 0.06), (w / 2 - 0.035, -d - 0.006, h / 2), 'MI_Chrome', bevel=0.004)
    return b


@wprop('SM_Towel_Rack', 'Props/House', 'none', 'house', kind='wall', size=(0.66, 0.11, 0.6))
def towel_rack():
    """Rod at (0, -0.08, 0.56); the towel hangs down to z = 0."""
    b = Builder('SM_Towel_Rack')
    rz, ry, rr = 0.56, -0.08, 0.011
    for sx in (-1, 1):
        b.cyl(0.026, 0.008, (sx * 0.31, -0.004, rz), 'MI_Chrome', rot=(90, 0, 0), segs=14)
        b.cyl(0.01, 0.075, (sx * 0.31, -0.045, rz), 'MI_Chrome', rot=(90, 0, 0), segs=10)
        b.sphere(0.016, (sx * 0.31, ry, rz), 'MI_Chrome', segs=10, rings=6)
    b.cyl(rr, 0.6, (0, ry, rz), 'MI_Chrome', rot=(0, 90, 0), segs=10)
    R = rr + 0.006
    front_len, back_len = rz - 0.0, rz - 0.09
    total = front_len + math.pi * R + back_len

    def fn(u, v):
        x = -0.24 + 0.48 * u
        s = v * total
        if s < front_len:
            y, z = ry - R, s
            hang = 1 - s / front_len
        elif s < front_len + math.pi * R:
            a = math.pi - (s - front_len) / R
            y, z = ry + R * math.cos(a), rz + R * math.sin(a)
            hang = 0.0
        else:
            y, z = ry + R, rz - (s - front_len - math.pi * R)
            hang = (s - front_len - math.pi * R) / back_len
        y += 0.008 * math.sin(x * 26 + 1.0) * hang * (1 if y < ry else 0.6)
        return (x + 0.01 * hang * math.sin(z * 3), y, z)
    _sheet(b, fn, 18, 30, 'MI_Fabric_Sofa_Green', thickness=0.006, size=(0.48, total))
    for zz in (0.06, 0.1):
        b.box((0.481, 0.002, 0.012), (0, ry - R - 0.0045, zz), 'MI_Fabric_Bedding')
    return b


@wprop('SM_ShowerCurtain', 'Props/House', 'none', 'house', size=(1.7, 0.1, 1.95))
def shower_curtain():
    """Rod 1.7 m along X at z = 1.92 (ends meet the walls), curtain mostly drawn to -X."""
    b = Builder('SM_ShowerCurtain')
    L, rz = 1.7, 1.92
    b.cyl(0.012, L, (0, 0, rz), 'MI_Chrome', rot=(0, 90, 0), segs=10)
    for sx in (-1, 1):
        b.cyl(0.03, 0.01, (sx * (L / 2 - 0.005), 0, rz), 'MI_Chrome', rot=(0, 90, 0), segs=14)
    x0, x1 = -L / 2 + 0.03, 0.45
    n_rings = 12
    for k in range(n_rings):
        x = x0 + (x1 - x0) * k / (n_rings - 1)
        b.torus(0.022, 0.003, (x, 0, rz - 0.012), 'MI_Plastic_White', rot=(90, 0, 90), segs=12, rsegs=4)

    def fn(u, v):
        x = x0 + (x1 - x0) * u
        z = 0.06 + (rz - 0.1) * v
        lam = (x1 - x0) / (n_rings - 1)
        amp = 0.03 * (1.25 - 0.35 * v)
        y = amp * math.sin(2 * math.pi * (x - x0) / lam + 1.2 * (1 - v))
        return (x + 0.012 * (1 - v) * math.sin(x * 9), y, z - 0.01 * (1 - v) * math.sin(x * 13) ** 2)
    _sheet(b, fn, 88, 10, 'MI_Fabric_Curtain_Light', thickness=0.003, size=(x1 - x0, rz))
    return _recentre(b, x=False, y=False)


@wprop('SM_Desk_Home', 'Props/House', 'box', 'house', size=(1.2, 0.6, 0.75))
def desk_home():
    b = Builder('SM_Desk_Home')
    m = 'MI_Cabinet_Wood'
    w, d, h = 1.2, 0.6, 0.75
    b.box((w, d, 0.03), (0, 0, h - 0.015), m, bevel=0.006)
    b.box((0.7, 0.45, 0.004), (-0.12, -0.02, h + 0.002), 'MI_Leather', bevel=0.001)
    px = w / 2 - 0.21
    b.box((0.4, d - 0.04, h - 0.03), (px, 0.01, (h - 0.03) / 2), m, bevel=0.004)
    for k, (z, hh) in enumerate(((0.6, 0.14), (0.42, 0.18), (0.18, 0.26))):
        _drawer(b, px, z, 0.37, hh, -d / 2 + 0.01, m, knobs=1)
    b.box((0.03, d - 0.04, h - 0.03), (-w / 2 + 0.03, 0.01, (h - 0.03) / 2), m, bevel=0.004)
    b.box((w - 0.48, 0.015, 0.4), (-0.2, d / 2 - 0.04, h - 0.23), m)
    b.box((0.55, d - 0.06, 0.08), (-0.17, 0.0, h - 0.07), m)
    _drawer(b, -0.17, h - 0.07, 0.5, 0.07, -d / 2 + 0.01, m, knobs=1)
    return b


@wprop('SM_OfficeChair', 'Props/House', 'box', 'house', size=(0.6, 0.6, 1.0))
def office_chair():
    b = Builder('SM_OfficeChair')
    for k in range(5):
        a = math.radians(90 + k * 72)
        c, s = math.cos(a), math.sin(a)
        b.box((0.27, 0.045, 0.03), (c * 0.135, s * 0.135, 0.075), 'MI_Plastic_Black', rot=(0, 0, math.degrees(a)),
              bevel=0.008)
        b.cyl(0.012, 0.03, (c * 0.26, s * 0.26, 0.06), 'MI_Plastic_Black', segs=8)
        b.cyl(0.026, 0.04, (c * 0.26, s * 0.26, 0.028), 'MI_Plastic_Black', rot=(0, 90, math.degrees(a) + 90), segs=12)
    b.cyl(0.04, 0.05, (0, 0, 0.085), 'MI_Plastic_Black', segs=14)
    b.cyl(0.025, 0.3, (0, 0, 0.25), 'MI_Chrome', segs=12)
    b.box((0.2, 0.22, 0.04), (0, 0.0, 0.42), 'MI_Plastic_Black', bevel=0.006)
    b.box((0.06, 0.18, 0.012), (0.08, -0.14, 0.415), 'MI_Plastic_Black', rot=(0, 0, 0), bevel=0.003)
    b.box((0.5, 0.48, 0.09), (0, -0.02, 0.48), 'MI_Fabric_Navy', bevel=0.035, segs=3)
    b.box((0.06, 0.04, 0.32), (0, 0.24, 0.55), 'MI_Plastic_Black', rot=(-8, 0, 0), bevel=0.01)
    b.box((0.46, 0.08, 0.46), (0, 0.27, 0.76), 'MI_Fabric_Navy', rot=(-8, 0, 0), bevel=0.035, segs=3)
    for sx in (-1, 1):
        b.box((0.03, 0.04, 0.18), (sx * 0.27, 0.02, 0.6), 'MI_Plastic_Black', bevel=0.008)
        b.box((0.06, 0.24, 0.03), (sx * 0.27, -0.02, 0.7), 'MI_Plastic_Black', bevel=0.012)
        b.box((0.03, 0.12, 0.02), (sx * 0.24, 0.02, 0.53), 'MI_Plastic_Black', bevel=0.005)
    return _recentre(b)


@wprop('SM_Monitor_Flat', 'Props/House', 'box', 'house', size=(0.55, 0.18, 0.42))
def monitor_flat():
    b = Builder('SM_Monitor_Flat')
    w, h, z0 = 0.55, 0.35, 0.07
    b.prism(_rrect(0.24, 0.18, 0.03), 0.014, (0, 0, 0.007), 'MI_Plastic_Black', axis='Z', bevel=0.003)
    b.box((0.05, 0.025, 0.12), (0, 0.035, 0.07), 'MI_Plastic_Black', rot=(-10, 0, 0), bevel=0.006)
    b.box((w, 0.022, h), (0, 0, z0 + h / 2), 'MI_Plastic_Black', bevel=0.005)
    b.box((0.3, 0.03, 0.18), (0, 0.022, z0 + 0.17), 'MI_Plastic_Black', bevel=0.012)
    b.plane(w - 0.03, h - 0.04, (0, -0.0112, z0 + h / 2 + 0.005), 'MI_Screen_Black')
    b.box((0.004, 0.003, 0.004), (w / 2 - 0.03, -0.0115, z0 + 0.009), 'MI_Emit_Green')
    for k in range(4):
        b.box((0.012, 0.003, 0.004), (w / 2 - 0.12 + k * 0.02, -0.0115, z0 + 0.009), 'MI_Plastic_Grey')
    return b


def _laundry(name, dryer=False):
    b = Builder(name)
    w, d, h = 0.6, 0.58, 0.85
    p = 'MI_Porcelain'
    yf = -d / 2 + 0.01
    b.box_minmax((-w / 2, yf, 0.02), (w / 2, d / 2, h - 0.02), p, bevel=0.012)
    b.box_minmax((-w / 2, yf - 0.01, h - 0.025), (w / 2, d / 2, h), 'MI_Plastic_White', bevel=0.008)
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.cyl(0.02, 0.02, (sx * (w / 2 - 0.05), sy * (d / 2 - 0.06), 0.01), 'MI_Rubber_Black', segs=8)
    pz = h - 0.1
    b.box_minmax((-w / 2 + 0.005, yf - 0.025, pz - 0.06), (w / 2 - 0.005, yf, pz + 0.065), 'MI_Plastic_White',
                 bevel=0.008)
    if dryer:
        for x in (-0.18, 0.18):
            b.cyl(0.032, 0.025, (x, yf - 0.035, pz), 'MI_Plastic_Grey', rot=(90, 0, 0), segs=16)
            b.box((0.006, 0.006, 0.026), (x, yf - 0.049, pz + 0.012), 'MI_Plastic_Black')
        b.box((0.12, 0.004, 0.03), (0, yf - 0.026, pz), 'MI_Plastic_Grey', bevel=0.002)
        b.box((0.012, 0.004, 0.008), (0.05, yf - 0.028, pz), 'MI_Emit_Amber')
    else:
        b.box_minmax((-w / 2 + 0.02, yf - 0.03, pz - 0.045), (-w / 2 + 0.19, yf - 0.024, pz + 0.05), 'MI_Plastic_White',
                     bevel=0.004)
        b.box((0.08, 0.006, 0.012), (-w / 2 + 0.105, yf - 0.032, pz - 0.02), 'MI_Plastic_Grey', bevel=0.003)
        b.box((0.09, 0.004, 0.04), (0.02, yf - 0.026, pz + 0.01), 'MI_Screen_Black', bevel=0.002)
        b.box((0.05, 0.002, 0.014), (0.02, yf - 0.029, pz + 0.01), 'MI_Emit_Green')
        for k in range(4):
            b.cyl(0.007, 0.006, (-0.04 + k * 0.03, yf - 0.027, pz - 0.03), 'MI_Plastic_Grey', rot=(90, 0, 0), segs=8)
        b.cyl(0.034, 0.025, (0.2, yf - 0.035, pz), 'MI_Chrome', rot=(90, 0, 0), segs=18)
        b.box((0.006, 0.006, 0.03), (0.2, yf - 0.049, pz + 0.012), 'MI_Plastic_Black')
    cz = 0.4
    if dryer:
        b.cyl(0.21, 0.035, (0, yf - 0.018, cz), 'MI_Plastic_White', rot=(90, 0, 0), segs=32)
        b.torus(0.205, 0.008, (0, yf - 0.035, cz), 'MI_Plastic_Grey', rot=(90, 0, 0), segs=32, rsegs=5)
        b.sphere(0.12, (0, yf - 0.036, cz + 0.02), 'MI_Glass_Van', scale=(1, 0.12, 1), segs=20, rings=8)
        b.box((0.03, 0.04, 0.09), (0.18, yf - 0.05, cz), 'MI_Plastic_Grey', bevel=0.01)
    else:
        b.torus(0.17, 0.028, (0, yf - 0.03, cz), 'MI_Plastic_Grey', rot=(90, 0, 0), segs=32, rsegs=8)
        b.torus(0.17, 0.012, (0, yf - 0.052, cz), 'MI_Chrome', rot=(90, 0, 0), segs=32, rsegs=5)
        b.sphere(0.15, (0, yf - 0.04, cz), 'MI_Glass_Van', scale=(1, 0.2, 1), segs=24, rings=10)
        b.box((0.03, 0.04, 0.09), (0.175, yf - 0.055, cz), 'MI_Plastic_Grey', bevel=0.01)
    b.box_minmax((-w / 2 + 0.03, yf - 0.006, 0.04), (w / 2 - 0.03, yf, 0.12), p, bevel=0.004)
    b.box((0.1, 0.004, 0.05), (w / 2 - 0.1, yf - 0.007, 0.08), 'MI_Plastic_White', bevel=0.002)
    return _recentre(b)


@wprop('SM_Washer', 'Props/House', 'box', 'house', size=(0.6, 0.6, 0.85))
def washer():
    return _laundry('SM_Washer')


@wprop('SM_Dryer', 'Props/House', 'box', 'house', size=(0.6, 0.6, 0.85))
def dryer():
    return _laundry('SM_Dryer', dryer=True)


@wprop('SM_Furnace', 'Props/House', 'box', 'house', size=(0.7, 0.7, 1.5))
def furnace():
    """Old oil furnace; supply/return ducts and the flue rise to a 2.3 m basement ceiling.
    Burner peephole glow at (0, -0.37, 0.3)."""
    b = Builder('SM_Furnace')
    m = 'MI_Metal_Cream'
    w, h = 0.7, 1.5
    y0, y1 = -0.2, 0.35
    b.box_minmax((-w / 2, y0, 0.05), (w / 2, y1, h), m, bevel=0.01)
    b.box_minmax((-w / 2 + 0.02, y0 + 0.02, 0.0), (w / 2 - 0.02, y1 - 0.02, 0.05), 'MI_Metal_Rust')
    b.box_minmax((-w / 2 + 0.04, y0 - 0.012, 0.75), (w / 2 - 0.04, y0, 1.42), m, bevel=0.005)
    for k in range(9):
        b.box((0.4, 0.006, 0.012), (0, y0 - 0.014, 1.0 + k * 0.035), 'MI_Metal_Black')
    b.box((0.14, 0.003, 0.08), (0.2, y0 - 0.014, 1.3), 'MI_Paper')
    b.box_minmax((-w / 2 + 0.04, y0 - 0.012, 0.1), (w / 2 - 0.04, y0, 0.7), m, bevel=0.005)
    # Oil burner gun sticking out of the lower panel.
    b.cyl(0.07, 0.15, (0.0, y0 - 0.075, 0.3), 'MI_Metal_Grey', rot=(90, 0, 0), segs=16)
    b.box((0.18, 0.12, 0.16), (0.0, y0 - 0.1, 0.3), 'MI_Metal_Grey', bevel=0.01)
    b.cyl(0.065, 0.11, (0.13, y0 - 0.1, 0.3), 'MI_Metal_Red', rot=(0, 90, 0), segs=16)
    b.box((0.1, 0.08, 0.06), (-0.02, y0 - 0.1, 0.41), 'MI_Metal_Black', bevel=0.006)
    b.cyl(0.015, 0.012, (-0.06, y0 - 0.152, 0.3), 'MI_Emit_Ember', rot=(90, 0, 0), segs=10)
    b.tube([(-0.09, y0 - 0.1, 0.24), (-0.2, y0 - 0.12, 0.05), (-0.25, y0 - 0.12, 0.012), (-0.34, y0 - 0.13, 0.012)],
           0.005, 'MI_Copper', segs=6, bend=0.04)
    b.box((0.1, 0.05, 0.12), (w / 2 + 0.0, y1 - 0.1, 1.1), 'MI_Metal_Grey', bevel=0.004)
    b.box((0.03, 0.02, 0.05), (w / 2 + 0.03, y1 - 0.13, 1.1), 'MI_Metal_Red', bevel=0.004)
    # Plenum, ducts and flue up to the ceiling.
    b.box_minmax((-w / 2 + 0.02, y0 + 0.02, h), (w / 2 - 0.02, y1 - 0.02, h + 0.25), 'MI_Steel', bevel=0.004)
    for x in (-0.15, 0.15):
        b.box_minmax((x - 0.12, -0.12, h + 0.25), (x + 0.12, 0.1, 2.3), 'MI_Steel', bevel=0.003)
        b.box_minmax((x - 0.13, -0.13, h + 0.3), (x + 0.13, 0.11, h + 0.33), 'MI_Steel', bevel=0.003)
    b.cyl(0.07, 0.6, (0.0, y1 - 0.02, h + 0.48), 'MI_Metal_Rust', segs=14)
    b.tube([(0.0, y1 - 0.02, h + 0.75), (0.0, y1 - 0.02, 2.0), (0.0, y1 - 0.1, 2.3)], 0.07, 'MI_Metal_Rust', segs=14,
           bend=0.1)
    return b


@wprop('SM_Shelf_Wood', 'Props/House', 'box', 'house', size=(1.2, 0.4, 1.8))
def shelf_wood():
    """Rough basement shelf with preserves, cans, paint and boxes."""
    b = Builder('SM_Shelf_Wood')
    rng = _rng('SM_Shelf_Wood')
    w, d, h = 1.2, 0.4, 1.8
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.box((0.04, 0.09, h), (sx * (w / 2 - 0.02), sy * (d / 2 - 0.045), h / 2), 'MI_Wood_Raw', bevel=0.003,
                  uv_offset=(sx * 0.3, sy * 0.2))
        b.box((0.025, d - 0.18, 0.06), (sx * (w / 2 - 0.02), 0, 0.3), 'MI_Wood_Raw', bevel=0.002)
    b.box((1.25, 0.02, 0.07), (0, d / 2 - 0.02, h / 2), 'MI_Wood_Raw', rot=(0, -54, 0), bevel=0.002)
    levels = (0.12, 0.56, 1.0, 1.44)
    for z in levels:
        for k in range(3):
            b.box((w + 0.02, 0.12, 0.022), (0, -d / 2 + 0.065 + k * 0.135, z - 0.011), 'MI_Wood_Raw', bevel=0.002,
                  uv_offset=(z * 3 + k, k * 0.37))
    jar_fill = ('MI_Plastic_Red', 'MI_Fabric_Olive', 'MI_Plastic_Yellow', 'MI_Bakelite', 'MI_Leather')
    for li, z in enumerate(levels):
        x = -w / 2 + 0.06
        while x < w / 2 - 0.12:
            r = rng.random()
            if r < 0.4:                      # mason jar
                jr, jh = rng.uniform(0.04, 0.05), rng.uniform(0.13, 0.18)
                y = rng.uniform(-0.08, 0.06)
                b.lathe([(0, 0), (jr - 0.006, 0), (jr, 0.008), (jr, jh * 0.8), (jr * 0.8, jh * 0.9), (jr * 0.8, jh),
                         (0, jh)], (x + jr, y, z), 'MI_Glass', segs=12)
                b.lathe([(0, 0.004), (jr - 0.004, 0.004), (jr - 0.004, jh * 0.72), (0, jh * 0.74)], (x + jr, y, z),
                        rng.choice(jar_fill), segs=10)
                b.cyl(jr * 0.82, 0.014, (x + jr, y, z + jh + 0.007), 'MI_Brass', segs=12)
                b.cyl(jr + 0.0008, 0.035, (x + jr, y, z + jh * 0.45), 'MI_Paper', segs=12) if rng.random() < 0.5 else None
                x += 2 * jr + 0.012
            elif r < 0.6:                    # tin can
                cr, ch = 0.038, rng.uniform(0.1, 0.12)
                y = rng.uniform(-0.1, 0.08)
                b.lathe([(0, 0.002), (cr - 0.004, 0), (cr, 0.006), (cr, ch - 0.006), (cr - 0.004, ch), (0, ch - 0.002)],
                        (x + cr, y, z), 'MI_Steel', segs=12)
                b.cyl(cr + 0.0008, ch * 0.7, (x + cr, y, z + ch / 2), rng.choice(('MI_Paper', 'MI_Plastic_Red',
                                                                                 'MI_Metal_Green')), segs=12)
                x += 2 * cr + 0.008
            elif r < 0.75 and li < 3:        # paint can
                pr, ph = 0.085, 0.19
                b.lathe([(0, 0), (pr, 0), (pr, ph - 0.01), (pr - 0.005, ph), (0, ph)], (x + pr, 0.0, z),
                        rng.choice(('MI_Metal_Cream', 'MI_Metal_Blue', 'MI_Metal_Grey')), segs=16)
                b.torus(pr - 0.015, 0.003, (x + pr, 0.0, z + ph), 'MI_Steel', rot=(90, 0, rng.uniform(0, 60)), segs=10,
                        rsegs=4, arc=180)
                x += 2 * pr + 0.015
            elif r < 0.92:                   # cardboard box
                bw, bd, bh = rng.uniform(0.2, 0.32), rng.uniform(0.24, 0.32), rng.uniform(0.14, 0.3)
                if x + bw > w / 2 - 0.03:
                    break
                b.box((bw, bd, bh), (x + bw / 2, 0.01, z + bh / 2), 'MI_Cardboard', rot=(0, 0, rng.uniform(-4, 4)),
                      bevel=0.003, uv_offset=(x, z))
                b.box((0.05, bd + 0.002, 0.002), (x + bw / 2, 0.01, z + bh + 0.0005), 'MI_Plastic_Beige')
                x += bw + 0.02
            else:
                x += rng.uniform(0.05, 0.12)
    return b


@wprop('SM_Plant_Pot', 'Props/House', 'convex', 'house', size=(0.35, 0.35, 0.9))
def plant_pot():
    b = Builder('SM_Plant_Pot')
    rng = _rng('SM_Plant_Pot')
    b.lathe([(0, 0.0), (0.11, 0.0), (0.115, 0.01), (0.15, 0.26), (0.165, 0.27), (0.165, 0.29), (0.15, 0.29),
             (0.142, 0.27), (0.0, 0.25)], mat='MI_Ceramic', segs=24)
    b.torus(0.13, 0.004, (0, 0, 0.12), 'MI_Car_Paint_Blue', segs=24, rsegs=4, scale=(1.0, 1.0, 1.0))
    _blob(b, 0.14, (0, 0, 0.255), 'MI_Dirt', scale=(1, 1, 0.12), subdiv=2, amp=0.15, seed=2)
    for sa, lean, top in ((0.3, 0.05, 0.86), (2.4, 0.07, 0.72), (4.4, 0.06, 0.64)):
        base = Vector((math.cos(sa) * 0.03, math.sin(sa) * 0.03, 0.26))
        tip = Vector((math.cos(sa) * lean, math.sin(sa) * lean, top))
        b.tube([tuple(base), tuple(base.lerp(tip, 0.5) + Vector((0.01, 0, 0))), tuple(tip)], 0.007, 'MI_Wood_Dark',
               segs=6, bend=0.1)
        n = int((top - 0.33) / 0.055)
        for k in range(n + 1):
            t = 1.0 - k / (n + 1) * 0.85
            p = base.lerp(tip, t)
            a = sa + k * 2.4 + rng.uniform(-0.3, 0.3)
            ll = rng.uniform(0.14, 0.19) * (0.75 if k == 0 else 1.0)
            up = 50 if k == 0 else rng.uniform(15, 40)
            lv = Builder('leaf')
            lv.sphere(1.0, (0, ll * 0.5, 0), 'MI_Hedge', scale=(ll * 0.3, ll * 0.5, 0.004), segs=10, rings=6)
            for vtx in lv.bm.verts:
                tt = vtx.co.y / ll
                vtx.co.z -= 0.22 * ll * tt * tt + 0.15 * abs(vtx.co.x)
            _attach(b, lv, tuple(p), (up, 0, math.degrees(a) - 90))
    return b


@wprop('SM_Coat_Rack', 'Props/House', 'box', 'house', size=(0.5, 0.5, 1.8))
def coat_rack():
    b = Builder('SM_Coat_Rack')
    m = 'MI_Wood_Dark'
    b.lathe([(0, 0.08), (0.03, 0.08), (0.025, 0.2), (0.02, 1.0), (0.024, 1.6), (0.03, 1.66), (0.03, 1.7), (0.02, 1.72),
             (0, 1.72)], mat=m, segs=12)
    b.sphere(0.035, (0, 0, 1.75), m, segs=12, rings=8)
    for k in range(3):
        a = math.radians(k * 120 + 30)
        b.tube([(0, 0, 0.2), (math.cos(a) * 0.12, math.sin(a) * 0.12, 0.08), (math.cos(a) * 0.23, math.sin(a) * 0.23,
                                                                              0.012)], 0.018, m, segs=8, bend=0.08)
        b.sphere(0.02, (math.cos(a) * 0.235, math.sin(a) * 0.235, 0.014), m, scale=(1, 1, 0.7), segs=8, rings=5)
    for k in range(6):
        a = math.radians(k * 60 + 15)
        z = 1.62 if k % 2 else 1.5
        c, s = math.cos(a), math.sin(a)
        b.tube([(c * 0.02, s * 0.02, z - 0.03), (c * 0.09, s * 0.09, z + 0.02), (c * 0.12, s * 0.12, z + 0.07)], 0.009,
               'MI_Brass', segs=6, bend=0.03)
        b.sphere(0.013, (c * 0.122, s * 0.122, z + 0.075), 'MI_Brass', segs=8, rings=5)
    from props import _garment
    coat = _garment('coat', 'MI_Fabric_Brown', 0.95, shoulders=0.36)
    _attach(b, coat, (0.0, -0.16, 1.6), (6, 0, 0))
    b.tube([(-0.05, -0.08, 1.58), (0.0, -0.12, 1.55), (0.05, -0.08, 1.58)], 0.012, 'MI_Fabric_Brown', segs=6, bend=0.03)
    hat = Builder('hat')
    hat.lathe([(0, 0), (0.15, 0.0), (0.155, 0.006), (0.1, 0.012), (0.095, 0.1), (0.07, 0.12), (0, 0.11)],
              mat='MI_Fabric_Grey', segs=20, scale=(1, 0.85, 1))
    hat.torus(0.097, 0.012, (0, 0, 0.03), 'MI_Leather_Black', segs=20, rsegs=4, scale=(1, 0.85, 0.6))
    c, s = math.cos(math.radians(75)), math.sin(math.radians(75))
    _attach(b, hat, (c * 0.1, s * 0.12, 1.58), (20, 0, 0))
    return _recentre(b, z0=0.0, x=False, y=False)


@wprop('SM_Picture_Frame', 'Props/House', 'none', 'house', kind='wall', size=(0.5, 0.03, 0.4))
def picture_frame():
    b = Builder('SM_Picture_Frame')
    w, h = 0.5, 0.4
    _frame_ring(b, w, h, w - 0.04, h - 0.04, -0.03, 0.0, 'MI_Wood_Dark', cz=h / 2, bevel=0.003)
    _frame_ring(b, w - 0.03, h - 0.03, w - 0.048, h - 0.048, -0.024, 0.0, 'MI_Gold', cz=h / 2)
    b.plane(w - 0.046, h - 0.046, (0, -0.012, h / 2), 'MI_Img_Photo_Family')
    b.box((w - 0.03, 0.01, h - 0.03), (0, -0.005, h / 2), 'MI_Cardboard')
    return b


@wprop('SM_Painting_Large', 'Props/House', 'box', 'house', kind='wall', size=(1.0, 0.05, 0.75))
def painting_large():
    b = Builder('SM_Painting_Large')
    w, h = 1.0, 0.75
    m = 'MI_Wood_Dark'
    _frame_ring(b, w, h, w - 0.05, h - 0.05, -0.05, 0.0, m, cz=h / 2, bevel=0.005)
    _frame_ring(b, w - 0.04, h - 0.04, w - 0.1, h - 0.1, -0.038, 0.0, m, cz=h / 2, bevel=0.006)
    _frame_ring(b, w - 0.096, h - 0.096, w - 0.116, h - 0.116, -0.042, 0.0, 'MI_Gold', cz=h / 2, bevel=0.002)
    b.plane(w - 0.112, h - 0.112, (0, -0.024, h / 2), 'MI_Img_Painting_Landscape')
    b.box((w - 0.1, 0.02, h - 0.1), (0, -0.012, h / 2), 'MI_Fabric_Shade')
    return b


@wprop('SM_Painting_Lore', 'Props/House', 'box', 'house', kind='wall', size=(0.9, 0.07, 1.15))
def painting_lore():
    """The cursed portrait: heavy carved and gilded frame with corner cartouches, bead and
    egg-and-dart mouldings and a crest on top."""
    b = Builder('SM_Painting_Lore')
    g = 'MI_Gold'
    w, h = 0.9, 1.08
    cz = h / 2
    _frame_ring(b, w, h, w - 0.03, h - 0.03, -0.045, 0.0, g, cz=cz, bevel=0.006)
    _frame_ring(b, w - 0.025, h - 0.025, w - 0.1, h - 0.1, -0.06, 0.0, g, cz=cz, bevel=0.012)
    _frame_ring(b, w - 0.095, h - 0.095, w - 0.15, h - 0.15, -0.042, 0.0, g, cz=cz, bevel=0.008)
    _frame_ring(b, w - 0.145, h - 0.145, w - 0.165, h - 0.165, -0.05, 0.0, g, cz=cz, bevel=0.003)
    b.plane(w - 0.16, h - 0.16, (0, -0.03, cz), 'MI_Img_Painting_Lore')
    b.box((w - 0.15, 0.03, h - 0.15), (0, -0.015, cz), 'MI_Fabric_Shade')
    # Egg-and-dart on the cove, beads on the sight edge.
    for side in range(4):
        horiz = side % 2 == 0
        length = (w if horiz else h) - 0.13
        n = int(length / 0.055)
        for k in range(n):
            t = (k + 0.5) / n - 0.5
            off = ((h if horiz else w) - 0.12) / 2
            if horiz:
                x, z = t * length, cz + (off if side == 0 else -off)
            else:
                x, z = (off if side == 1 else -off), cz + t * length
            b.sphere(0.013, (x, -0.058, z), g, scale=(1.0 if horiz else 0.7, 0.45, 0.7 if horiz else 1.0), segs=6,
                     rings=4)
        length2 = (w if horiz else h) - 0.155
        n2 = int(length2 / 0.034)
        for k in range(n2):
            t = (k + 0.5) / n2 - 0.5
            off = ((h if horiz else w) - 0.155) / 2
            if horiz:
                x, z = t * length2, cz + (off if side == 0 else -off)
            else:
                x, z = (off if side == 1 else -off), cz + t * length2
            b.sphere(0.0055, (x, -0.051, z), g, segs=6, rings=3)
    # Carved corner cartouches and a crest.
    for sx in (-1, 1):
        for sz in (-1, 1):
            cx, cc = sx * (w / 2 - 0.035), cz + sz * (h / 2 - 0.035)
            _blob(b, 0.045, (cx, -0.055, cc), g, scale=(1, 0.4, 1), subdiv=2, amp=0.3, freq=2.5, seed=sx * 5 + sz)
            for k in range(3):
                a = math.radians(45 + sx * 0 + k * 30) if sx > 0 else math.radians(135 - k * 30)
                a = a if sz > 0 else -a
                _blob(b, 0.022, (cx - sx * math.cos(abs(a)) * 0.06 * (k + 1) * 0.6,
                                 -0.052, cc - sz * math.sin(abs(a)) * 0.06 * (k + 1) * 0.6), g, scale=(1, 0.45, 0.7),
                      subdiv=1, amp=0.3, seed=k + sx + sz * 3)
    crest = [(-0.17, 0), (0.17, 0), (0.12, 0.04), (0.06, 0.05), (0.0, 0.085), (-0.06, 0.05), (-0.12, 0.04)]
    b.prism(crest, 0.03, (0, -0.03, h - 0.005), g, axis='Y', bevel=0.008)
    _blob(b, 0.035, (0, -0.05, h + 0.03), g, scale=(1.2, 0.5, 1), subdiv=2, amp=0.3, freq=2.5, seed=9)
    for sx in (-1, 1):
        b.torus(0.03, 0.007, (sx * 0.1, -0.045, h + 0.02), g, rot=(90, 0, 0), segs=12, rsegs=5, arc=270)
    return _recentre(b, x=False, y=False)


def _curtains(name, mat, heavy=False):
    """Rod with finials on wall brackets (back at y = 0) and two gathered panels, 1.6 x 2.0."""
    b = Builder(name)
    W, H = 1.6, 2.0
    rz, ry = H - 0.04, -0.09
    rod_mat = 'MI_Wood_Dark' if heavy else 'MI_Brass'
    b.cyl(0.016 if heavy else 0.011, W - 0.11, (0, ry, rz), rod_mat, rot=(0, 90, 0), segs=12)
    for sx in (-1, 1):
        b.lathe([(0, 0), (0.02, 0.0), (0.03, 0.02), (0.022, 0.04), (0.008, 0.05), (0, 0.052)],
                (sx * (W / 2 - 0.055), ry, rz), rod_mat, rot=(0, sx * 90, 0), segs=12)
        b.cyl(0.006, -ry, (sx * (W / 2 - 0.12), ry / 2, rz), rod_mat, rot=(90, 0, 0), segs=8)
        b.cyl(0.02, 0.006, (sx * (W / 2 - 0.12), -0.003, rz), rod_mat, rot=(90, 0, 0), segs=10)
    tie = 0.95 if heavy else 1.05
    for sx in (-1, 1):
        outer = sx * (W / 2 - 0.07)
        inner = sx * (0.12 if heavy else 0.32)
        folds = 8 if heavy else 7

        def fn(u, v, outer=outer, inner=inner, sx=sx):
            z = 0.01 + (rz - 0.05) * v
            top_w = abs(inner - outer)
            if v > tie / H:
                k = (v - tie / H) / (1 - tie / H)
                width = 0.18 + (top_w - 0.18) * (k ** 0.8)
            else:
                k = 1 - v / (tie / H)
                width = 0.18 + (top_w * (0.75 if heavy else 0.6) - 0.18) * (k ** 1.6)
            x = outer + (inner - outer) * u * width / top_w
            gather = 1.0 - 0.6 * (abs(v - tie / H) < 0.04)
            amp = (0.035 if heavy else 0.028) * (0.6 + 0.9 * (1 - width / top_w)) * gather
            y = ry + 0.035 - amp * (1 + math.sin(2 * math.pi * folds * u + sx * 0.7))
            return (x, y, z)
        _sheet(b, fn, folds * 6, 12, mat, thickness=0.004, size=(abs(inner - outer), rz))
        b.torus(0.07, 0.012, (outer + sx * -0.05, ry + 0.0, tie), mat if heavy else 'MI_Fabric_Curtain_Light',
                rot=(0, 0, 0), segs=14, rsegs=5, scale=(1.3, 0.8, 0.5))
        b.cyl(0.015, 0.03, (outer, -0.015, tie), 'MI_Brass', rot=(90, 0, 0), segs=10)
        n_rings = 10
        for k in range(n_rings):
            x = outer + (inner - outer) * k / (n_rings - 1)
            b.torus(0.022 if heavy else 0.016, 0.003, (x, ry, rz), rod_mat, rot=(0, 90, 0), segs=8, rsegs=3)
    if heavy:
        def val(u, v):
            x = -W / 2 + 0.02 + (W - 0.04) * u
            sag = 0.12 * math.sin(math.pi * ((u * 2) % 1.0))
            z = rz + 0.05 - (0.25 + sag) * (1 - v)
            return (x, ry - 0.045 - 0.015 * math.sin(v * math.pi) - 0.01 * math.sin(u * 60), z)
        _sheet(b, val, 32, 4, mat, thickness=0.004, size=(W, 0.3))
    return b


@wprop('SM_Curtains_Window', 'Props/House', 'none', 'house', kind='wall', size=(1.6, 0.15, 2.0))
def curtains_window():
    return _curtains('SM_Curtains_Window', 'MI_Fabric_Curtain_Light')


@wprop('SM_Curtains_Heavy', 'Props/House', 'none', 'house', kind='wall', size=(1.6, 0.15, 2.0))
def curtains_heavy():
    return _curtains('SM_Curtains_Heavy', 'MI_Fabric_Curtain', heavy=True)


@wprop('SM_Ceiling_Light_Home', 'Props/House', 'none', 'house', kind='ceiling', size=(0.35, 0.35, 0.18))
def ceiling_light_home():
    """Ceiling mount at the origin; bulb at (0, 0, -0.09)."""
    b = Builder('SM_Ceiling_Light_Home')
    b.lathe([(0, 0), (0.09, 0.0), (0.095, -0.008), (0.08, -0.018), (0, -0.02)], mat='MI_Brass', segs=24)
    b.torus(0.172, 0.006, (0, 0, -0.03), 'MI_Brass', segs=32, rsegs=5)
    b.cyl(0.172, 0.012, (0, 0, -0.024), 'MI_Brass', segs=32, caps=False)

    def dome(u, v, r=0.17, depth=0.13, inner=False):
        a = (-1 if inner else 1) * 2 * math.pi * u
        t = v * math.pi / 2
        rr = (r - (0.003 if inner else 0)) * math.cos(t)
        return (rr * math.cos(a), rr * math.sin(a), -0.03 - (depth - (0.003 if inner else 0)) * math.sin(t))
    _sheet(b, lambda u, v: dome(u, 1 - v), 28, 8, 'MI_Glass_Dirty')
    _sheet(b, lambda u, v: dome(u, 1 - v, inner=True), 28, 8, 'MI_Emit_Shade')
    b.sphere(0.035, (0, 0, -0.09), 'MI_Emit_Bulb', segs=10, rings=6)
    b.lathe([(0, 0), (0.012, 0.0), (0.016, -0.015), (0.008, -0.025), (0, -0.026)], (0, 0, -0.158), 'MI_Brass', segs=12)
    return b


@wprop('SM_Wall_Sconce', 'Props/House', 'none', 'house', kind='wall', size=(0.16, 0.22, 0.34))
def wall_sconce():
    """Wall mount (back on y = 0); bulb at (0, -0.15, 0.24)."""
    b = Builder('SM_Wall_Sconce')
    b.prism(_ellipse(0.05, 0.08, 20), 0.012, (0, -0.006, 0.08), 'MI_Brass', axis='Y', bevel=0.003)
    b.sphere(0.014, (0, -0.016, 0.08), 'MI_Brass', segs=10, rings=6)
    b.tube([(0, -0.016, 0.08), (0, -0.09, 0.05), (0, -0.15, 0.1), (0, -0.15, 0.15)], 0.007, 'MI_Brass', segs=8,
           bend=0.04)
    b.lathe([(0, 0), (0.028, 0.0), (0.032, 0.012), (0.014, 0.022), (0.014, 0.05), (0, 0.05)], (0, -0.15, 0.15),
            'MI_Brass', segs=14)
    b.sphere(0.022, (0, -0.15, 0.225), 'MI_Emit_Bulb', scale=(1, 1, 1.3), segs=10, rings=6)
    _frustum_shell(b, 0.075, 0.0, 0.045, 0.13, 'MI_Fabric_Shade', 'MI_Emit_Shade', segs=20, loc=(0, -0.15, 0.2))
    return _recentre(b, z0=0.0, x=True, y=False)


@wprop('SM_Doormat', 'Props/House', 'none', 'house', size=(0.8, 0.5, 0.015))
def doormat():
    b = Builder('SM_Doormat')
    b.box((0.8, 0.5, 0.008), (0, 0, 0.004), 'MI_Rubber_Black', bevel=0.003)
    b.box((0.74, 0.44, 0.012), (0, 0, 0.007), 'MI_Fabric_Brown', bevel=0.002)
    for k in range(2):
        s = 0.06 + k * 0.05
        b.box((0.74 - 2 * s, 0.012, 0.002), (0, 0.22 - s, 0.0135), 'MI_Leather_Black')
        b.box((0.74 - 2 * s, 0.012, 0.002), (0, -0.22 + s, 0.0135), 'MI_Leather_Black')
        b.box((0.012, 0.44 - 2 * s, 0.002), (0.37 - s, 0, 0.0135), 'MI_Leather_Black')
        b.box((0.012, 0.44 - 2 * s, 0.002), (-0.37 + s, 0, 0.0135), 'MI_Leather_Black')
    return b


@wprop('SM_Rug_Large', 'Props/House', 'none', 'house', size=(2.4, 1.7, 0.01))
def rug_large():
    b = Builder('SM_Rug_Large')
    w, d = 2.3, 1.7
    b.box((w, d, 0.008), (0, 0, 0.004), 'MI_Fabric_Rug', bevel=0.003)
    for inset, mat, t in ((0.06, 'MI_Fabric_Navy', 0.09), (0.2, 'MI_Fabric_Shade', 0.025), (0.25, 'MI_Fabric_Navy', 0.02)):
        ow, od = w - 2 * inset, d - 2 * inset
        for sy in (-1, 1):
            b.box((ow, t, 0.001), (0, sy * (od / 2 - t / 2), 0.0085), mat)
        for sx in (-1, 1):
            b.box((t, od - 2 * t, 0.001), (sx * (ow / 2 - t / 2), 0, 0.0085), mat)
    b.prism(_ellipse(0.42, 0.3, 28), 0.001, (0, 0, 0.0085), 'MI_Fabric_Navy', axis='Z')
    b.prism(_ellipse(0.32, 0.22, 24), 0.001, (0, 0, 0.0092), 'MI_Fabric_Shade', axis='Z')
    b.prism(_ellipse(0.2, 0.13, 20), 0.001, (0, 0, 0.0098), 'MI_Fabric_Rug', axis='Z')
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.prism([(0, 0.0), (0.16, 0.0), (0.0, 0.12)], 0.001, (sx * (w / 2 - 0.3), sy * (d / 2 - 0.3), 0.0088),
                    'MI_Fabric_Navy', rot=(0, 0, 0 if sx > 0 and sy > 0 else 90 if sy > 0 else 270 if sx > 0 else 180),
                    axis='Z')
        for i in range(30):
            b.box((0.05, 0.012, 0.003), (sx * (w / 2 + 0.024), (i - 14.5) * 0.054, 0.0015), 'MI_Fabric_Shade')
    return b


# =======================================================================================
# 3. TOWN STREET FURNITURE (Props/Town)

def _face_uv(b, pts, y, mat, rect, flip=False, rot_z=0.0):
    """Flat ngon in the XZ plane at depth y facing -Y (or +Y with flip) whose bounding 'rect'
    (x0, z0, x1, z1) maps onto UV 0..1 (image top = +Z, reads correctly from its front)."""
    x0, z0, x1, z1 = rect
    m = _mat4((0, 0, 0), (0, 0, rot_z))
    pb, uv = b._begin()
    order = list(reversed(pts)) if flip else list(pts)
    vs = [pb.verts.new(m @ Vector((x, y, z))) for x, z in order]
    f = pb.faces.new(vs)
    for loop, (x, z) in zip(f.loops, order):
        u = (x - x0) / (x1 - x0)
        loop[uv].uv = ((1 - u) if flip else u, (z - z0) / (z1 - z0))
    b._commit(pb, mat, smooth=False)
    return b


def _octagon(r_flat, cz):
    R = r_flat / math.cos(math.radians(22.5))
    return [(R * math.cos(math.radians(22.5 + 45 * k)), cz + R * math.sin(math.radians(22.5 + 45 * k))) for k in range(8)]


def _sign_post(b, h, mat='MI_Steel', w=0.05):
    """Square perforated U-channel style post, bottom at z = 0."""
    b.box((w, w, h), (0, 0, h / 2), mat, bevel=0.004)
    for k in range(int(h / 0.1) - 1):
        b.box((0.012, 0.004, 0.012), (0, -w / 2 - 0.0005, 0.08 + k * 0.1), 'MI_Metal_Black')


@wprop('SM_Bin_Wheelie', 'Props/Town', 'box', 'town', size=(0.6, 0.7, 1.05))
def bin_wheelie():
    """Front (-Y) faces the street; hinge, handle and wheels at the back."""
    b = Builder('SM_Bin_Wheelie')
    m = 'MI_Plastic_Black'
    rings = [(0, 0, 0.05), (0.22, 0.25, 0.05), (0.235, 0.265, 0.07), (0.27, 0.31, 0.9), (0.278, 0.318, 0.93),
             (0.278, 0.318, 0.96), (0.0, 0.0, 0.96)]
    _sloft(b, rings, m, n=6, segs=32, loc=(0, -0.01, 0))
    for z in (0.3, 0.6):
        _sloft(b, [(0.241 + z * 0.04, 0.271 + z * 0.044, z - 0.012), (0.246 + z * 0.04, 0.276 + z * 0.044, z),
                   (0.241 + z * 0.04, 0.271 + z * 0.044, z + 0.012)], m, n=6, segs=32, loc=(0, -0.01, 0))
    b.box((0.6, 0.66, 0.035), (0, -0.0, 0.98), m, bevel=0.012)
    b.box((0.56, 0.05, 0.03), (0, -0.33, 0.965), m, bevel=0.01)
    b.box((0.2, 0.004, 0.12), (0, -0.333, 0.62), 'MI_Paper', rot=(-2.5, 0, 0))
    b.box((0.08, 0.04, 0.04), (0, -0.33, 0.95), m, bevel=0.01)
    b.cyl(0.018, 0.56, (0, 0.33, 0.93), m, rot=(0, 90, 0), segs=10)
    for sx in (-1, 1):
        b.box((0.04, 0.06, 0.08), (sx * 0.25, 0.32, 0.92), m, bevel=0.01)
        b.cyl(0.1, 0.05, (sx * 0.255, 0.24, 0.1), 'MI_Rubber_Black', rot=(0, 90, 0), segs=18)
        b.cyl(0.055, 0.052, (sx * 0.255, 0.24, 0.1), 'MI_Plastic_Grey', rot=(0, 90, 0), segs=12)
    b.cyl(0.015, 0.56, (0, 0.24, 0.1), 'MI_Steel', rot=(0, 90, 0), segs=8)
    b.box((0.44, 0.08, 0.05), (0, -0.2, 0.025), m, bevel=0.01)
    return _recentre(b)


@wprop('SM_Bin_Metal', 'Props/Town', 'convex', 'town', size=(0.55, 0.55, 0.75))
def bin_metal():
    b = Builder('SM_Bin_Metal')
    prof = [(0, 0.01), (0.2, 0.0), (0.22, 0.02), (0.22, 0.04)]
    for z in (0.12, 0.3, 0.48):
        prof += [(0.228, z - 0.02), (0.236, z), (0.228, z + 0.02)]
    prof += [(0.245, 0.62), (0.252, 0.63), (0.252, 0.645), (0.0, 0.645)]
    b.lathe(prof, mat='MI_Steel', segs=28)
    b.lathe([(0, 0.71), (0.08, 0.705), (0.2, 0.67), (0.26, 0.645), (0.27, 0.635), (0.262, 0.628), (0.0, 0.64)],
            mat='MI_Steel', segs=28)
    b.torus(0.045, 0.008, (0, 0, 0.715), 'MI_Steel', rot=(90, 0, 0), segs=12, rsegs=5, arc=180)
    for sx in (-1, 1):
        b.torus(0.05, 0.007, (sx * 0.24, 0, 0.53), 'MI_Steel', rot=(90, 90 * sx, 90), segs=12, rsegs=5, arc=180)
        b.box((0.012, 0.08, 0.03), (sx * 0.243, 0, 0.5), 'MI_Steel', bevel=0.003)
    return b


@wprop('SM_Mailbox_Post', 'Props/Town', 'box', 'town', size=(0.25, 0.5, 1.3))
def mailbox_post():
    """Door faces -Y (the street); flag on the +X side."""
    b = Builder('SM_Mailbox_Post')
    b.box((0.09, 0.09, 1.1), (0, 0.05, 0.55), 'MI_Wood_Raw', bevel=0.006)
    b.box((0.16, 0.42, 0.025), (0, -0.0, 1.1125), 'MI_Wood_Raw', bevel=0.004)
    b.box((0.04, 0.2, 0.025), (0, 0.11, 1.07), 'MI_Wood_Raw', rot=(40, 0, 0), bevel=0.003)
    prof = [(-0.085, 0.0), (0.085, 0.0), (0.085, 0.1)]
    for k in range(1, 12):
        a = math.pi * k / 12
        prof.append((0.085 * math.cos(a), 0.1 + 0.085 * math.sin(a)))
    prof.append((-0.085, 0.1))
    b.prism(prof, 0.46, (0, -0.0, 1.125), 'MI_Metal_Black', axis='Y', bevel=0.004)
    b.prism([(x * 1.06, z * 1.04) for x, z in prof], 0.012, (0, -0.236, 1.122), 'MI_Metal_Black', axis='Y', bevel=0.003)
    b.box((0.02, 0.03, 0.03), (0, -0.25, 1.3), 'MI_Metal_Black', bevel=0.006)
    b.box((0.01, 0.12, 0.03), (0.092, 0.08, 1.2), 'MI_Metal_Black', bevel=0.002)
    b.cyl(0.008, 0.01, (0.095, 0.08, 1.2), 'MI_Steel', rot=(0, 90, 0), segs=8)
    b.box((0.006, 0.03, 0.17), (0.1, 0.08, 1.26), 'MI_Metal_Red', bevel=0.002)
    b.box((0.006, 0.09, 0.05), (0.1, 0.05, 1.33), 'MI_Metal_Red', bevel=0.002)
    b.box((0.003, 0.2, 0.05), (-0.088, -0.05, 1.17), 'MI_Paper')
    return _recentre(b)


@wprop('SM_Mailbox_Public', 'Props/Town', 'box', 'town', size=(0.5, 0.55, 1.25))
def mailbox_public():
    b = Builder('SM_Mailbox_Public')
    m = 'MI_Metal_Blue'
    w, d = 0.5, 0.5
    prof = [(-d / 2, 0.16), (d / 2, 0.16), (d / 2, 1.0)]
    for k in range(1, 14):
        a = math.pi * k / 14
        prof.append((d / 2 * math.cos(a), 1.0 + 0.25 * math.sin(a)))
    prof.append((-d / 2, 1.0))
    b.prism([(-y, z) for y, z in prof], w, (0, 0, 0), m, axis='X', bevel=0.01)
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.box((0.05, 0.05, 0.17), (sx * (w / 2 - 0.04), sy * (d / 2 - 0.04), 0.085), m, bevel=0.006)
            b.box((0.09, 0.09, 0.01), (sx * (w / 2 - 0.04), sy * (d / 2 - 0.04), 0.005), m, bevel=0.003)
    b.box((w - 0.04, d - 0.04, 0.03), (0, 0, 0.17), m, bevel=0.006)
    # Pull-down chute with handle, access door, info plate, lock.
    b.box((0.4, 0.03, 0.17), (0, -d / 2 - 0.012, 0.9), m, rot=(-8, 0, 0), bevel=0.008)
    b.tube([(-0.12, -d / 2 - 0.03, 0.96), (-0.1, -d / 2 - 0.06, 0.96), (0.1, -d / 2 - 0.06, 0.96),
            (0.12, -d / 2 - 0.03, 0.96)], 0.01, m, segs=8, bend=0.015)
    b.box((0.36, 0.006, 0.006), (0, -d / 2 - 0.002, 0.8), 'MI_Metal_Black')
    _frame_ring(b, 0.42, 0.5, 0.40, 0.48, -d / 2 - 0.004, -d / 2 + 0.002, m, cz=0.48)
    b.box((0.18, 0.003, 0.12), (0, -d / 2 - 0.004, 0.62), 'MI_Paper')
    b.box((0.2, 0.002, 0.14), (0, -d / 2 - 0.002, 0.62), 'MI_Plastic_White')
    b.cyl(0.015, 0.012, (0.14, -d / 2 - 0.005, 0.4), 'MI_Chrome', rot=(90, 0, 0), segs=12)
    return _recentre(b)


@wprop('SM_Hydrant', 'Props/Town', 'convex', 'town', size=(0.35, 0.35, 0.75))
def hydrant():
    b = Builder('SM_Hydrant')
    red, cap = 'MI_Metal_Red', 'MI_Metal_Yellow'
    b.lathe([(0, 0), (0.13, 0), (0.13, 0.04), (0.11, 0.05), (0.095, 0.08), (0.09, 0.5), (0.11, 0.52), (0.11, 0.56),
             (0.0, 0.56)], mat=red, segs=24)
    for k in range(8):
        a = 2 * math.pi * k / 8
        b.cyl(0.011, 0.016, (math.cos(a) * 0.115, math.sin(a) * 0.115, 0.045), 'MI_Steel', segs=6)
        b.cyl(0.009, 0.012, (math.cos(a) * 0.1, math.sin(a) * 0.1, 0.565), 'MI_Steel', segs=6)
    b.lathe([(0, 0.555), (0.1, 0.555), (0.098, 0.6), (0.08, 0.655), (0.04, 0.68), (0, 0.685)], mat=cap, segs=24)
    b.cyl(0.03, 0.04, (0, 0, 0.7), cap, segs=5)
    b.cyl(0.04, 0.008, (0, 0, 0.684), cap, segs=16)
    for sx in (-1, 1):
        b.cyl(0.04, 0.08, (sx * 0.12, 0, 0.4), red, rot=(0, 90, 0), segs=16)
        b.cyl(0.046, 0.03, (sx * 0.165, 0, 0.4), cap, rot=(0, 90, 0), segs=16)
        b.cyl(0.02, 0.02, (sx * 0.19, 0, 0.4), cap, rot=(0, 90, 0), segs=5)
    b.cyl(0.06, 0.07, (0, -0.12, 0.33), red, rot=(90, 0, 0), segs=18)
    b.cyl(0.066, 0.03, (0, -0.165, 0.33), cap, rot=(90, 0, 0), segs=18)
    b.cyl(0.025, 0.018, (0, -0.186, 0.33), cap, rot=(90, 0, 0), segs=5)
    _chain(b, [(0.12, -0.04, 0.38), (0.11, -0.1, 0.27), (0.04, -0.15, 0.3)], link=0.02, wire=0.0025, mat='MI_Steel')
    return b


@wprop('SM_Sign_Stop', 'Props/Town', 'box', 'town', size=(0.75, 0.08, 2.4))
def sign_stop():
    """Steel post, octagonal sign facing -Y; sign centre at z = 2.025."""
    b = Builder('SM_Sign_Stop')
    _sign_post(b, 2.36)
    cz = 2.4 - 0.375
    octo = _octagon(0.375, cz)
    b.prism(octo, 0.003, (0, -0.03, 0), 'MI_Aluminium', axis='Y')
    _face_uv(b, octo, -0.0316, 'MI_Img_Sign_Stop', (-0.375, cz - 0.375, 0.375, cz + 0.375))
    for dz in (-0.33, 0.33):
        b.cyl(0.008, 0.045, (0, -0.012, cz + dz), 'MI_Steel', rot=(90, 0, 0), segs=8)
        b.cyl(0.011, 0.004, (0, -0.034, cz + dz), 'MI_Steel', rot=(90, 0, 0), segs=8)
    return b


@wprop('SM_Sign_Speed', 'Props/Town', 'box', 'town', size=(0.6, 0.08, 2.4))
def sign_speed():
    b = Builder('SM_Sign_Speed')
    _sign_post(b, 2.36)
    w, h = 0.6, 0.75
    cz = 2.4 - h / 2
    pts = [(x, z + cz) for x, z in _rrect(w, h, 0.03, 3)]
    b.prism(pts, 0.003, (0, -0.03, 0), 'MI_Aluminium', axis='Y')
    _face_uv(b, pts, -0.0316, 'MI_Img_Sign_Speed', (-w / 2, cz - h / 2, w / 2, cz + h / 2))
    for dz in (-0.335, 0.335):
        b.cyl(0.008, 0.045, (0, -0.012, cz + dz), 'MI_Steel', rot=(90, 0, 0), segs=8)
        b.cyl(0.011, 0.004, (0, -0.034, cz + dz), 'MI_Steel', rot=(90, 0, 0), segs=8)
    return b


STREET_ROWS = {'MAIN': 0, 'ASHGROVE': 1, 'MARROW': 2, 'CHURCH': 3}


def _street_sign(name, cross):
    """Corner post 2.9 m tall; blade 1 along X reads MAIN ST (both faces), blade 2 along Y reads the
    cross street. Atlas T_Sign_StreetNames: 4 rows top to bottom, row k = V 1-(k+1)/4 .. 1-k/4."""
    b = Builder(name)
    b.lathe([(0, 0), (0.04, 0), (0.035, 0.03), (0.032, 2.62), (0.0, 2.62)], mat='MI_Metal_Green', segs=12)
    b.lathe([(0, 2.6), (0.04, 2.6), (0.04, 2.66), (0.0, 2.66)], mat='MI_Steel', segs=12)
    bl, bh, bt = 0.8, 0.1, 0.006
    for k, (row, z, along_y) in enumerate(((STREET_ROWS['MAIN'], 2.715, False), (STREET_ROWS[cross], 2.835, True))):
        v0, v1 = 1 - (row + 1) / 4, 1 - row / 4
        s = Builder('blade')
        s.box((bl, bt, bh), (0, 0, 0), 'MI_Aluminium', bevel=0.0015, segs=1)
        s.plane(bl, bh, (0, -bt / 2 - 0.0005, 0), 'MI_Img_Sign_Street', uv_rect=(0, v0, 1, v1))
        s.plane(bl, bh, (0, bt / 2 + 0.0005, 0), 'MI_Img_Sign_Street', rot=(0, 0, 180), uv_rect=(0, v0, 1, v1))
        for sz in (-1, 1):     # saddle clamps above and below the blade, clear of the lettering
            s.box((0.07, 0.03, 0.018), (0, 0, sz * (bh / 2 + 0.008)), 'MI_Steel', bevel=0.003)
        _attach(b, s, (0, 0, z), (0, 0, 90 if along_y else 0))
    b.cyl(0.018, 0.02, (0, 0, 2.775), 'MI_Steel', segs=10)
    b.sphere(0.02, (0, 0, 2.9 - 0.02), 'MI_Steel', segs=10, rings=6)
    return b


@wprop('SM_Sign_Street_Ashgrove', 'Props/Town', 'box', 'town', size=(0.8, 0.8, 2.9))
def sign_street_ashgrove():
    return _street_sign('SM_Sign_Street_Ashgrove', 'ASHGROVE')


@wprop('SM_Sign_Street_Marrow', 'Props/Town', 'box', 'town', size=(0.8, 0.8, 2.9))
def sign_street_marrow():
    return _street_sign('SM_Sign_Street_Marrow', 'MARROW')


@wprop('SM_Sign_Street_Church', 'Props/Town', 'box', 'town', size=(0.8, 0.8, 2.9))
def sign_street_church():
    return _street_sign('SM_Sign_Street_Church', 'CHURCH')


def _bench_end(b, x, iron='MI_Metal_Black'):
    b.tube([(x, 0.26, 0.0), (x, 0.23, 0.2), (x, 0.2, 0.43)], 0.022, iron, segs=8, bend=0.1)
    b.tube([(x, -0.25, 0.0), (x, -0.22, 0.2), (x, -0.2, 0.43), (x, -0.22, 0.6), (x, -0.12, 0.66), (x, 0.06, 0.64)],
           0.022, iron, segs=8, bend=0.06)
    b.tube([(x, -0.22, 0.43), (x, 0.2, 0.43), (x, 0.28, 0.6), (x, 0.32, 0.86)], 0.02, iron, segs=8, bend=0.08)
    b.torus(0.045, 0.012, (x, -0.11, 0.58), iron, rot=(0, 90, 0), segs=12, rsegs=5, arc=270)
    b.torus(0.04, 0.012, (x, 0.0, 0.25), iron, rot=(0, 90, 0), segs=12, rsegs=5, arc=300)
    for y in (-0.25, 0.26):
        b.sphere(0.035, (x, y, 0.02), iron, scale=(1, 1.6, 0.55), segs=10, rings=6)


@wprop('SM_Bench_Park', 'Props/Town', 'box', 'town', size=(1.8, 0.65, 0.88))
def bench_park():
    b = Builder('SM_Bench_Park')
    L = 1.8
    for sx in (-1, 1):
        _bench_end(b, sx * (L / 2 - 0.12))
    b.box((0.04, 0.04, 0.04), (0, 0.0, 0.41), 'MI_Metal_Black')
    b.tube([(0, -0.18, 0.41), (0, 0.18, 0.41), (0, 0.27, 0.62), (0, 0.3, 0.8)], 0.016, 'MI_Metal_Black', segs=8, bend=0.08)
    for k in range(5):
        y = -0.21 + k * 0.095
        b.box((L, 0.08, 0.035), (0, y, 0.455), 'MI_Wood_Planks', bevel=0.005, uv_offset=(k * 0.27, 0))
    for k in range(3):
        z = 0.56 + k * 0.11
        y = 0.24 + (z - 0.43) * 0.3
        b.box((L, 0.03, 0.085), (0, y + 0.015, z), 'MI_Wood_Planks', rot=(-17, 0, 0), bevel=0.005,
              uv_offset=(k * 0.41, 0.2))
    return _recentre(b)


@wprop('SM_BusStop', 'Props/Town', 'complex', 'town', size=(3.0, 1.4, 2.4))
def bus_stop():
    """Open side faces -Y (the road)."""
    b = Builder('SM_BusStop')
    fr = 'MI_Metal_Grey'
    W, D, H = 2.88, 1.28, 2.3
    xs = (-W / 2 + 0.04, 0.0, W / 2 - 0.04)
    for x in xs:
        b.box((0.07, 0.07, H), (x, D / 2 - 0.05, H / 2), fr, bevel=0.006)
    for sx in (-1, 1):
        b.box((0.07, 0.07, H), (sx * (W / 2 - 0.04), -D / 2 + 0.05, H / 2), fr, bevel=0.006)
        b.box((0.06, D - 0.1, 0.05), (sx * (W / 2 - 0.04), 0, H - 0.03), fr, bevel=0.004)
        b.box((0.05, D - 0.1, 0.05), (sx * (W / 2 - 0.04), 0, 0.15), fr, bevel=0.004)
    b.box((W - 0.1, 0.06, 0.05), (0, D / 2 - 0.05, H - 0.03), fr, bevel=0.004)
    b.box((W - 0.1, 0.05, 0.05), (0, D / 2 - 0.05, 0.15), fr, bevel=0.004)
    b.box((W - 0.1, 0.06, 0.05), (0, -D / 2 + 0.05, H - 0.03), fr, bevel=0.004)
    # Glass back and one glazed side, an advert panel on the other side.
    for x0, x1 in ((xs[0], xs[1]), (xs[1], xs[2])):
        b.box((x1 - x0 - 0.07, 0.012, H - 0.4), ((x0 + x1) / 2, D / 2 - 0.05, 0.18 + (H - 0.4) / 2 + 0.02),
              'MI_Glass_Dirty')
    b.box((0.012, D - 0.17, H - 0.4), (W / 2 - 0.04, 0, 0.2 + (H - 0.4) / 2), 'MI_Glass_Dirty')
    b.box((0.09, D - 0.17, 1.75), (-W / 2 + 0.04, 0, 0.18 + 0.875 + 0.02), fr, bevel=0.006)
    for sx in (-1, 1):
        s = Builder('ad')
        s.plane(1.0, 1.4, (0, -0.0465, 0), 'MI_Img_PosterMuseum')
        _attach(b, s, (-W / 2 + 0.04, 0, 1.08), (0, 0, 90 if sx > 0 else -90))
    # Roof with a slight fall to the back and a fascia.
    b.box((W + 0.1, D + 0.08, 0.04), (0, 0.0, H + 0.035), fr, rot=(1.5, 0, 0), bevel=0.006)
    b.box((W + 0.12, 0.04, 0.1), (0, -D / 2 - 0.04, H + 0.03), fr, bevel=0.006)
    b.box((W + 0.08, D + 0.04, 0.012), (0, 0.0, H + 0.062), 'MI_Glass_Dirty', rot=(1.5, 0, 0))
    b.box((0.9, 0.006, 0.07), (0.6, -D / 2 - 0.062, H + 0.03), 'MI_Plastic_White', bevel=0.002)
    # Bench along the back.
    b.box((1.8, 0.32, 0.04), (0.35, D / 2 - 0.3, 0.46), 'MI_Steel', bevel=0.006)
    for x in (-0.45, 0.35, 1.15):
        b.box((0.05, 0.25, 0.44), (x, D / 2 - 0.27, 0.22), fr, bevel=0.004)
    b.box((0.3, 0.004, 0.42), (0.6, D / 2 - 0.085, 1.45), 'MI_Paper')
    b.box((0.32, 0.01, 0.44), (0.6, D / 2 - 0.08, 1.45), fr, bevel=0.003)
    return b


@wprop('SM_UtilityPole', 'Props/Town', 'complex', 'town', size=(2.4, 0.6, 9.0))
def utility_pole():
    """Wooden pole; crossarm at 8.3 m along X with insulator tops at z = 8.47; transformer on -Y."""
    b = Builder('SM_UtilityPole')
    b.lathe([(0, 0), (0.14, 0), (0.135, 3.0), (0.115, 8.95), (0.08, 9.0), (0, 9.0)], mat='MI_Wood_Dark', segs=14)
    b.box((2.4, 0.09, 0.11), (0, 0.18, 8.3), 'MI_Wood_Dark', bevel=0.008)
    for sx in (-1, 1):
        b.box((0.9, 0.006, 0.04), (sx * 0.38, 0.14, 8.1), 'MI_Steel', rot=(0, sx * 28, 0))
    b.cyl(0.012, 0.36, (0, 0.05, 8.3), 'MI_Steel', rot=(90, 0, 0), segs=8)
    for x in (-1.1, -0.75, 0.75, 1.1):
        b.cyl(0.012, 0.1, (x, 0.18, 8.395), 'MI_Steel', segs=6)
        b.lathe([(0, 0.0), (0.045, 0.0), (0.05, 0.02), (0.035, 0.05), (0.042, 0.06), (0.03, 0.09), (0.018, 0.11),
                 (0, 0.115)], (x, 0.18, 8.37), 'MI_Glass_Bottle', segs=12)
    b.lathe([(0, 0.0), (0.045, 0.0), (0.05, 0.02), (0.035, 0.05), (0.042, 0.06), (0.03, 0.09), (0.018, 0.11),
             (0, 0.115)], (0, 0, 9.0), 'MI_Glass_Bottle', segs=12)
    # Transformer can on a bracket.
    b.box((0.08, 0.14, 0.8), (0, -0.16, 7.05), 'MI_Steel', bevel=0.006)
    b.lathe([(0, 0), (0.24, 0), (0.25, 0.02), (0.25, 0.68), (0.23, 0.72), (0.0, 0.74)], (0, -0.42, 6.7),
            'MI_Metal_Grey', segs=20)
    for k in range(3):
        b.box((0.012, 0.06, 0.5), (math.cos(k * 1.2 + 2.0) * 0.25, -0.42 + math.sin(k * 1.2 + 2.0) * 0.25, 6.98),
              'MI_Metal_Grey', rot=(0, 0, math.degrees(k * 1.2 + 2.0)))
    for dx in (-0.1, 0.1):
        b.lathe([(0, 0), (0.03, 0), (0.035, 0.02), (0.025, 0.03), (0.03, 0.05), (0.022, 0.06), (0.026, 0.08), (0, 0.09)],
                (dx, -0.42, 7.44), 'MI_Porcelain', segs=10)
        b.tube([(dx, -0.42, 7.53), (dx * 3, -0.3, 7.9), (dx * 8, 0.18, 8.36)], 0.006, 'MI_Cable', segs=5, bend=0.3)
    b.box((0.1, 0.002, 0.06), (0, -0.425, 7.1), 'MI_Paper')
    # Ground wire, step bolts, number tag.
    b.tube([(0.0, -0.13, 0.0), (0.0, -0.125, 3.0), (0.0, -0.11, 6.6)], 0.006, 'MI_Copper', segs=5)
    for k in range(10):
        z = 2.6 + k * 0.45
        sx = 1 if k % 2 else -1
        r = 0.135 - (z - 3.0) * 0.0034 if z > 3 else 0.135
        b.cyl(0.01, 0.2, (sx * (r + 0.07), 0, z), 'MI_Steel', rot=(0, 90, 0), segs=6)
    b.box((0.08, 0.004, 0.12), (0.0, -0.14, 2.1), 'MI_Aluminium', rot=(0, 0, 0), bevel=0.002)
    for k in range(3):
        b.box((0.05, 0.002, 0.018), (0.0, -0.143, 2.13 - k * 0.03), 'MI_Plastic_Black')
    return b


@wprop('SM_Manhole', 'Props/Town', 'none', 'town', size=(0.7, 0.7, 0.02))
def manhole():
    b = Builder('SM_Manhole')
    b.lathe([(0, 0), (0.35, 0), (0.35, 0.015), (0.33, 0.017), (0.0, 0.017)], mat='MI_Metal_Rust', segs=36)
    b.lathe([(0, 0.012), (0.315, 0.012), (0.315, 0.018), (0.305, 0.019), (0.0, 0.019)], mat='MI_Metal_Black', segs=36)
    for r in (0.08, 0.16, 0.24, 0.29):
        b.torus(r, 0.004, (0, 0, 0.019), 'MI_Metal_Black', segs=32, rsegs=4, scale=(1, 1, 0.5))
    for k in range(16):
        a = math.pi * k / 8
        b.box((0.2, 0.008, 0.002), (math.cos(a) * 0.19, math.sin(a) * 0.19, 0.019), 'MI_Metal_Black',
              rot=(0, 0, math.degrees(a)))
    for x in (-0.2, 0.2):
        b.cyl(0.012, 0.004, (x, 0.0, 0.018), 'MI_Plastic_Black', segs=10)
    return b


@wprop('SM_Storm_Drain', 'Props/Town', 'none', 'town', size=(0.9, 0.35, 0.02))
def storm_drain():
    """Curb inlet grate; the curb side is +Y."""
    b = Builder('SM_Storm_Drain')
    w, d = 0.9, 0.35
    b.box((w - 0.04, d - 0.04, 0.001), (0, 0, 0.0005), 'MI_Plastic_Black')
    for sy in (-1, 1):
        b.box((w, 0.03, 0.02), (0, sy * (d / 2 - 0.015), 0.01), 'MI_Metal_Black', bevel=0.003)
    for sx in (-1, 1):
        b.box((0.03, d, 0.02), (sx * (w / 2 - 0.015), 0, 0.01), 'MI_Metal_Black', bevel=0.003)
    for k in range(13):
        b.box((0.025, d - 0.05, 0.018), (-w / 2 + 0.075 + k * 0.0625, 0, 0.009), 'MI_Metal_Black', bevel=0.002)
    b.box((w - 0.05, 0.02, 0.016), (0, 0, 0.008), 'MI_Metal_Black')
    _blob(b, 0.08, (0.2, 0.1, 0.019), 'MI_Dirt', scale=(1.4, 0.6, 0.06), subdiv=1, amp=0.3, seed=5)
    return _recentre(b, z0=0.0, x=False, y=False)


@wprop('SM_Newspaper_Box', 'Props/Town', 'box', 'town', size=(0.45, 0.4, 1.0))
def newspaper_box():
    b = Builder('SM_Newspaper_Box')
    m = 'MI_Metal_Red'
    w, d = 0.45, 0.38
    b.box((0.06, 0.06, 0.4), (0, 0.0, 0.2), 'MI_Metal_Black', bevel=0.006)
    b.box((0.3, 0.3, 0.02), (0, 0.0, 0.01), 'MI_Metal_Black', bevel=0.004)
    b.box((w, d, 0.56), (0, 0, 0.4 + 0.28), m, bevel=0.012)
    b.box((w + 0.006, d + 0.006, 0.03), (0, 0, 0.965), m, bevel=0.008)
    b.box((w - 0.04, 0.012, 0.4), (0, -d / 2 - 0.006, 0.67), m, bevel=0.004)
    b.plane(w - 0.14, 0.22, (0, -d / 2 - 0.0125, 0.72), 'MI_Img_Newspaper', uv_rect=(0.05, 0.4, 0.95, 0.98))
    b.box((w - 0.12, 0.003, 0.24), (0, -d / 2 - 0.02, 0.72), 'MI_Glass')
    _frame_ring(b, w - 0.04, 0.3, w - 0.12, 0.24, -d / 2 - 0.026, -d / 2 - 0.012, m, cz=0.72, bevel=0.003)
    b.box((0.16, 0.025, 0.025), (0, -d / 2 - 0.035, 0.53), 'MI_Chrome', bevel=0.006)
    b.box((0.12, 0.04, 0.09), (0.13, -d / 2 - 0.035, 0.925), 'MI_Steel', bevel=0.008)
    b.box((0.02, 0.004, 0.035), (0.11, -d / 2 - 0.056, 0.935), 'MI_Plastic_Black')
    b.box((0.035, 0.01, 0.02), (0.15, -d / 2 - 0.058, 0.91), 'MI_Chrome', bevel=0.003)
    b.box((w - 0.08, 0.004, 0.08), (0, -d / 2 - 0.0005, 0.47), 'MI_Plastic_White')
    return _recentre(b)


@wprop('SM_Bollard', 'Props/Town', 'convex', 'town', size=(0.2, 0.2, 0.9))
def bollard():
    b = Builder('SM_Bollard')
    b.lathe([(0, 0), (0.1, 0), (0.1, 0.04), (0.085, 0.07), (0.075, 0.1), (0.075, 0.72), (0.085, 0.74), (0.085, 0.76),
             (0.07, 0.8), (0.06, 0.85), (0.035, 0.885), (0.0, 0.9)], mat='MI_Metal_Black', segs=20)
    b.cyl(0.0765, 0.05, (0, 0, 0.66), 'MI_Metal_Yellow', segs=20)
    return b


@wprop('SM_Traffic_Cone', 'Props/Town', 'convex', 'town', size=(0.3, 0.3, 0.7))
def traffic_cone():
    b = Builder('SM_Traffic_Cone')
    b.prism(_rrect(0.3, 0.3, 0.04), 0.03, (0, 0, 0.015), 'MI_Rubber_Black', axis='Z', bevel=0.006)
    prof = [(0.12, 0.03), (0.025, 0.7), (0.0, 0.7)]
    b.lathe([(0, 0.03)] + prof, mat='MI_Plastic_Red', segs=20)
    for z0, z1 in ((0.38, 0.46), (0.54, 0.6)):
        r0, r1 = 0.12 - 0.095 * (z0 - 0.03) / 0.67, 0.12 - 0.095 * (z1 - 0.03) / 0.67
        b.lathe([(r0 + 0.0012, z0), (r1 + 0.0012, z1)], mat='MI_Plastic_White', segs=20)
    return b


@wprop('SM_Planter_Box', 'Props/Town', 'box', 'town', size=(1.2, 0.5, 0.5))
def planter_box():
    b = Builder('SM_Planter_Box')
    rng = _rng('SM_Planter_Box')
    w, d, h = 1.2, 0.5, 0.45
    for k in range(3):
        z = 0.03 + k * 0.14 + 0.065
        for sy in (-1, 1):
            b.box((w - 0.08, 0.025, 0.13), (0, sy * (d / 2 - 0.0125), z), 'MI_Wood_Raw', bevel=0.003,
                  uv_offset=(k * 0.3, sy))
        for sx in (-1, 1):
            b.box((0.025, d - 0.05, 0.13), (sx * (w / 2 - 0.0525), 0, z), 'MI_Wood_Raw', bevel=0.003,
                  uv_offset=(k * 0.2, sx))
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.box((0.07, 0.07, h + 0.03), (sx * (w / 2 - 0.035), sy * (d / 2 - 0.035), (h + 0.03) / 2), 'MI_Wood_Raw',
                  bevel=0.005)
    for sy in (-1, 1):
        b.box((w, 0.07, 0.03), (0, sy * (d / 2 - 0.035), h + 0.045), 'MI_Wood_Raw', bevel=0.005)
    for sx in (-1, 1):
        b.box((0.07, d - 0.14, 0.03), (sx * (w / 2 - 0.035), 0, h + 0.045), 'MI_Wood_Raw', bevel=0.005)
    b.box((w - 0.1, d - 0.1, 0.02), (0, 0, h - 0.02), 'MI_Dirt')
    for k in range(5):
        x = -0.42 + k * 0.21 + rng.uniform(-0.03, 0.03)
        _blob(b, rng.uniform(0.08, 0.11), (x, rng.uniform(-0.05, 0.05), h - 0.01), 'MI_Hedge', scale=(1, 1, 0.8),
              subdiv=2, amp=0.35, freq=2.5, seed=k + 3, zmin=h - 0.01)
        for j in range(3):
            a = rng.uniform(0, 6.28)
            b.sphere(0.014, (x + math.cos(a) * 0.06, math.sin(a) * 0.06, h + 0.09 + rng.uniform(-0.02, 0.02)),
                     ('MI_Plastic_Red', 'MI_Plastic_Yellow', 'MI_Porcelain')[(k + j) % 3], segs=6, rings=4)
    return b


# --- Fences and hedges: segments span x = 0 .. length, centred on y = 0, tile along +X ----

def _picket(b, x, z0, h, w, t, y, mat, tip=0.06, rot_y=0.0):
    poly = [(-w / 2, 0), (w / 2, 0), (w / 2, h - tip), (0, h), (-w / 2, h - tip)]
    b.prism(poly, t, (x, y, z0), mat, rot=(0, rot_y, 0), axis='Y', bevel=0.003)


@wprop('SM_Fence_Picket', 'Props/Town', 'box', 'town', kind='fence', size=(2.0, 0.09, 1.0))
def fence_picket():
    b = Builder('SM_Fence_Picket')
    rng = _rng('SM_Fence_Picket')
    m = 'MI_Trim_White'
    b.box((0.09, 0.09, 0.98), (0, 0.0, 0.49), m, bevel=0.006)
    b.box((0.11, 0.11, 0.02), (0, 0.0, 0.99), m, bevel=0.004)
    b.prism([(-0.045, 0), (0.045, 0), (0, 0.025)], 0.09, (0, 0, 1.0 - 0.0), m, axis='Y')
    for z in (0.2, 0.74):
        b.box((2.0 - 0.045, 0.035, 0.08), (0.045 + (2.0 - 0.045) / 2, 0.0275, z), m, bevel=0.004)
    for k in range(16):
        x = 0.1 + k * 0.1225
        tilt = rng.uniform(-1.2, 1.2) if k != 11 else 7.0
        h = 0.92 + rng.uniform(-0.012, 0.012) if k != 11 else 0.86
        _picket(b, x, 0.04 if k != 11 else 0.07, h, 0.075, 0.02, 0.0, m, rot_y=tilt)
    return b


@wprop('SM_Fence_Picket_Gate', 'Props/Town', 'box', 'town', kind='gate', size=(1.0, 0.09, 1.0))
def fence_picket_gate():
    """Hinge at the origin, leaf along +X."""
    b = Builder('SM_Fence_Picket_Gate')
    m = 'MI_Trim_White'
    for z in (0.2, 0.74):
        b.box((0.96, 0.035, 0.08), (0.5, 0.0275, z), m, bevel=0.004)
    b.box((1.0, 0.03, 0.08), (0.5, 0.0275, 0.47), m, rot=(0, -31, 0), bevel=0.004)
    for k in range(8):
        _picket(b, 0.07 + k * 0.1225, 0.06, 0.92, 0.075, 0.02, 0.0, m)
    for z in (0.2, 0.74):
        b.box((0.16, 0.006, 0.035), (0.08, 0.047, z), 'MI_Metal_Black', bevel=0.002)
        b.cyl(0.012, 0.08, (0.0, 0.047, z), 'MI_Metal_Black', segs=8)
    b.box((0.012, 0.03, 0.09), (0.97, 0.05, 0.74), 'MI_Metal_Black', bevel=0.002)
    b.box((0.08, 0.008, 0.012), (0.95, 0.06, 0.76), 'MI_Metal_Black', bevel=0.002)
    return b


def _spear(b, x, z, mat='MI_Metal_Black'):
    b.lathe([(0, 0), (0.014, 0.0), (0.016, 0.012), (0.012, 0.02), (0.02, 0.035), (0.0, 0.075)], (x, 0, z), mat, segs=6)


@wprop('SM_Fence_Iron', 'Props/Town', 'box', 'town', kind='fence', size=(2.0, 0.34, 1.6))
def fence_iron():
    b = Builder('SM_Fence_Iron')
    iron = 'MI_Metal_Black'
    L = 2.0
    b.box_minmax((0.0, -0.15, 0.0), (L, 0.15, 0.27), 'MI_Stone_Wall', bevel=0.006)
    b.box_minmax((-0.01, -0.17, 0.27), (L + 0.0, 0.17, 0.32), 'MI_Concrete_Raw', bevel=0.008)
    b.box((0.07, 0.07, 1.2), (0.0, 0.0, 0.32 + 0.6), iron, bevel=0.006)
    b.lathe([(0, 0), (0.05, 0), (0.05, 0.015), (0.025, 0.03), (0.03, 0.05), (0.04, 0.08), (0.025, 0.11),
             (0.0, 0.13)], (0.0, 0, 1.47), iron, segs=10)
    for z in (0.38, 1.35):
        b.box((L - 0.035, 0.014, 0.035), (0.035 + (L - 0.035) / 2, 0, z), iron, bevel=0.003)
    n = 17
    for k in range(n):
        x = 0.1 + k * (L - 0.15) / (n - 1)
        b.cyl(0.009, 1.1, (x, 0, 0.32 + 0.55), iron, segs=6)
        _spear(b, x, 1.42)
        if k < n - 1:
            b.torus(0.03, 0.005, (x + (L - 0.15) / (n - 1) / 2, 0, 1.3), iron, rot=(90, 0, 0), segs=10, rsegs=4)
    return b


@wprop('SM_Fence_Iron_Gate', 'Props/Town', 'box', 'town', kind='gate', size=(1.2, 0.08, 1.6))
def fence_iron_gate():
    """Hinge at the origin, leaf along +X."""
    b = Builder('SM_Fence_Iron_Gate')
    iron = 'MI_Metal_Black'
    W = 1.2
    for x in (0.03, W - 0.03):
        b.box((0.04, 0.03, 1.42), (x, 0, 0.05 + 0.71), iron, bevel=0.004)
    for z in (0.1, 0.5, 1.25):
        b.box((W - 0.06, 0.016, 0.035), (W / 2, 0, z), iron, bevel=0.003)
    arch = [(0.03 + (W - 0.06) * t, 0, 1.45 + 0.12 * math.sin(math.pi * t)) for t in [k / 12 for k in range(13)]]
    b.tube(arch, 0.012, iron, segs=6)
    n = 10
    for k in range(n):
        x = 0.03 + (k + 0.5) * (W - 0.06) / n
        top = 1.45 + 0.12 * math.sin(math.pi * (x - 0.03) / (W - 0.06))
        b.cyl(0.009, top - 0.1, (x, 0, 0.1 + (top - 0.1) / 2), iron, segs=6)
        _spear(b, x, top - 0.01)
    for sx in (-1, 1):
        b.torus(0.07, 0.008, (W / 2 + sx * 0.15, 0, 1.15), iron, rot=(90, 0, 0), segs=14, rsegs=4, arc=270)
        b.torus(0.04, 0.007, (W / 2 + sx * 0.32, 0, 0.3), iron, rot=(90, 0, 0), segs=12, rsegs=4, arc=300)
    b.torus(0.06, 0.008, (W / 2, 0, 0.75), iron, rot=(90, 0, 0), segs=16, rsegs=4)
    for z in (0.25, 1.25):
        b.cyl(0.02, 0.12, (0.0, 0, z), iron, segs=10)
        b.box((0.05, 0.02, 0.025), (0.02, 0, z), iron, bevel=0.003)
    b.box((0.05, 0.025, 0.08), (W - 0.03, -0.02, 1.0), iron, bevel=0.004)
    b.tube([(W - 0.04, -0.04, 1.0), (W - 0.04, -0.06, 1.0), (W - 0.12, -0.06, 0.98)], 0.006, iron, segs=5, bend=0.01)
    return b


@wprop('SM_Fence_Privacy', 'Props/Town', 'box', 'town', kind='fence', size=(2.0, 0.12, 1.8))
def fence_privacy():
    """Weathered board fence; one board is snapped short."""
    b = Builder('SM_Fence_Privacy')
    rng = _rng('SM_Fence_Privacy')
    m = 'MI_Wood_Raw'
    b.box((0.1, 0.1, 1.86), (0, 0.0, 0.93), 'MI_Wood_Dark', bevel=0.006)
    for z in (0.25, 0.9, 1.55):
        b.box((2.0 - 0.05, 0.04, 0.09), (0.05 + (2.0 - 0.05) / 2, 0.03, z), m, bevel=0.004)
    n = 13
    bw = (2.0 - 0.05) / n
    for k in range(n):
        x = 0.05 + (k + 0.5) * bw
        h = 1.78 + rng.uniform(-0.015, 0.015)
        z0 = 0.03
        if k == 8:
            poly = [(-bw / 2 + 0.004, 0), (bw / 2 - 0.004, 0), (bw / 2 - 0.004, 0.95), (0.03, 1.02), (-0.01, 0.93),
                    (-bw / 2 + 0.004, 1.06)]
        else:
            poly = [(-bw / 2 + 0.004, 0), (bw / 2 - 0.004, 0), (bw / 2 - 0.004, h - 0.025), (bw / 2 - 0.03, h),
                    (-bw / 2 + 0.03, h), (-bw / 2 + 0.004, h - 0.025)]
        b.prism(poly, 0.018, (x, -0.0, z0), m, rot=(rng.uniform(-0.6, 0.6), rng.uniform(-0.5, 0.5), 0), axis='Y',
                bevel=0.002)
    return _recentre(b, x=False, y=True)


@wprop('SM_Fence_Chain', 'Props/Town', 'box', 'town', kind='fence', size=(3.0, 0.08, 1.5))
def fence_chain():
    b = Builder('SM_Fence_Chain')
    L, H = 3.0, 1.5
    b.cyl(0.03, H - 0.02, (0, 0, (H - 0.02) / 2), 'MI_Steel', segs=12)
    b.lathe([(0, 0), (0.034, 0), (0.034, 0.02), (0.02, 0.035), (0, 0.038)], (0, 0, H - 0.04), 'MI_Steel', segs=12)
    b.cyl(0.0175, L - 0.02, (L / 2, 0.0, H - 0.05), 'MI_Steel', rot=(0, 90, 0), segs=10)
    b.cyl(0.003, L, (L / 2, -0.03, 0.06), 'MI_Steel', rot=(0, 90, 0), segs=4)
    b.plane(L - 0.03, H - 0.08, (0.015 + (L - 0.03) / 2, -0.03, 0.03 + (H - 0.08) / 2), 'MI_ChainLink',
            uv_rect=(0, 0, (L - 0.03) / 0.2, (H - 0.08) / 0.2))
    for z in (0.2, 0.6, 1.0, 1.35):
        b.torus(0.034, 0.004, (0, 0, z), 'MI_Steel', segs=12, rsegs=4)
        b.box((0.03, 0.035, 0.012), (0.03, -0.02, z), 'MI_Steel', bevel=0.002)
    b.box((0.008, 0.01, H - 0.1), (0.04, -0.03, H / 2), 'MI_Steel')
    for k in range(6):
        x = 0.4 + k * 0.48
        b.torus(0.022, 0.0015, (x, -0.01, H - 0.05), 'MI_Steel', rot=(0, 90, 0), segs=8, rsegs=3)
    return b


def _hedge_run(b, L, D, H, mat, seed=0, res=0.08, amp=0.06, r=0.16):
    """Lumpy trimmed hedge from x = 0 to L: a lofted rounded profile displaced by noise that is
    periodic in x, so segments tile without seams; flat end caps."""
    prof = []
    n_side = 7
    for k in range(n_side + 1):          # -Y side, bottom to the start of the rounding
        z = (H - r) * k / n_side
        prof.append((-D / 2, z, -1.0, 0.0))
    for k in range(1, 6):
        a = math.pi - math.pi / 2 * k / 6
        prof.append((-D / 2 + r + r * math.cos(a), H - r + r * math.sin(a), math.cos(a), math.sin(a)))
    n_top = max(2, int((D - 2 * r) / res))
    for k in range(n_top + 1):
        prof.append((-D / 2 + r + (D - 2 * r) * k / n_top, H, 0.0, 1.0))
    for k in range(1, 6):
        a = math.pi / 2 - math.pi / 2 * k / 6
        prof.append((D / 2 - r + r * math.cos(a), H - r + r * math.sin(a), math.cos(a), math.sin(a)))
    for k in range(n_side + 1):
        z = (H - r) * (1 - k / n_side)
        prof.append((D / 2, z, 1.0, 0.0))
    nx = max(4, int(round(L / res)))
    R = L / (2 * math.pi)
    off = Vector((seed * 1.7, seed * 2.3, seed * 0.9))
    pb, uv = b._begin()
    rings = []
    for i in range(nx + 1):
        x = L * i / nx
        th = 2 * math.pi * i / nx
        ring = []
        for y, z, ny, nz in prof:
            q = Vector((R * math.cos(th) * 3.0, R * math.sin(th) * 3.0 + y * 3.0, z * 3.0)) + off
            d = amp * (noise.noise(q) + 0.5 * noise.noise(q * 2.4) + 0.35 * noise.noise(q * 5.5))
            taper = 0.86 + 0.14 * min(1.0, z / 0.25)
            ring.append(pb.verts.new((x, (y + ny * d) * taper, max(0.0, z + nz * d))))
        rings.append(ring)
    m = len(prof)
    for a, c in zip(rings, rings[1:]):
        for k in range(m - 1):
            pb.faces.new((a[k], a[k + 1], c[k + 1], c[k]))
    pb.faces.new(rings[0])
    pb.faces.new(list(reversed(rings[-1])))
    bmesh.ops.recalc_face_normals(pb, faces=pb.faces)
    ng = [f for f in pb.faces if len(f.verts) > 4]
    bmesh.ops.triangulate(pb, faces=ng)
    pb.normal_update()
    _box_uv(pb.faces, uv)
    b._commit(pb, mat)


@wprop('SM_Hedge_Box', 'Props/Town', 'box', 'town', kind='fence', size=(2.0, 0.7, 1.2))
def hedge_box():
    b = Builder('SM_Hedge_Box')
    _hedge_run(b, 2.0, 0.66, 1.18, 'MI_Hedge', seed=3)
    return b


@wprop('SM_Bush_Round', 'Props/Town', 'convex', 'town', size=(1.0, 1.0, 0.9))
def bush_round():
    b = Builder('SM_Bush_Round')
    _blob(b, 0.47, (0, 0, 0.42), 'MI_Hedge', scale=(1.0, 0.98, 0.95), subdiv=4, amp=0.16, freq=2.2, seed=11, zmin=0.0)
    for k in range(5):
        a = k * 1.26
        _blob(b, 0.16, (math.cos(a) * 0.3, math.sin(a) * 0.3, 0.55 + 0.1 * math.sin(k)), 'MI_Hedge', subdiv=2,
              amp=0.3, freq=2.5, seed=k + 20)
    return _recentre(b)


@wprop('SM_Bush_Small', 'Props/Town', 'convex', 'town', size=(0.6, 0.6, 0.55))
def bush_small():
    b = Builder('SM_Bush_Small')
    _blob(b, 0.29, (0, 0, 0.25), 'MI_Hedge', scale=(1.0, 0.95, 0.92), subdiv=3, amp=0.18, freq=2.4, seed=31, zmin=0.0)
    for k in range(3):
        a = k * 2.1
        _blob(b, 0.1, (math.cos(a) * 0.17, math.sin(a) * 0.17, 0.33), 'MI_Hedge', subdiv=2, amp=0.3, freq=2.5,
              seed=k + 40)
    return _recentre(b)


# =======================================================================================
# 4. VEGETATION AND PARKED CARS (Props/Town)
# Tree collision is 'complex': the importer's box / convex hulls would wrap the whole canopy,
# complex-as-simple leaves the space under the crown walkable and blocks only the trunk.

def _taper_tube(b, pts, radii, mat, segs=8, caps=True):
    """Tube along a polyline with a radius per point (parallel-transport frames)."""
    pts = [Vector(p) for p in pts]
    pb, uv = b._begin()
    rings = []
    prev = None
    for i, p in enumerate(pts):
        if i == 0:
            t = pts[1] - p
        elif i == len(pts) - 1:
            t = p - pts[i - 1]
        else:
            t = pts[i + 1] - pts[i - 1]
        t.normalize()
        if prev is None:
            ref = Vector((0, 0, 1)) if abs(t.z) < 0.9 else Vector((1, 0, 0))
            n = (ref - t * ref.dot(t)).normalized()
        else:
            n = (prev - t * prev.dot(t)).normalized()
        prev = n
        bn = t.cross(n)
        rings.append([pb.verts.new(p + (n * math.cos(a) + bn * math.sin(a)) * radii[i])
                      for a in (2 * math.pi * k / segs for k in range(segs))])
    dist = [0.0]
    for a, c in zip(pts, pts[1:]):
        dist.append(dist[-1] + (c - a).length)
    for ri in range(len(rings) - 1):
        for k in range(segs):
            k2 = (k + 1) % segs
            f = pb.faces.new((rings[ri][k], rings[ri][k2], rings[ri + 1][k2], rings[ri + 1][k]))
            for loop, (kk, rr) in zip(f.loops, ((k, ri), (k + 1, ri), (k + 1, ri + 1), (k, ri + 1))):
                loop[uv].uv = (kk / segs * 2 * math.pi * max(radii[rr], 0.05), dist[rr])
    if caps:
        pb.faces.new(list(reversed(rings[0])))
        pb.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(pb, faces=pb.faces)
    b._commit(pb, mat)


def _limb(b, rng, start, direction, length, r0, depth, ends, mat='MI_Bark', gnarl=0.25, up=0.15, n=4,
          kids=(2, 3), spread=40.0, child_len=0.62, droop=0.0):
    """Recursive branch: a tapered, jittered tube that forks into 'kids' children."""
    d = Vector(direction).normalized()
    p = Vector(start)
    pts, radii = [p.copy()], [r0]
    for i in range(1, n + 1):
        jit = Vector((rng.uniform(-gnarl, gnarl), rng.uniform(-gnarl, gnarl), rng.uniform(-gnarl, gnarl)))
        d = (d + jit + Vector((0, 0, up - droop * i / n))).normalized()
        p = p + d * (length / n)
        pts.append(p.copy())
        radii.append(r0 * (1 - 0.55 * i / n))
    _taper_tube(b, pts, radii, mat, segs=max(4, 9 - 2 * (3 - depth)) if depth < 3 else 9)
    if depth <= 0:
        ends.append((p.copy(), d.copy()))
        return
    count = rng.randint(*kids)
    for c in range(count):
        idx = rng.randint(max(1, n - 2), n)
        base = pts[idx]
        az = rng.uniform(0, 2 * math.pi)
        side = Vector((math.cos(az), math.sin(az), 0))
        side = (side - d * side.dot(d)).normalized()
        ang = math.radians(rng.uniform(spread * 0.5, spread))
        nd = (d * math.cos(ang) + side * math.sin(ang)).normalized()
        _limb(b, rng, base, nd, length * child_len * rng.uniform(0.85, 1.1), radii[idx] * 0.72, depth - 1, ends, mat,
              gnarl, up, n, kids, spread, child_len, droop)
    if count:
        ends.append((p.copy(), d.copy()))


def _root_flare(b, r, h, mat='MI_Bark', segs=10):
    b.lathe([(0, -0.02), (r * 1.55, -0.02), (r * 1.2, h * 0.25), (r * 1.05, h * 0.6), (r, h), (0, h)], mat=mat, segs=segs)
    for k in range(5):
        a = k * 2 * math.pi / 5 + 0.3
        b.tube([(math.cos(a) * r * 0.6, math.sin(a) * r * 0.6, h * 0.5), (math.cos(a) * r * 1.6, math.sin(a) * r * 1.6, 0.02),
                (math.cos(a) * r * 2.4, math.sin(a) * r * 2.4, -0.02)], r * 0.25, mat, segs=5, bend=r * 0.5)


def _crown(b, ends, rng, mat, rmin, rmax, flatten=0.8, extra=()):
    for i, (p, d) in enumerate(list(ends) + list(extra)):
        r = rng.uniform(rmin, rmax)
        _blob(b, r, tuple(p + Vector((0, 0, r * 0.25))), mat, scale=(1.0, 1.0, flatten), subdiv=3, amp=0.22, freq=1.4,
              seed=i * 7 + 1)


def _clip_floor(b):
    for v in b.bm.verts:
        if v.co.z < 0.0:
            v.co.z = 0.0


@wprop('SM_Tree_Oak', 'Props/Town', 'complex', 'nature', size=(7.0, 7.0, 8.5))
def tree_oak():
    b = Builder('SM_Tree_Oak')
    rng = _rng('SM_Tree_Oak')
    _root_flare(b, 0.3, 0.7)
    trunk = [(0, 0, 0.5), (0.05, 0.02, 1.4), (0.0, 0.08, 2.3), (0.06, 0.05, 2.9)]
    _taper_tube(b, trunk, (0.3, 0.27, 0.24, 0.21), 'MI_Bark', segs=10)
    ends = []
    for k in range(5):
        az = k * 2 * math.pi / 5 + rng.uniform(-0.3, 0.3)
        el = math.radians(rng.uniform(30, 48))
        d = Vector((math.cos(az) * math.cos(el), math.sin(az) * math.cos(el), math.sin(el)))
        _limb(b, rng, (0.06, 0.05, 2.75), d, rng.uniform(2.2, 2.7), 0.15, 2, ends, gnarl=0.18, up=0.08, kids=(2, 2),
              spread=38, child_len=0.6)
    tops = [(Vector((0, 0, 6.2)), None), (Vector((0.8, -0.6, 5.6)), None), (Vector((-0.7, 0.7, 5.8)), None)]
    _crown(b, [e for e in ends if e[0].z > 4.0], rng, 'MI_Tree_Leaves', 1.05, 1.5, 0.78, tops)
    _clip_floor(b)
    return b


@wprop('SM_Tree_Maple', 'Props/Town', 'complex', 'nature', size=(4.8, 4.8, 6.0))
def tree_maple():
    b = Builder('SM_Tree_Maple')
    rng = _rng('SM_Tree_Maple')
    _root_flare(b, 0.18, 0.4)
    _taper_tube(b, [(0, 0, 0.3), (0.03, 0.0, 1.1), (0.0, 0.04, 1.9)], (0.18, 0.16, 0.14), 'MI_Bark', segs=9)
    ends = []
    for k in range(4):
        az = k * math.pi / 2 + rng.uniform(-0.3, 0.3)
        el = math.radians(rng.uniform(55, 68))
        d = Vector((math.cos(az) * math.cos(el), math.sin(az) * math.cos(el), math.sin(el)))
        _limb(b, rng, (0.0, 0.04, 1.8), d, rng.uniform(1.8, 2.1), 0.1, 2, ends, gnarl=0.15, up=0.12, kids=(2, 2),
              spread=35, child_len=0.6)
    _crown(b, [e for e in ends if e[0].z > 3.0], rng, 'MI_Tree_Leaves', 0.8, 1.15, 0.9,
           [(Vector((0, 0, 4.9)), None), (Vector((0.3, 0.2, 4.0)), None)])
    _clip_floor(b)
    return b


@wprop('SM_Tree_Dead', 'Props/Town', 'complex', 'nature', size=(5.5, 5.5, 6.5))
def tree_dead():
    """Bare, gnarled, twisting branches - the spooky silhouette by the old house."""
    b = Builder('SM_Tree_Dead')
    rng = _rng('SM_Tree_Dead')
    _root_flare(b, 0.24, 0.5)
    trunk = [(0, 0, 0.4), (0.12, -0.05, 1.0), (0.02, 0.1, 1.7), (-0.12, 0.02, 2.3)]
    _taper_tube(b, trunk, (0.24, 0.22, 0.19, 0.16), 'MI_Bark', segs=9)
    ends = []
    for k in range(4):
        az = k * math.pi / 2 + rng.uniform(-0.5, 0.5)
        el = math.radians(rng.uniform(25, 60))
        d = Vector((math.cos(az) * math.cos(el), math.sin(az) * math.cos(el), math.sin(el)))
        _limb(b, rng, (-0.12, 0.02, 2.2), d, rng.uniform(1.7, 2.1), 0.12, 3, ends, gnarl=0.42, up=0.05, n=5,
              kids=(2, 3), spread=55, child_len=0.62, droop=0.12)
    _limb(b, rng, (0.1, -0.04, 1.2), Vector((0.9, -0.3, 0.25)), 1.5, 0.08, 2, ends, gnarl=0.45, up=0.0, n=5, kids=(2, 2),
          spread=50, child_len=0.6, droop=0.2)
    _clip_floor(b)
    lo, hi = _bounds(b)
    bmesh.ops.scale(b.bm, vec=(6.5 / hi.z,) * 3, verts=b.bm.verts)
    return b


@wprop('SM_Tree_Pine', 'Props/Town', 'complex', 'nature', size=(4.2, 4.2, 9.0))
def tree_pine():
    b = Builder('SM_Tree_Pine')
    b.lathe([(0, -0.02), (0.3, -0.02), (0.22, 0.15), (0.2, 0.5), (0.12, 5.0), (0.04, 8.6), (0, 8.7)], mat='MI_Bark',
            segs=10)
    tiers = 8
    for i in range(tiers):
        t = i / (tiers - 1)
        z = 1.3 + t * 6.5
        r = 1.7 * (1 - t) + 0.4
        h = 1.25 - 0.45 * t
        s = Builder('tier')
        s.lathe([(0.12, z + h), (r * 0.55, z + h * 0.45), (r, z - 0.05), (r * 0.94, z - 0.2), (r * 0.5, z + 0.05),
                 (0.12, z + h * 0.35)], mat='MI_Tree_Leaves', segs=18, closed=True)
        for v in s.bm.verts:
            a = math.atan2(v.co.y, v.co.x)
            rr = math.hypot(v.co.x, v.co.y)
            if rr > 0.3:
                k = 1 + 0.16 * math.sin(a * 9 + i * 1.7) * min(1.0, rr / r) + 0.06 * noise.noise(
                    Vector((math.cos(a) * 2 + i, math.sin(a) * 2, v.co.z)))
                v.co.x *= k
                v.co.y *= k
                v.co.z -= 0.18 * (rr / r) ** 2 * (0.5 + 0.5 * math.sin(a * 9 + i * 1.7))
        _attach(b, s, (0, 0, 0), (0, 0, i * 23))
    b.lathe([(0.0, 8.2), (0.32, 8.2), (0.0, 9.0)], mat='MI_Tree_Leaves', segs=10)
    _clip_floor(b)
    return b


# --- Cars ---------------------------------------------------------------------------------

CAR_TYPES = {
    # L, W, wheel radius, front / rear axle y, rocker z, lower-body profile (y, z) above the arches,
    # cabin profile (y, z) and door seams.
    'sedan': dict(L=4.7, W=1.8, r=0.32, ax=(-1.42, 1.33), rocker=0.24,
                  top=[(2.35, 0.32), (2.35, 0.78), (2.3, 0.86), (1.2, 0.9), (-0.85, 0.9), (-2.28, 0.81), (-2.35, 0.74),
                       (-2.35, 0.32)],
                  cabin=[(-0.88, 0.89), (-0.22, 1.37), (0.78, 1.38), (1.24, 0.89)],
                  doors=(-0.86, 0.27, 1.16), bumper='MI_Chrome'),
    'hatch': dict(L=4.0, W=1.7, r=0.3, ax=(-1.3, 1.22), rocker=0.24,
                  top=[(2.0, 0.32), (2.0, 0.9), (1.95, 0.96), (-0.65, 0.93), (-1.92, 0.82), (-2.0, 0.74), (-2.0, 0.32)],
                  cabin=[(-0.68, 0.92), (-0.08, 1.43), (1.38, 1.44), (1.95, 0.95)],
                  doors=(-0.66, 0.55), bumper='MI_Plastic_Black'),
    'pickup': dict(L=5.3, W=1.95, r=0.38, ax=(-1.75, 1.5), rocker=0.45,
                   top=[(0.42, 0.45), (0.42, 1.12), (-0.95, 1.12), (-2.56, 1.08), (-2.65, 1.0), (-2.65, 0.5)],
                   cabin=[(-0.97, 1.11), (-0.42, 1.78), (0.34, 1.8), (0.42, 1.11)],
                   doors=(-0.95, 0.38), bumper='MI_Chrome'),
}


def _arch(cy, r, z0, zb, n=10):
    pts = [(cy - r, zb), (cy - r, z0)]
    for i in range(1, n):
        a = math.pi - math.pi * i / n
        pts.append((cy + r * math.cos(a), z0 + r * math.sin(a)))
    return pts + [(cy + r, z0), (cy + r, zb)]


def _car_wheel(b, x, y, r, w, side):
    """Tyre + hubcap; axle along X, the hubcap faces outwards (side = +-1)."""
    s = Builder('wheel')
    s.lathe([(r * 0.62, -w / 2), (r * 0.9, -w / 2 - 0.005), (r, -w * 0.38), (r * 1.0, w * 0.38), (r * 0.9, w / 2 + 0.005),
             (r * 0.62, w / 2)], mat='MI_Tire', segs=22, closed=True)
    s.lathe([(0, w / 2 + 0.012), (r * 0.18, w / 2 + 0.012), (r * 0.25, w / 2 + 0.005), (r * 0.58, w / 2 + 0.002),
             (r * 0.64, w / 2 - 0.01), (r * 0.62, -w / 4), (0, -w / 4)], mat='MI_Chrome', segs=20)
    for k in range(8):
        a = 2 * math.pi * k / 8
        s.box((r * 0.22, 0.008, 0.02), (math.cos(a) * r * 0.42, math.sin(a) * r * 0.42, w / 2 + 0.006), 'MI_Plastic_Black',
              rot=(0, 0, math.degrees(a)))
    _attach(b, s, (x, y, r), (0, 90 * side, 0))


def _car(name, paint, kind):
    c = CAR_TYPES[kind]
    L, W, r = c['L'], c['W'], c['r']
    b = Builder(name)
    hw = W / 2
    ar, z0 = r + 0.07, r + 0.02
    rb = c['rocker']
    fy, ry = c['ax']
    # Lower body (arches cut) - pickups end at the cab and get a separate bed.
    if kind == 'pickup':
        prof = _arch(fy, ar, z0, rb) + c['top']
    else:
        prof = _arch(fy, ar, z0, rb) + _arch(ry, ar, z0, rb) + c['top']
    b.prism(prof, W - 0.02, (0, 0, 0), paint, axis='X', bevel=0.035)
    # Greenhouse: tapered prism (tumblehome), painted, with dark glass laid on it.
    cab = c['cabin']
    belt, roof = cab[0][1], max(z for _, z in cab)
    hb, hr = hw - 0.03, hw - 0.17

    def half(z):
        t = max(0.0, min(1.0, (z - belt) / (roof - belt)))
        return hb + (hr - hb) * t
    s = Builder('cabin')
    s.prism(cab, W, (0, 0, 0), paint, axis='X')
    for v in s.bm.verts:
        v.co.x = math.copysign(half(v.co.z), v.co.x)
    bmesh.ops.bevel(s.bm, geom=list(s.bm.edges), offset=0.025, offset_type='OFFSET', segments=2, profile=0.5,
                    affect='EDGES', clamp_overlap=True)
    _attach(b, s)
    glass = 'MI_Glass_Van'

    def pane(pts_yz, sx):
        vs = [(sx * (half(z) + 0.004), y, z) for y, z in pts_yz]
        if sx > 0:
            vs = list(reversed(vs))
        _geo(b, vs, [list(range(len(vs)))], glass, smooth=False)

    (y0, z0c), (y1, z1c), (y2, z2c), (y3, z3c) = cab
    for sx in (-1, 1):
        if kind == 'pickup':
            pane([(y0 + 0.16, belt + 0.04), (y1 + 0.06, z1c - 0.05), (y2 - 0.12, z2c - 0.05), (y3 - 0.12, belt + 0.04)], sx)
        else:
            bp = c['doors'][1]
            pane([(y0 + 0.16, belt + 0.04), (y1 + 0.06, z1c - 0.05), (bp - 0.04, z1c - 0.05), (bp - 0.04, belt + 0.04)], sx)
            pane([(bp + 0.05, belt + 0.04), (bp + 0.05, z2c - 0.05), (y2 - 0.02, z2c - 0.05),
                  (y3 - 0.2, belt + 0.04)], sx)
    for (ya, za), (yb, zb_), front in (((y0, z0c), (y1, z1c), True), ((y3, z3c), (y2, z2c), False)):
        dy, dz = yb - ya, zb_ - za
        ln = math.hypot(dy, dz)
        ny, nz = (-dz / ln, dy / ln) if front else (dz / ln, -dy / ln)
        if ny * (-1 if front else 1) < 0:
            ny, nz = -ny, -nz
        pts = []
        for t, inset in ((0.05, 0.08), (0.95, 0.08), (0.95, -0.08), (0.05, -0.08)):
            yy, zz = ya + dy * t + ny * 0.006, za + dz * t + nz * 0.006
            pts.append((yy, zz, t))
        quad = []
        for (yy, zz, t), sx in zip(pts, (1, 1, -1, -1)):
            quad.append((sx * (half(zz) - 0.08), yy, zz))
        if not front:
            quad = list(reversed(quad))
        _geo(b, quad, [[0, 1, 2, 3]], glass, smooth=False)
    # Pickup bed.
    if kind == 'pickup':
        bed = [(0.47, rb)] + _arch(ry, ar, z0, rb)[1:-1] + [(ry + ar, rb), (L / 2, rb), (L / 2, 1.12), (0.47, 1.12)]
        for sx in (-1, 1):
            b.prism(bed, 0.06, (sx * (hw - 0.04), 0, 0), paint, axis='X', bevel=0.012)
            b.cyl(ar, 0.3, (sx * (hw - 0.22), ry, z0), paint, rot=(0, 90, 0), segs=16)
            b.box((0.06, L / 2 - 0.47, 0.03), (sx * (hw - 0.04), (L / 2 + 0.47) / 2, 1.125), 'MI_Plastic_Black', bevel=0.008)
        b.box_minmax((-hw + 0.07, 0.47, 0.6), (hw - 0.07, L / 2 - 0.06, 0.64), paint)
        for k in range(6):
            b.box((0.03, L / 2 - 0.55, 0.012), (-0.6 + k * 0.24, (L / 2 + 0.47) / 2, 0.646), paint)
        b.box_minmax((-hw + 0.07, 0.47, 0.64), (hw - 0.07, 0.52, 1.12), paint, bevel=0.006)
        b.box_minmax((-hw + 0.02, L / 2 - 0.06, rb + 0.05), (hw - 0.02, L / 2, 1.12), paint, bevel=0.012)
        b.box((0.3, 0.01, 0.05), (0, L / 2 + 0.002, 1.02), 'MI_Plastic_Black', bevel=0.004)
        b.box_minmax((-0.45, 0.47, 0.3), (0.45, L / 2 - 0.2, 0.6), 'MI_Metal_Black')
        b.box((0.9, 0.01, 0.5), (0, 0.415, 1.38), glass)
    # Underbody so the car can't be seen through, wheel-well liners, wheels.
    b.box_minmax((-hw + 0.25, -L / 2 + 0.4, 0.14), (hw - 0.25, L / 2 - 0.4, rb + 0.05), 'MI_Metal_Black')
    for y in (fy, ry):
        for sx in (-1, 1):
            b.cyl(ar - 0.01, 0.26, (sx * (hw - 0.14), y, z0), 'MI_Plastic_Black', rot=(0, 90, 0), segs=16, caps=False)
            _car_wheel(b, sx * (hw - 0.135), y, r, 0.21 if kind != 'pickup' else 0.25, sx)
    # Front: grille, headlights, bumper, plate. Rear: tail lights, bumper, plate.
    yf, yr = -L / 2, (L / 2 if kind != 'pickup' else L / 2)
    zg = 0.6 if kind != 'pickup' else 0.85
    gw = 0.9 if kind == 'sedan' else (0.7 if kind == 'hatch' else 1.1)
    gh = 0.18 if kind != 'hatch' else 0.09
    b.box((gw, 0.03, gh), (0, yf - 0.002, zg), 'MI_Plastic_Black', bevel=0.004)
    _frame_ring(b, gw + 0.03, gh + 0.03, gw, gh, yf - 0.012, yf + 0.005, 'MI_Chrome' if kind != 'hatch' else 'MI_Plastic_Grey',
                cz=zg)
    for k in range(4 if kind != 'hatch' else 2):
        b.box((gw - 0.02, 0.012, 0.012), (0, yf - 0.012, zg - gh / 2 + (k + 1) * gh / (5 if kind != 'hatch' else 3)),
              'MI_Chrome' if kind != 'hatch' else 'MI_Plastic_Grey')
    lw, lh = (0.34, 0.14) if kind == 'sedan' else ((0.36, 0.11) if kind == 'hatch' else (0.3, 0.2))
    for sx in (-1, 1):
        x = sx * (gw / 2 + lw / 2 + 0.03)
        b.box((lw + 0.02, 0.025, lh + 0.02), (x, yf - 0.002, zg), 'MI_Chrome', bevel=0.005)
        b.box((lw, 0.02, lh), (x, yf - 0.012, zg), 'MI_Glass', bevel=0.004)
        b.box((lw - 0.04, 0.004, lh - 0.04), (x, yf - 0.004, zg), 'MI_Chrome')
        b.box((0.1, 0.02, 0.05), (sx * (hw - 0.1), yf + 0.02, zg - lh / 2 - 0.06), 'MI_Emit_Amber', bevel=0.004)
    bz = rb + 0.12 if kind != 'pickup' else rb + 0.08
    bm = c['bumper']
    b.box((W + 0.02, 0.14, 0.16), (0, yf + 0.0, bz), bm, bevel=0.04)
    b.box((W - 0.1, 0.15, 0.035), (0, yf + 0.0, bz), 'MI_Rubber_Black', bevel=0.01)
    b.box((W + 0.02, 0.14, 0.16), (0, yr - 0.0, bz), bm, bevel=0.04)
    b.box((W - 0.1, 0.15, 0.035), (0, yr - 0.0, bz), 'MI_Rubber_Black', bevel=0.01)
    b.box((0.31, 0.008, 0.155), (0, yf - 0.074, bz), 'MI_Plastic_White', bevel=0.003)
    rz = 0.8 if kind == 'sedan' else (0.85 if kind == 'hatch' else 0.95)
    for sx in (-1, 1):
        b.box((0.32 if kind != 'pickup' else 0.1, 0.03, 0.14 if kind != 'pickup' else 0.32),
              (sx * (hw - (0.2 if kind != 'pickup' else 0.08)), yr + 0.005, rz - (0.08 if kind == 'sedan' else 0.1)),
              'MI_Plastic_Red', bevel=0.006)
        b.box((0.08, 0.032, 0.06), (sx * (hw - 0.42), yr + 0.005, rz - 0.12), 'MI_Plastic_White', bevel=0.004)
    b.box((0.31, 0.008, 0.155), (0, yr + 0.02, rz - 0.18 if kind != 'pickup' else bz + 0.17), 'MI_Plastic_White',
          bevel=0.003)
    # Doors: seams, handles; side rubbing strip; mirrors; wipers; antenna; exhaust.
    doors = c['doors']
    xs = hw - 0.004
    for sx in (-1, 1):
        for y in doors:
            b.box((0.006, 0.005, belt - rb - 0.06), (sx * xs, y, (belt + rb) / 2 + 0.01), 'MI_Plastic_Black')
        for y0d, y1d in zip(doors, doors[1:]):
            b.box((0.02, 0.13, 0.025), (sx * (xs + 0.005), y1d - 0.2, belt - 0.1), 'MI_Chrome', bevel=0.006)
        b.box((0.012, (ry - fy) - 2 * ar - 0.1, 0.035), (sx * (xs + 0.004), (fy + ry) / 2, rb + 0.27), 'MI_Plastic_Black',
              bevel=0.006)
        b.box((0.035, 0.1, 0.12), (sx * (hw + 0.05), y0 + 0.2, belt + 0.12), 'MI_Plastic_Black', bevel=0.012)
        b.box((0.05, 0.04, 0.03), (sx * (hw + 0.015), y0 + 0.22, belt + 0.06), 'MI_Plastic_Black', bevel=0.008)
        b.box((0.006, 0.08, 0.1), (sx * (hw + 0.07), y0 + 0.21, belt + 0.12), 'MI_Mirror')
        b.box((0.5, 0.02, 0.012), (sx * 0.28, y0 - 0.06, belt + 0.02), 'MI_Plastic_Black', rot=(0, 0, sx * 6))
    b.cyl(0.003, 0.45, (hw - 0.15, y0 - 0.25, belt + 0.22), 'MI_Chrome', rot=(-12, 0, 0), segs=4)
    b.tube([(0.5, L / 2 - 0.5, 0.2), (0.5, L / 2 + 0.02, 0.2)], 0.03, 'MI_Chrome', segs=10)
    b.box((0.012, 0.12, 0.12), (hw - 0.002, (ry - ar - 0.2) if kind != 'pickup' else 2.2, belt - 0.12), paint,
          bevel=0.004)
    return _recentre(b)


@wprop('SM_Car_Sedan_Red', 'Props/Town', 'box', 'nature', size=(1.8, 4.7, 1.4))
def car_sedan_red():
    return _car('SM_Car_Sedan_Red', 'MI_Car_Paint_Red', 'sedan')


@wprop('SM_Car_Sedan_Silver', 'Props/Town', 'box', 'nature', size=(1.8, 4.7, 1.4))
def car_sedan_silver():
    return _car('SM_Car_Sedan_Silver', 'MI_Car_Paint_Silver', 'sedan')


@wprop('SM_Car_Hatch_Blue', 'Props/Town', 'box', 'nature', size=(1.7, 4.0, 1.45))
def car_hatch_blue():
    return _car('SM_Car_Hatch_Blue', 'MI_Car_Paint_Blue', 'hatch')


@wprop('SM_Car_Hatch_Green', 'Props/Town', 'box', 'nature', size=(1.7, 4.0, 1.45))
def car_hatch_green():
    return _car('SM_Car_Hatch_Green', 'MI_Car_Paint_Green', 'hatch')


@wprop('SM_Car_Pickup_Black', 'Props/Town', 'box', 'nature', size=(1.95, 5.3, 1.8))
def car_pickup_black():
    return _car('SM_Car_Pickup_Black', 'MI_Car_Paint_Black', 'pickup')


# =======================================================================================
# 5. YARD PROPS, AWNING AND SHOP SIGNS (Props/Town)

@wprop('SM_Grill_BBQ', 'Props/Town', 'convex', 'yard', size=(0.65, 0.65, 1.0))
def grill_bbq():
    b = Builder('SM_Grill_BBQ')
    k = 'MI_Metal_Black'
    zc = 0.68
    b.lathe([(0, zc - 0.25), (0.12, zc - 0.245), (0.22, zc - 0.18), (0.27, zc - 0.07), (0.28, zc), (0.29, zc + 0.005),
             (0.27, zc + 0.005), (0.26, zc - 0.06), (0.0, zc - 0.2)], mat=k, segs=28)
    b.lathe([(0.288, zc + 0.008), (0.288, zc + 0.03), (0.26, zc + 0.12), (0.18, zc + 0.21), (0.08, zc + 0.25),
             (0.0, zc + 0.255)], mat=k, segs=28)
    b.cyl(0.035, 0.012, (0, 0, zc + 0.26), 'MI_Steel', segs=12)
    b.box((0.2, 0.03, 0.03), (0, -0.3, zc + 0.2), 'MI_Wood_Dark', bevel=0.01)
    for sx in (-1, 1):
        b.box((0.012, 0.03, 0.05), (sx * 0.08, -0.29, zc + 0.18), 'MI_Steel')
    b.box((0.18, 0.03, 0.03), (0, -0.3, zc - 0.04), 'MI_Wood_Dark', bevel=0.01)
    b.cyl(0.258, 0.006, (0, 0, zc - 0.02), 'MI_Steel', segs=24, caps=False)
    for k2 in range(9):
        b.box((0.48 - abs(k2 - 4) * 0.05, 0.006, 0.006), (0, -0.2 + k2 * 0.05, zc - 0.02), 'MI_Steel')
    for i in range(3):
        a = math.radians(90 + i * 120)
        top = (math.cos(a) * 0.17, math.sin(a) * 0.17, zc - 0.18)
        bot = (math.cos(a) * 0.3, math.sin(a) * 0.3, 0.06 if i else 0.0)
        b.tube([top, bot], 0.012, k, segs=8)
        if i:
            b.cyl(0.06, 0.03, (bot[0] * 1.08, bot[1] * 1.08, 0.06), 'MI_Rubber_Black', rot=(0, 90, math.degrees(a) + 90),
                  segs=14)
    b.torus(0.21, 0.006, (0, 0, 0.3), k, segs=24, rsegs=4)
    b.lathe([(0, 0.27), (0.12, 0.28), (0.13, 0.31), (0.12, 0.31), (0.11, 0.29), (0, 0.285)], mat='MI_Steel', segs=18)
    return _recentre(b)


@wprop('SM_Patio_Chair', 'Props/Town', 'box', 'yard', size=(0.56, 0.56, 0.8))
def patio_chair():
    """White resin monobloc chair, a bit grimy."""
    b = Builder('SM_Patio_Chair')
    m = 'MI_Plastic_White'
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.cyl(0.026, 0.42, (sx * 0.22, sy * 0.2, 0.21), m, rot=(sy * 6, -sx * 6, 0), segs=10, radius2=0.03)
    b.box((0.48, 0.46, 0.04), (0, -0.01, 0.43), m, rot=(-4, 0, 0), bevel=0.018)
    for sx in (-1, 1):
        b.box((0.05, 0.04, 0.4), (sx * 0.2, 0.235 + 0.0, 0.64), m, rot=(-14, 0, 0), bevel=0.016)
    for k in range(4):
        z = 0.5 + k * 0.075
        b.box((0.38, 0.03, 0.045), (0, 0.205 + (z - 0.45) * 0.25, z), m, rot=(-14, 0, 0), bevel=0.012)
    b.box((0.46, 0.05, 0.06), (0, 0.27, 0.82), m, rot=(-14, 0, 0), bevel=0.02)
    for sx in (-1, 1):
        b.box((0.05, 0.42, 0.035), (sx * 0.25, 0.02, 0.62), m, rot=(-3, 0, 0), bevel=0.014)
        b.box((0.04, 0.04, 0.2), (sx * 0.25, -0.17, 0.52), m, bevel=0.012)
    return _recentre(b)


@wprop('SM_Patio_Table', 'Props/Town', 'box', 'yard', size=(0.9, 0.9, 0.72))
def patio_table():
    b = Builder('SM_Patio_Table')
    m = 'MI_Plastic_White'
    b.lathe([(0.0, 0.7), (0.03, 0.7), (0.03, 0.72), (0.44, 0.72), (0.45, 0.715), (0.45, 0.69), (0.43, 0.68),
             (0.035, 0.69), (0.03, 0.66), (0.0, 0.66)], mat=m, segs=36)
    for k in range(8):
        a = 2 * math.pi * k / 8
        b.box((0.3, 0.012, 0.003), (math.cos(a) * 0.22, math.sin(a) * 0.22, 0.7205), 'MI_Plastic_Grey',
              rot=(0, 0, math.degrees(a)))
    for k in range(4):
        a = math.radians(45 + k * 90)
        b.tube([(math.cos(a) * 0.3, math.sin(a) * 0.3, 0.68), (math.cos(a) * 0.36, math.sin(a) * 0.36, 0.0)], 0.024, m,
               segs=10)
    b.torus(0.3, 0.012, (0, 0, 0.2), m, segs=28, rsegs=5)
    return b


@wprop('SM_Doghouse', 'Props/Town', 'complex', 'yard', size=(1.0, 1.2, 1.1))
def doghouse():
    """Empty doghouse, opening faces -Y; a chain and an empty collar lie in front."""
    b = Builder('SM_Doghouse')
    w, d, wh = 0.9, 1.0, 0.7
    m = 'MI_Paint_Wood_Red'
    b.box((w + 0.04, d + 0.04, 0.08), (0, 0, 0.04), 'MI_Wood_Dark', bevel=0.005)
    for sx in (-1, 1):
        b.box((0.03, d, wh), (sx * (w / 2 - 0.015), 0, 0.08 + wh / 2), m, bevel=0.003)
    b.box((w, 0.03, wh), (0, d / 2 - 0.015, 0.08 + wh / 2), m, bevel=0.003)
    for sy in (-1, 1):
        gable = [(-w / 2, 0), (w / 2, 0), (0, 0.3)]
        b.prism(gable, 0.03, (0, sy * (d / 2 - 0.015), 0.08 + wh), m, axis='Y')
    # Front wall around an arched opening.
    yf = -d / 2 + 0.015
    ow, oh = 0.38, 0.5
    for sx in (-1, 1):
        b.box(((w - ow) / 2, 0.03, wh), (sx * (ow / 2 + (w - ow) / 4), yf, 0.08 + wh / 2), m, bevel=0.003)
    arch = [(-ow / 2, oh - ow / 2), (-ow / 2, wh), (ow / 2, wh), (ow / 2, oh - ow / 2)]
    for k in range(1, 9):
        a = math.pi * k / 9
        arch.append((ow / 2 * math.cos(a), oh - ow / 2 + ow / 2 * math.sin(a)))
    b.prism(arch, 0.03, (0, yf, 0.08), m, axis='Y')
    b.box((w - 0.06, d - 0.06, 0.01), (0, 0.0, 0.085), 'MI_Wood_Dark')
    _frame_ring(b, 0.2, 0.07, 0.19, 0.06, yf - 0.025, yf - 0.015, 'MI_Wood_Raw', cz=0.08 + wh + 0.08)
    b.box((0.19, 0.012, 0.06), (0, yf - 0.02, 0.08 + wh + 0.08), 'MI_Wood_Raw')
    # Roof.
    z_ridge, z_eave, run = 0.08 + wh + 0.32, 0.08 + wh - 0.04, w / 2 + 0.08
    rl = math.hypot(run, z_ridge - z_eave)
    ang = math.degrees(math.atan2(z_ridge - z_eave, run))
    for sx in (-1, 1):
        nx_, nz_ = sx * math.sin(math.radians(ang)), math.cos(math.radians(ang))
        b.box((rl + 0.02, d + 0.14, 0.03), (sx * run / 2 + nx_ * 0.015, 0, (z_ridge + z_eave) / 2 + nz_ * 0.015),
              'MI_Roof_Shingles_Dark', rot=(0, sx * ang, 0), bevel=0.004)
    b.box((0.06, d + 0.15, 0.05), (0, 0, z_ridge + 0.03), 'MI_Wood_Dark', bevel=0.008)
    # Stake, chain and the empty collar.
    b.cyl(0.012, 0.12, (0.42, -0.62, 0.04), 'MI_Steel', segs=8)
    b.torus(0.025, 0.004, (0.42, -0.62, 0.1), 'MI_Steel', rot=(90, 0, 0), segs=10, rsegs=4)
    _chain(b, [(0.42, -0.63, 0.012), (0.3, -0.72, 0.006), (0.1, -0.7, 0.006), (-0.05, -0.8, 0.006), (-0.22, -0.74, 0.006)],
           link=0.035, wire=0.0045, mat='MI_Steel', segs=6, rsegs=4)
    b.torus(0.08, 0.012, (-0.3, -0.74, 0.012), 'MI_Leather', segs=20, rsegs=4, scale=(1, 0.8, 1))
    b.box((0.03, 0.02, 0.022), (-0.225, -0.74, 0.012), 'MI_Steel', bevel=0.004)
    b.lathe([(0, 0), (0.08, 0), (0.1, 0.05), (0.095, 0.05), (0.075, 0.008), (0, 0.008)], (0.25, -0.85, 0.0), 'MI_Steel',
            segs=18)
    return _recentre(b, x=True, y=False)


@wprop('SM_Garden_Gnome', 'Props/Town', 'convex', 'yard', size=(0.2, 0.2, 0.35))
def garden_gnome():
    """Chipped garden gnome with a fixed, too-wide grin."""
    b = Builder('SM_Garden_Gnome')
    b.lathe([(0, 0), (0.09, 0), (0.092, 0.015), (0.0, 0.02)], mat='MI_Stone_Wall', segs=16)
    for sx in (-1, 1):
        b.sphere(0.03, (sx * 0.03, -0.02, 0.035), 'MI_Plastic_Black', scale=(1, 1.5, 0.8), segs=10, rings=6)
    b.lathe([(0.0, 0.03), (0.065, 0.03), (0.07, 0.08), (0.066, 0.13), (0.055, 0.16), (0.0, 0.165)], mat='MI_Car_Paint_Blue',
            segs=18)
    b.torus(0.066, 0.008, (0, 0, 0.1), 'MI_Plastic_Black', segs=18, rsegs=4)
    b.box((0.022, 0.006, 0.018), (0, -0.072, 0.1), 'MI_Gold', bevel=0.002)
    for sx in (-1, 1):
        b.tube([(sx * 0.06, 0.0, 0.15), (sx * 0.075, -0.03, 0.11), (sx * 0.03, -0.065, 0.1)], 0.018, 'MI_Car_Paint_Blue',
               segs=8, bend=0.02)
        b.sphere(0.016, (sx * 0.026, -0.072, 0.1), 'MI_Ceramic', segs=8, rings=5)
    hz = 0.2
    b.sphere(0.048, (0, 0, hz), 'MI_Ceramic', segs=16, rings=10)
    _blob(b, 0.05, (0, -0.03, hz - 0.035), 'MI_Porcelain', scale=(0.9, 0.55, 1.1), subdiv=2, amp=0.2, freq=3, seed=4)
    b.sphere(0.014, (0, -0.05, hz + 0.0), 'MI_Paint_Wood_Red', segs=10, rings=6)
    for sx in (-1, 1):
        b.sphere(0.009, (sx * 0.018, -0.04, hz + 0.018), 'MI_Porcelain', segs=8, rings=5)
        b.sphere(0.0045, (sx * 0.018, -0.0485, hz + 0.018), 'MI_Plastic_Black', segs=6, rings=4)
    b.torus(0.022, 0.0025, (0, -0.052, hz - 0.012), 'MI_Plastic_Black', rot=(90, 0, 0), segs=12, rsegs=3, arc=150,
            scale=(1, 1, 1))
    b.lathe([(0.0, 0.0), (0.052, 0.0), (0.05, 0.02), (0.03, 0.07), (0.012, 0.11), (0.0, 0.13)], (0, 0.004, hz + 0.02),
            'MI_Plastic_Red', rot=(-12, 8, 0), segs=16)
    return _recentre(b)


@wprop('SM_Swing_Rusty', 'Props/Town', 'complex', 'yard', size=(2.4, 1.5, 2.1))
def swing_rusty():
    """Rusty A-frame swing set; the left swing hangs askew as if someone just left it."""
    b = Builder('SM_Swing_Rusty')
    rust = 'MI_Metal_Rust'
    top = 2.06
    for sx in (-1, 1):
        x = sx * 1.15
        for sy in (-1, 1):
            b.tube([(x, 0.0, top), (x + sx * 0.03, sy * 0.72, 0.0)], 0.024, rust, segs=8)
        b.cyl(0.015, 1.2, (x + sx * 0.012, 0.0, 0.7), rust, rot=(90, 0, 0), segs=6)
    b.cyl(0.035, 2.4, (0, 0, top), rust, rot=(0, 90, 0), segs=10)
    for sx in (-1, 1):
        b.sphere(0.04, (sx * 1.2, 0, top), rust, segs=8, rings=5)
    for i, (x, swing) in enumerate(((-0.5, 14.0), (0.5, 0.0))):
        seat_z = 0.48
        s = Builder('swing')
        for dx in (-0.2, 0.2):
            s.torus(0.02, 0.004, (dx, 0, -0.02), 'MI_Steel', rot=(0, 90, 0), segs=8, rsegs=3)
            _chain(s, [(dx, 0, -0.04), (dx, 0, -(top - seat_z) + 0.04)], link=0.06, wire=0.0035, mat='MI_Steel', segs=5,
                   rsegs=3)
        seat = Builder('seat')
        seat.box((0.46, 0.17, 0.02), (0, 0, 0), 'MI_Rubber_Black', bevel=0.006)
        seat.box((0.48, 0.03, 0.035), (0, 0, 0.012), 'MI_Steel', bevel=0.004)
        _attach(s, seat, (0, 0, -(top - seat_z)), (0, 0, 0))
        _attach(b, s, (x, 0, top), (swing, 0, 4.0 if i == 0 else 0))
    return _recentre(b)


@wprop('SM_Wheelbarrow', 'Props/Town', 'convex', 'yard', size=(0.65, 1.45, 0.65))
def wheelbarrow():
    """Wheel at the front (-Y), handles at the back."""
    b = Builder('SM_Wheelbarrow')
    rings = [(0, 0, 0.0), (0.18, 0.24, 0.0), (0.24, 0.32, 0.12), (0.3, 0.38, 0.26), (0.31, 0.39, 0.28), (0.3, 0.38, 0.285),
             (0.285, 0.365, 0.27), (0.23, 0.3, 0.12), (0.17, 0.22, 0.02), (0, 0, 0.02)]
    s = Builder('tray')
    _sloft(s, rings, 'MI_Metal_Green', n=3.0, segs=28)
    _attach(b, s, (0, -0.1, 0.32), (-8, 0, 0))
    for sx in (-1, 1):
        b.tube([(sx * 0.1, -0.62, 0.17), (sx * 0.2, -0.3, 0.3), (sx * 0.24, 0.3, 0.42), (sx * 0.27, 0.78, 0.55)], 0.018,
               'MI_Wood_Raw', segs=8)
        b.cyl(0.022, 0.12, (sx * 0.27, 0.73, 0.54), 'MI_Rubber_Black', rot=(-75, 0, 0), segs=10)
        b.tube([(sx * 0.21, 0.2, 0.39), (sx * 0.22, 0.25, 0.0)], 0.012, 'MI_Metal_Green', segs=6)
        b.box((0.03, 0.08, 0.012), (sx * 0.22, 0.26, 0.006), 'MI_Metal_Green')
        b.tube([(sx * 0.1, -0.6, 0.17), (sx * 0.16, -0.45, 0.28)], 0.008, 'MI_Metal_Green', segs=6)
    b.cyl(0.17, 0.08, (0, -0.62, 0.17), 'MI_Tire', rot=(0, 90, 0), segs=20)
    b.cyl(0.09, 0.09, (0, -0.62, 0.17), 'MI_Metal_Green', rot=(0, 90, 0), segs=14)
    b.cyl(0.012, 0.22, (0, -0.62, 0.17), 'MI_Steel', rot=(0, 90, 0), segs=6)
    return _recentre(b)


@wprop('SM_Shed_Small', 'Props/Town', 'complex', 'yard', size=(2.4, 1.8, 2.3))
def shed_small():
    """Wooden garden shed, door on the front (-Y), a small window on +X."""
    b = Builder('SM_Shed_Small')
    w, d, wh = 2.2, 1.6, 1.85
    m = 'MI_Siding_Worn'
    b.box((w + 0.04, d + 0.04, 0.1), (0, 0, 0.05), 'MI_Wood_Dark', bevel=0.006)
    t = 0.03
    b.box((w, t, wh), (0, d / 2 - t / 2, 0.1 + wh / 2), m)
    for sx in (-1, 1):
        if sx > 0:
            for (y0, y1, z0, z1) in ((-d / 2, -0.25, 0.1, 0.1 + wh), (0.25, d / 2, 0.1, 0.1 + wh),
                                     (-0.25, 0.25, 0.1, 1.05), (-0.25, 0.25, 1.55, 0.1 + wh)):
                b.box_minmax((w / 2 - t, y0, z0), (w / 2, y1, z1), m)
        else:
            b.box((t, d, wh), (sx * (w / 2 - t / 2), 0, 0.1 + wh / 2), m)
    # Window on +X.
    _sw = Builder('win')
    _frame_ring(_sw, 0.56, 0.56, 0.5, 0.5, -0.03, 0.03, 'MI_Trim_White', cz=0)
    _sw.box((0.5, 0.006, 0.5), (0, 0, 0), 'MI_Glass_Dirty')
    _sw.box((0.5, 0.02, 0.025), (0, -0.01, 0), 'MI_Trim_White')
    _sw.box((0.025, 0.02, 0.5), (0, -0.01, 0), 'MI_Trim_White')
    _sw.box((0.62, 0.06, 0.03), (0, -0.04, -0.29), 'MI_Trim_White', bevel=0.004)
    _attach(b, _sw, (w / 2 - 0.0, 0, 1.3), (0, 0, 90))
    # Front wall with a door opening and a ledged-and-braced door with hasp and padlock.
    dw, dh = 0.8, 1.7
    yf = -d / 2 + t / 2
    for sx in (-1, 1):
        b.box(((w - dw) / 2, t, wh), (sx * (dw / 2 + (w - dw) / 4), yf, 0.1 + wh / 2), m)
    b.box((dw, t, wh - dh), (0, yf, 0.1 + dh + (wh - dh) / 2), m)
    for sy in (-1, 1):
        b.prism([(-w / 2, 0), (w / 2, 0), (0, 0.42)], t, (0, sy * (d / 2 - t / 2), 0.1 + wh), m, axis='Y')
    door = Builder('door')
    for k in range(6):
        door.box((dw / 6 - 0.004, 0.025, dh - 0.01), ((k + 0.5) * dw / 6 - dw / 2, 0, dh / 2), 'MI_Wood_Planks',
                 bevel=0.002, uv_rotate=True, uv_offset=(k * 0.17, 0))
    for z in (0.2, dh - 0.2):
        door.box((dw - 0.04, 0.025, 0.1), (0, -0.025, z), 'MI_Wood_Raw', bevel=0.003)
    door.box((0.07, 0.025, (dh - 0.4) / math.cos(math.radians(26))), (0, -0.025, dh / 2), 'MI_Wood_Raw',
             rot=(0, -26, 0), bevel=0.003)
    for z in (0.2, dh - 0.2):
        door.box((0.3, 0.006, 0.035), (-dw / 2 + 0.15, -0.04, z), 'MI_Metal_Black', bevel=0.002)
    door.box((0.12, 0.006, 0.03), (dw / 2 - 0.06, -0.04, 1.0), 'MI_Steel', bevel=0.002)
    door.box((0.04, 0.03, 0.06), (dw / 2 + 0.02, -0.03, 1.0), 'MI_Steel', bevel=0.003)
    door.box((0.045, 0.02, 0.04), (dw / 2 + 0.02, -0.06, 0.96), 'MI_Brass', bevel=0.006)
    door.torus(0.014, 0.003, (dw / 2 + 0.02, -0.06, 0.99), 'MI_Steel', rot=(90, 0, 0), segs=10, rsegs=4, arc=200)
    _attach(b, door, (0, yf - 0.005, 0.11), (0, 0, 0))
    # Roof with overhang and fascia boards.
    z_ridge, z_eave, run = 0.1 + wh + 0.42, 0.1 + wh - 0.04, w / 2 + 0.1
    rl = math.hypot(run, z_ridge - z_eave)
    ang = math.degrees(math.atan2(z_ridge - z_eave, run))
    for sx in (-1, 1):
        nx_, nz_ = sx * math.sin(math.radians(ang)), math.cos(math.radians(ang))
        cx, cz = sx * run / 2, (z_ridge + z_eave) / 2
        b.box((rl, d + 0.2, 0.04), (cx + nx_ * 0.02, 0, cz + nz_ * 0.02), 'MI_Roof_Shingles_Brown', rot=(0, sx * ang, 0),
              bevel=0.005)
        for sy in (-1, 1):
            b.box((rl, 0.02, 0.1), (cx - nx_ * 0.03, sy * (d / 2 + 0.1), cz - nz_ * 0.03), 'MI_Trim_White',
                  rot=(0, sx * ang, 0), bevel=0.003)
    b.box((0.08, d + 0.21, 0.06), (0, 0, z_ridge + 0.05), 'MI_Trim_Dark', bevel=0.008)
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.box((0.06, 0.06, wh), (sx * (w / 2 - 0.01), sy * (d / 2 - 0.01), 0.1 + wh / 2), 'MI_Trim_White',
                  bevel=0.004)
    return _recentre(b)


@wprop('SM_Awning', 'Props/Town', 'none', 'yard', kind='wall', size=(3.0, 1.0, 0.75))
def awning():
    """Striped shop awning: back rail on the wall (y = 0), slopes down to a scalloped valance."""
    b = Builder('SM_Awning')
    W, D = 3.0, 1.0
    ztop, zfront = 0.75, 0.3
    stripes = ('MI_Fabric_Curtain', 'MI_Fabric_Olive')
    ns = 12
    pb, uv = b._begin()
    slots = [b._slot(m) for m in stripes]
    nx, ny = ns * 3, 8
    rows = []
    for j in range(ny + 1):
        v = j / ny
        row = []
        for i in range(nx + 1):
            u = i / nx
            x = -W / 2 + 0.02 + (W - 0.04) * u
            y = -0.03 - (D - 0.05) * v
            z = ztop - (ztop - zfront) * v - 0.025 * math.sin(math.pi * v) * (1 + 0.3 * math.sin(u * 40))
            row.append(pb.verts.new((x, y, z)))
        rows.append(row)
    for j in range(ny):
        for i in range(nx):
            f = pb.faces.new((rows[j][i], rows[j][i + 1], rows[j + 1][i + 1], rows[j + 1][i]))
            f.material_index = slots[(i // 3) % 2]
            for loop, (a, c) in zip(f.loops, ((i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1))):
                loop[uv].uv = (a / nx * W, c / ny * D)
    # Scalloped valance hanging from the front edge.
    sw = (W - 0.04) / ns
    for k in range(ns):
        x0 = -W / 2 + 0.02 + k * sw
        pts = [(x0, zfront), (x0 + sw, zfront)]
        for q in range(1, 8):
            a = math.pi * q / 8
            pts.append((x0 + sw / 2 + sw / 2 * math.cos(a), zfront - 0.2 - 0.05 * math.sin(a)))
        vs = [pb.verts.new((x, -D + 0.02, z)) for x, z in pts]
        f = pb.faces.new(vs)
        f.material_index = slots[k % 2]
        for loop, (x, z) in zip(f.loops, pts):
            loop[uv].uv = (x, z)
    bmesh.ops.solidify(pb, geom=list(pb.faces), thickness=0.004)
    bmesh.ops.recalc_face_normals(pb, faces=pb.faces)
    pb.normal_update()
    _commit_multi(b, pb, [b.mats[f.material_index] for f in pb.faces])
    b.box((W, 0.04, 0.06), (0, -0.02, ztop + 0.0), 'MI_Metal_Black', bevel=0.005)
    b.cyl(0.015, W, (0, -D + 0.02, zfront), 'MI_Metal_Black', rot=(0, 90, 0), segs=8)
    for x in (-W / 2 + 0.05, 0.0, W / 2 - 0.05):
        b.tube([(x, -0.01, ztop - 0.35), (x, -D + 0.03, zfront - 0.005)], 0.01, 'MI_Metal_Black', segs=6)
        b.box((0.05, 0.02, 0.08), (x, -0.01, ztop - 0.35), 'MI_Metal_Black', bevel=0.004)
    return _recentre(b, z0=0.0, x=False, y=False)


def _shop_sign(name, img, frame='MI_Trim_Dark', back='MI_Wood_Dark', museum=False):
    """3.0 x 0.08 x 0.75 framed board on the wall (back at y = 0); the front face shows the image 0..1."""
    b = Builder(name)
    W, D, H = 3.0, 0.08, 0.75
    cz = H / 2
    if museum:
        b.box((W, D - 0.03, H), (0, -(D - 0.03) / 2, cz), 'MI_Stone_Wall', bevel=0.01)
        _frame_ring(b, W - 0.02, H - 0.02, W - 0.05, H - 0.05, -D, -0.04, 'MI_Brass', cz=cz, bevel=0.004)
        b.plane(W - 0.05, H - 0.05, (0, -D + 0.012, cz), img)
        for sx in (-1, 1):
            for sz in (-1, 1):
                b.lathe([(0, 0), (0.018, 0.0), (0.016, 0.006), (0, 0.009)], (sx * (W / 2 - 0.06), -D + 0.01,
                                                                             cz + sz * (H / 2 - 0.06)),
                        'MI_Brass', rot=(90, 0, 0), segs=10)
        return b
    b.box((W - 0.04, 0.03, H - 0.04), (0, -0.015, cz), back)
    _frame_ring(b, W, H, W - 0.05, H - 0.05, -D, 0.0, frame, cz=cz, bevel=0.006)
    b.plane(W - 0.05, H - 0.05, (0, -0.0305, cz), img)
    for sx in (-1, 1):
        for sz in (-1, 1):
            b.cyl(0.01, 0.006, (sx * (W / 2 - 0.012), -D - 0.002, cz + sz * (H / 2 - 0.012)), 'MI_Steel',
                  rot=(90, 0, 0), segs=8)
    return b


@wprop('SM_Sign_Brannigan', 'Props/Town', 'box', 'yard', kind='wall', size=(3.0, 0.08, 0.75))
def sign_brannigan():
    return _shop_sign('SM_Sign_Brannigan', 'MI_Img_Sign_Brannigan')


@wprop('SM_Sign_Hardware', 'Props/Town', 'box', 'yard', kind='wall', size=(3.0, 0.08, 0.75))
def sign_hardware():
    return _shop_sign('SM_Sign_Hardware', 'MI_Img_Sign_Hardware')


@wprop('SM_Sign_Museum', 'Props/Town', 'box', 'yard', kind='wall', size=(3.0, 0.08, 0.75))
def sign_museum():
    return _shop_sign('SM_Sign_Museum', 'MI_Img_Sign_Museum', museum=True)


@wprop('SM_Sign_Clothing', 'Props/Town', 'box', 'yard', kind='wall', size=(3.0, 0.08, 0.75))
def sign_clothing():
    return _shop_sign('SM_Sign_Clothing', 'MI_Img_Sign_Clothing', frame='MI_Trim_White')


@wprop('SM_Sign_Diner', 'Props/Town', 'box', 'yard', kind='wall', size=(3.0, 0.08, 0.75))
def sign_diner():
    return _shop_sign('SM_Sign_Diner', 'MI_Img_Sign_Diner', frame='MI_Paint_Wood_Red')


@wprop('SM_Sign_Parking', 'Props/Town', 'box', 'yard', size=(0.6, 0.08, 2.4))
def sign_parking():
    b = Builder('SM_Sign_Parking')
    _sign_post(b, 2.36)
    w = 0.6
    cz = 2.4 - w / 2
    pts = [(x, z + cz) for x, z in _rrect(w, w, 0.03, 3)]
    b.prism(pts, 0.003, (0, -0.03, 0), 'MI_Aluminium', axis='Y')
    _face_uv(b, pts, -0.0316, 'MI_Img_Sign_Parking', (-w / 2, cz - w / 2, w / 2, cz + w / 2))
    for dz in (-0.25, 0.25):
        b.cyl(0.008, 0.045, (0, -0.012, cz + dz), 'MI_Steel', rot=(90, 0, 0), segs=8)
        b.cyl(0.011, 0.004, (0, -0.034, cz + dz), 'MI_Steel', rot=(90, 0, 0), segs=8)
    return b
