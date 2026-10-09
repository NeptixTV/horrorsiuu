"""
Cosmetic meshes for the crew body.

Skinned garments are shells cut from the body itself (same vertices, same weights) and pushed out
along the normals, so they deform exactly like the skin underneath. Attached pieces (hair, hats,
masks, bags, accessories) are modelled with signed distance fields around the rest-pose body and
exported in character space; HHCosmeticComponent attaches them to their bone using the inverse
reference pose, so they line up without per-item offsets.
"""
import math
import os

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

import catalog
import character as C
import hhblend as H
import sdf
from hhblend import Builder

OUT_REL = 'SourceArt/Meshes/Cosmetics'

SH = {1: Vector((16.5, 1.5, 144.0)), -1: Vector((-16.5, 1.5, 144.0))}
EL = {1: Vector((36.3, 1.5, 124.2)), -1: Vector((-36.3, 1.5, 124.2))}
WR = {1: Vector((53.4, -1.4, 106.3)), -1: Vector((-53.4, -1.4, 106.3))}


# =======================================================================================
# Skinned shells

class VInfo:
    __slots__ = ('co', 'n', 'w')

    def __init__(self, co, n, w):
        self.co = co
        self.n = n
        self.w = w

    def wsum(self, *prefixes):
        return sum(v for k, v in self.w.items() if k.startswith(prefixes))

    @property
    def side(self):
        return 1 if self.co.x >= 0 else -1

    @property
    def arm_s(self):
        """Distance (cm) along the arm from the shoulder joint."""
        s = self.side
        d1 = (EL[s] - SH[s])
        t1 = (self.co - SH[s]).dot(d1.normalized())
        if t1 <= d1.length:
            return t1
        d2 = (WR[s] - EL[s]).normalized()
        return d1.length + (self.co - EL[s]).dot(d2)

    @property
    def is_arm(self):
        return self.wsum('upperarm', 'lowerarm', 'hand') > 0.5

    @property
    def is_leg(self):
        return self.wsum('thigh', 'calf', 'foot', 'ball') > 0.5

    @property
    def is_head(self):
        return self.w.get('head', 0.0) > 0.5

    @property
    def is_neck(self):
        return self.w.get('neck_01', 0.0) > 0.35


def vertex_infos(body):
    me = body.data
    names = {vg.index: vg.name for vg in body.vertex_groups}
    infos = []
    for v in me.vertices:
        w = {names[g.group]: g.weight for g in v.groups if g.weight > 0.001}
        infos.append(VInfo(v.co.copy(), v.normal.copy(), w))
    return infos


def shell(name, body, arm, keep, offset, slots, face_slot=None, thickness=0.3, post=None, extras=()):
    """keep(VInfo) -> bool, offset(VInfo) -> cm, face_slot(centre, infos_of_face) -> slot index."""
    infos = vertex_infos(body)
    me = body.data.copy()
    me.name = name
    obj = bpy.data.objects.new(name, me)
    H.link(obj)
    for vg in body.vertex_groups:
        obj.vertex_groups.new(name=vg.name)
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.verts.ensure_lookup_table()
    for v in bm.verts:
        info = infos[v.index]
        v.co = info.co + info.n * offset(info)
    if post:
        for v in bm.verts:
            post(v, infos[v.index])
    kill = [f for f in bm.faces if f.material_index >= 1 or not all(keep(infos[v.index]) for v in f.verts)]
    bmesh.ops.delete(bm, geom=kill, context='FACES')
    loose = [v for v in bm.verts if not v.link_faces]
    bmesh.ops.delete(bm, geom=loose, context='VERTS')
    for f in bm.faces:
        f.material_index = 0
        f.smooth = True
    if face_slot:
        for f in bm.faces:
            f.material_index = face_slot(f.calc_center_median(), None)
    bm.to_mesh(me)
    bm.free()
    me.materials.clear()
    for slot in slots:
        me.materials.append(H.material(slot))
    for ex_builder, bone in extras:
        _merge_weighted(obj, ex_builder, bone)
    if thickness > 0:
        sol = obj.modifiers.new('Solidify', 'SOLIDIFY')
        sol.thickness = thickness
        sol.offset = -1.0
        sol.use_rim = True
        sol.use_even_offset = False
        bpy.context.view_layer.objects.active = obj
        H.deselect_all()
        obj.select_set(True)
        bpy.ops.object.modifier_apply(modifier=sol.name)
    C._box_uv_cm(obj.data)
    C.limit_and_normalise(obj)
    mod = obj.modifiers.new('Armature', 'ARMATURE')
    mod.object = arm
    obj.parent = arm
    return obj


def _merge_weighted(obj, builder, bone):
    """Append Builder geometry (cm) to a skinned object, weighted 100 % to 'bone'."""
    me = obj.data
    tmp = builder.mesh(sharp_angle=50)
    slot_map = []
    for m in tmp.materials:
        if m.name not in [x.name for x in me.materials]:
            me.materials.append(m)
        slot_map.append([x.name for x in me.materials].index(m.name))
    bm = bmesh.new()
    bm.from_mesh(me)
    n_before = len(bm.verts)
    bm.from_mesh(tmp)
    bm.verts.ensure_lookup_table()
    deform = bm.verts.layers.deform.verify()
    gi = obj.vertex_groups[bone].index
    new_verts = bm.verts[n_before:]
    new_set = set(new_verts)
    for v in new_verts:
        v[deform].clear()
        v[deform][gi] = 1.0
    for f in bm.faces:
        if f.verts[0] in new_set:
            f.material_index = slot_map[f.material_index] if f.material_index < len(slot_map) else 0
    bm.to_mesh(me)
    bm.free()
    bpy.data.meshes.remove(tmp)


# Region predicates ------------------------------------------------------------------------

def torso(v, z_lo, z_hi=151.0, neckline=True):
    if v.is_leg or v.is_head or v.is_arm:
        return False
    if v.is_neck and v.co.z > 147.5:
        return False
    if neckline and v.co.z > 146.0 and abs(v.co.x) < 7.0 and v.co.y < -2.0:
        return False
    return z_lo <= v.co.z <= z_hi


def sleeve(v, length):
    return v.is_arm and v.arm_s < length


def legs(v, z_lo, waist=101.0):
    if v.is_leg:
        return v.co.z >= z_lo
    return (not v.is_arm and not v.is_head) and v.co.z <= waist and v.co.z >= 84


def feet(v, z_hi):
    return v.wsum('foot', 'ball', 'calf') > 0.5 and v.co.z <= z_hi


def stripes(width=5.0):
    def fn(centre, _):
        if abs(centre.x) > 18:   # arms: stripes along the arm
            s = 1 if centre.x > 0 else -1
            t = (centre - SH[s]).length
            return int(t // width) % 2
        return int(centre.z // width) % 2
    return fn


# =======================================================================================
# Garments

def _box(size, loc, mat, rot=(0, 0, 0), bevel=0.4):
    b = Builder('_extra')
    b.box(size, loc, mat, rot=rot, bevel=bevel)
    return b


def build_garments(body, arm):
    out = {}

    out['SK_HH_Top_Tee'] = shell('SK_HH_Top_Tee', body, arm,
                                 lambda v: torso(v, 96.5) or sleeve(v, 12.5), lambda v: 0.55, ['MI_Char_Cotton'])

    out['SK_HH_Top_Sweater'] = shell('SK_HH_Top_Sweater', body, arm,
                                     lambda v: torso(v, 95.0) or sleeve(v, 50.0), lambda v: 0.95,
                                     ['MI_Char_Knit', 'MI_Char_Knit2'], face_slot=stripes(4.5), thickness=0.4)

    def hood_keep(v):
        if v.is_head:
            return (v.co.y > -5.5 or v.co.z > 172.5) and v.co.z > 150
        if v.is_neck:
            return True
        return torso(v, 94.0, 152.0, neckline=False) or sleeve(v, 50.0)

    def hood_offset(v):
        if v.is_head:
            return 2.4
        if v.is_neck:
            return 1.9
        return 1.15

    pocket = _box((17, 1.2, 9), (0, -10.1, 104.5), 'MI_Char_Cotton', rot=(8, 0, 0))
    out['SK_HH_Top_Hoodie'] = shell('SK_HH_Top_Hoodie', body, arm, hood_keep, hood_offset, ['MI_Char_Cotton'],
                                    thickness=0.45, extras=[(pocket, 'spine_01')])

    def turtle_keep(v):
        if v.is_neck and not v.is_head and v.co.z < 158.0:
            return True
        return torso(v, 95.0, 152.0, neckline=False) or sleeve(v, 50.5)
    out['SK_HH_Top_Turtleneck'] = shell('SK_HH_Top_Turtleneck', body, arm, turtle_keep,
                                        lambda v: 1.0 if v.is_neck else 0.7, ['MI_Char_Knit'])

    # Jackets sit outside every top.
    def jacket(z_lo, arms=True, collar=True):
        def keep(v):
            if collar and v.is_neck and not v.is_head and v.co.z < 152.0 and v.co.y > -4.0:
                return True
            return torso(v, z_lo, 153.0, neckline=True) or (arms and sleeve(v, 49.0))
        return keep

    pockets = Builder('_extra')
    for s in (-1, 1):
        pockets.box((7.5, 1.2, 7.0), (s * 8.5, -12.0, 128.0), 'MI_Char_Fabric', rot=(6, 0, 0), bevel=0.4)
        pockets.box((8.0, 1.2, 9.0), (s * 9.5, -11.5, 100.0), 'MI_Char_Fabric', rot=(-4, 0, 0), bevel=0.4)
    out['SK_HH_Jacket_Work'] = shell('SK_HH_Jacket_Work', body, arm, jacket(88.0), lambda v: 2.0,
                                     ['MI_Char_Fabric'], thickness=0.5, extras=[(pockets, 'spine_02')])
    out['SK_HH_Jacket_Bomber'] = shell('SK_HH_Jacket_Bomber', body, arm, jacket(95.0), lambda v: 2.4 + 0.3 * math.sin(v.co.z * 0.6),
                                       ['MI_Char_Nylon', 'MI_Char_Knit'], thickness=0.5,
                                       face_slot=lambda c, _: 1 if c.z < 98.5 or (abs(c.x) > 18 and (c - SH[1 if c.x > 0 else -1]).length > 46) else 0)
    out['SK_HH_Jacket_Rain'] = shell('SK_HH_Jacket_Rain', body, arm, jacket(80.0), lambda v: 2.3,
                                     ['MI_Char_Nylon'], thickness=0.5)
    out['SK_HH_Jacket_Leather'] = shell('SK_HH_Jacket_Leather', body, arm, jacket(91.0), lambda v: 1.9,
                                        ['MI_Char_Leather'], thickness=0.5)
    pouches = Builder('_extra')
    for x in (-9.5, -3.2, 3.2, 9.5):
        pouches.box((5.6, 3.2, 8.5), (x, -14.0, 116.0), 'MI_Char_Nylon', bevel=0.8)
    pouches.box((6.0, 2.5, 5.0), (8.5, -13.5, 133.0), 'MI_Char_Nylon', bevel=0.6)
    out['SK_HH_Jacket_Vest'] = shell('SK_HH_Jacket_Vest', body, arm, jacket(97.0, arms=False, collar=False),
                                     lambda v: 2.8, ['MI_Char_Nylon'], thickness=0.6, extras=[(pouches, 'spine_02')])

    out['SK_HH_Gloves'] = shell('SK_HH_Gloves', body, arm, lambda v: v.is_arm and v.arm_s > 49.5,
                                lambda v: 0.5, ['MI_Char_Leather'], thickness=0.25)
    out['SK_HH_Gloves_Thin'] = shell('SK_HH_Gloves_Thin', body, arm, lambda v: v.is_arm and v.arm_s > 50.5,
                                     lambda v: 0.22, ['MI_Char_Plastic'], thickness=0.12)

    belt = Builder('_extra')
    belt.torus(15.0, 0.75, (0, 1.2, 99.5), 'MI_Char_Leather', scale=(1.0, 10.6 / 15.0, 1.0), segs=40, rsegs=6)
    belt.box((4.2, 1.0, 3.2), (0, -9.9, 99.5), 'MI_Char_Metal', bevel=0.3)
    out['SK_HH_Pants_Jeans'] = shell('SK_HH_Pants_Jeans', body, arm, lambda v: legs(v, 11.0), lambda v: 0.75,
                                     ['MI_Char_Denim'], extras=[(belt, 'pelvis')])
    cargo = Builder('_extra')
    for s in (-1, 1):
        cargo.box((2.8, 10.0, 12.0), (s * 17.0, 0.0, 68.0), 'MI_Char_Fabric', bevel=0.8)
    out['SK_HH_Pants_Cargo'] = shell('SK_HH_Pants_Cargo', body, arm, lambda v: legs(v, 11.0), lambda v: 1.15,
                                     ['MI_Char_Fabric'], extras=[(cargo, 'pelvis')])
    out['SK_HH_Pants_Slacks'] = shell('SK_HH_Pants_Slacks', body, arm, lambda v: legs(v, 10.0), lambda v: 0.9,
                                      ['MI_Char_Fabric'])

    def sole(v_bm, info):
        if v_bm.co.z < 0.0:
            v_bm.co.z = 0.0

    out['SK_HH_Shoes_Sneakers'] = shell('SK_HH_Shoes_Sneakers', body, arm, lambda v: feet(v, 12.5),
                                        lambda v: 1.0, ['MI_Char_Cotton', 'MI_Char_Rubber2'],
                                        face_slot=lambda c, _: 1 if c.z < 2.8 else 0, post=sole, thickness=0.4)
    out['SK_HH_Shoes_Boots'] = shell('SK_HH_Shoes_Boots', body, arm, lambda v: feet(v, 21.0),
                                     lambda v: 1.25, ['MI_Char_Leather', 'MI_Char_Rubber'],
                                     face_slot=lambda c, _: 1 if c.z < 3.2 else 0, post=sole, thickness=0.5)
    out['SK_HH_Shoes_Dress'] = shell('SK_HH_Shoes_Dress', body, arm, lambda v: feet(v, 10.5),
                                     lambda v: 0.75, ['MI_Char_Leather'], post=sole, thickness=0.35)
    return out


# =======================================================================================
# Attached pieces (SDF)

def head_into(F, g, ears=True, neck=False, nose=False, k=3.0, op='union'):
    F.ellipsoid((0, 0.4, 168.6), (8.5 + g, 9.9 + g, 10.4 + g), op=op)
    F.ellipsoid((0, -3.4, 163.2), (6.5 + g, 7.6 + g, 8.4 + g), k=k, op=op)
    F.ellipsoid((0, -5.6, 157.8), (5.0 + g, 4.8 + g, 3.4 + g), k=k * 0.8, op=op)
    if ears:
        for s in (-1, 1):
            F.ellipsoid((s * 7.9, 0.4, 165.4), (1.3 + g, 2.4 + g, 3.2 + g), k=1.2, op=op)
    if nose:
        F.cone((0, -9.2, 167.8), (0, -10.7, 163.9), 0.9 + g, 1.45 + g, k=1.2, op=op)
        F.sphere((0, -10.5, 163.4), 1.45 + g, k=1.0, op=op)
    if neck:
        F.cone((0, 1.6, 140), (0, 0.6, 158), 6.0 + g, 5.1 + g, k=4, op=op)


HEAD_BOX = ((-17, -20, 145), (17, 21, 189))


def field(lo=HEAD_BOX[0], hi=HEAD_BOX[1], h=0.5):
    return sdf.Field(lo, hi, h)


def shell_of(outer, inner):
    outer.d = np.maximum(outer.d, -inner.d)
    return outer


def sdf_object(name, F, mat, ratio=1.0, smooth=1):
    verts, quads = F.mesh()
    me = sdf.to_blender_mesh(name, verts, quads)
    obj = bpy.data.objects.new(name, me)
    H.link(obj)
    bpy.context.view_layer.objects.active = obj
    H.deselect_all()
    obj.select_set(True)
    if ratio < 1.0:
        dec = obj.modifiers.new('Decimate', 'DECIMATE')
        dec.ratio = ratio
    if smooth:
        sm = obj.modifiers.new('Smooth', 'LAPLACIANSMOOTH')
        sm.lambda_factor = 0.3
        sm.iterations = smooth
        sm.use_volume_preserve = True
    for mod in list(obj.modifiers):
        bpy.ops.object.modifier_apply(modifier=mod.name)
    obj.data.shade_smooth()
    C._box_uv_cm(obj.data)
    obj.data.materials.append(H.material(mat))
    return obj


def join(objs, name):
    H.deselect_all()
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    obj.name = name
    obj.data.name = name
    return obj


def builder_object(b):
    obj = H.mesh_object(b.mesh(sharp_angle=45), weighted=False)
    obj.data.shade_smooth()
    H.link(obj)
    return obj


def hairline(F, front_z=170.8, slope=0.62):
    F.plane_cut((0, -9.0, front_z), (0, slope, 1.0), k=0.8)


def hair_crop():
    F = field()
    head_into(F, 1.0, ears=False)
    hairline(F)
    return sdf_object('SM_HH_Hair_Crop', F, 'MI_Char_Hair')


def hair_messy():
    F = field()
    head_into(F, 1.5, ears=False)
    rng = np.random.default_rng(7)
    for _ in range(46):
        th = rng.uniform(0, 2 * math.pi)
        ph = rng.uniform(0.0, 1.15)
        c = np.array((0, 0.4, 168.6)) + np.array((9.5 * math.sin(ph) * math.cos(th), 10.8 * math.sin(ph) * math.sin(th),
                                                   11.0 * math.cos(ph)))
        F.sphere(c, rng.uniform(1.6, 2.8), k=1.6)
    for x in (-4.5, -1.5, 1.8, 4.6):                                       # fringe
        F.cone((x * 0.9, -6.5, 177.0), (x, -10.4, 170.0 + abs(x) * 0.15), 1.8, 0.7, k=1.4)
    hairline(F, 169.6, 0.6)
    return sdf_object('SM_HH_Hair_Messy', F, 'MI_Char_Hair')


def hair_tied():
    F = field((-17, -20, 132), (17, 26, 189))
    head_into(F, 0.9, ears=False)
    hairline(F, 171.2, 0.72)
    F.sphere((0, 10.8, 165.5), 2.6, k=1.5)                                  # tie
    F.cone((0, 11.5, 164.5), (0, 15.0, 150.0), 2.9, 1.2, k=1.5)            # ponytail
    F.cone((0, 15.0, 150.0), (0, 14.0, 141.0), 1.2, 0.4, k=1.0)
    return sdf_object('SM_HH_Hair_Tied', F, 'MI_Char_Hair')


def hair_curly():
    F = field()
    head_into(F, 2.6, ears=False)
    rng = np.random.default_rng(11)
    for _ in range(90):
        th = rng.uniform(0, 2 * math.pi)
        ph = rng.uniform(0.0, 1.5)
        c = np.array((0, 0.6, 168.8)) + np.array((11.2 * math.sin(ph) * math.cos(th), 12.4 * math.sin(ph) * math.sin(th),
                                                   12.6 * math.cos(ph)))
        F.sphere(c, rng.uniform(1.8, 2.6), k=0.9)
    hairline(F, 170.0, 0.45)
    return sdf_object('SM_HH_Hair_Curly', F, 'MI_Char_Hair')


def hat_beanie():
    F = field()
    head_into(F, 1.9, ears=False)
    F.plane_cut((0, -9.0, 169.6), (0, 0.42, 1.0), k=0.6)
    rim = field()
    head_into(rim, 2.9, ears=False)
    rim.plane_cut((0, -9.0, 169.4), (0, 0.42, 1.0), k=0.5)
    rim.plane_cut((0, -9.0, 173.6), (0, -0.42, -1.0), k=0.5)
    F.d = sdf.smin(F.d, rim.d, 0.6)
    return sdf_object('SM_HH_Hat_Beanie', F, 'MI_Char_Knit')


def hat_cap():
    F = field()
    head_into(F, 1.3, ears=False)
    F.plane_cut((0, -9.0, 171.0), (0, 0.18, 1.0), k=0.6)
    F.sphere((0, 0.2, 179.6), 0.9, k=0.4)
    crown = sdf_object('SM_HH_Hat_Cap', F, 'MI_Char_Cotton')
    B = field((-17, -27, 160), (17, 0, 181))
    B.ellipsoid((0, -14.5, 171.2), (8.6, 8.2, 0.55))
    B.global_op(lambda P: P[..., 1] + 6.5, 'intersect', k=0.3)
    brim = sdf_object('_brim', B, 'MI_Char_Cotton2')
    return join([crown, brim], 'SM_HH_Hat_Cap')


def hat_flatcap():
    F = field((-17, -25, 145), (17, 21, 189))
    head_into(F, 1.4, ears=False)
    F.ellipsoid((0, -5.0, 175.0), (10.4, 13.5, 3.8), k=2.5)
    F.plane_cut((0, -9.0, 171.2), (0, 0.22, 1.0), k=0.6)
    F.ellipsoid((0, -15.2, 171.4), (7.5, 4.2, 0.6), k=1.2)
    return sdf_object('SM_HH_Hat_FlatCap', F, 'MI_Char_Fabric')


def mask_balaclava():
    F = field((-17, -20, 137), (17, 21, 189))
    head_into(F, 0.75, ears=True, neck=True, nose=True)
    F.plane_cut((0, 0, 143.0), (0, 0, 1.0), k=0.5)
    F.box((0, -11.0, 166.0), (5.4, 4.0, 1.6), radius=1.2, k=0.6, op='subtract')
    return sdf_object('SM_HH_Mask_Balaclava', F, 'MI_Char_Knit')


def mask_paperbag():
    F = field((-19, -20, 145), (19, 20, 192), h=0.6)
    F.box((0, -0.5, 170.5), (11.8, 13.0, 15.5), radius=1.2)
    F.box((0, -0.5, 168.0), (11.2, 12.4, 15.5), radius=1.0, op='subtract')
    rng = np.random.default_rng(3)
    for _ in range(30):
        c = np.array((rng.uniform(-11, 11), rng.choice([-13.4, 12.4]), rng.uniform(158, 184)))
        F.sphere(c, rng.uniform(0.6, 1.3), k=0.8)
    for s in (-1, 1):
        F.ellipsoid((s * 3.6, -13.0, 166.4), (1.6, 3.0, 1.3), op='subtract', k=0.2)
    F.ellipsoid((0, -13.0, 159.5), (3.0, 3.0, 0.9), op='subtract', k=0.2)
    return sdf_object('SM_HH_Mask_PaperBag', F, 'MI_Char_Paper')


def face_shell(g_out, g_in, z0, z1, y_max):
    F = field()
    head_into(F, g_out, ears=False, nose=True)
    I = field()
    head_into(I, g_in, ears=False, nose=True)
    shell_of(F, I)
    F.global_op(lambda P: P[..., 1] - y_max, 'intersect', k=0.6)
    F.global_op(lambda P: np.maximum(z0 - P[..., 2], P[..., 2] - z1), 'intersect', k=0.6)
    return F


def mask_hockey():
    F = face_shell(1.3, 0.5, 154.5, 177.5, -3.0)
    for s in (-1, 1):
        F.ellipsoid((s * 3.5, -11.0, 166.6), (1.9, 3.0, 1.2), op='subtract', k=0.3)
    for x, z in ((-3.5, 160), (0, 159), (3.5, 160), (-2, 157), (2, 157), (-5.5, 163), (5.5, 163), (0, 175), (-2.5, 174),
                 (2.5, 174)):
        F.global_op(lambda P, x=x, z=z: 0.55 - np.sqrt((P[..., 0] - x) ** 2 + (P[..., 2] - z) ** 2), 'intersect', k=0.1)
    return sdf_object('SM_HH_Mask_Hockey', F, 'MI_Char_Plastic')


def mask_doll():
    F = face_shell(1.2, 0.5, 155.5, 176.5, -2.5)
    for s in (-1, 1):
        F.ellipsoid((s * 3.4, -11.0, 166.3), (1.5, 3.0, 0.9), op='subtract', k=0.4)
    face = sdf_object('SM_HH_Mask_Doll', F, 'MI_Char_Plastic')
    P = field()
    for s in (-1, 1):
        P.ellipsoid((s * 4.8, -10.7, 162.6), (1.7, 0.45, 1.3))
    P.ellipsoid((0, -11.2, 159.6), (1.4, 0.45, 0.55))
    paint = sdf_object('_paint', P, 'MI_Char_Plastic2', smooth=0)
    return join([face, paint], 'SM_HH_Mask_Doll')


def mask_domino():
    F = face_shell(0.9, 0.4, 163.8, 169.2, -3.5)
    for s in (-1, 1):
        F.ellipsoid((s * 3.4, -11.0, 166.4), (1.6, 3.0, 1.1), op='subtract', k=0.3)
    return sdf_object('SM_HH_Mask_Domino', F, 'MI_Char_Fabric')


def mask_gas():
    F = face_shell(1.4, 0.4, 153.0, 175.5, -1.0)
    F.cone((0, -11.0, 159.5), (0, -15.5, 157.5), 3.2, 2.6, k=1.2)
    rubber = sdf_object('SM_HH_Mask_Gas', F, 'MI_Char_Rubber')
    b = Builder('_gas')
    for s in (-1, 1):
        b.cyl(2.2, 0.8, (s * 3.5, -11.6, 166.4), 'MI_Char_Glass', rot=(90, 0, 0), segs=20)
        b.torus(2.3, 0.45, (s * 3.5, -11.6, 166.4), 'MI_Char_Metal', rot=(90, 0, 0), segs=20, rsegs=6)
    b.cyl(3.0, 6.5, (5.5, -15.5, 155.0), 'MI_Char_Metal', rot=(80, 0, -30), segs=20)
    b.cyl(2.6, 1.0, (0, -16.8, 157.2), 'MI_Char_Metal', rot=(80, 0, 0), segs=16)
    for z in (171.0, 160.0):
        b.torus(10.3, 0.35, (0, 1.5, z), 'MI_Char_Rubber', scale=(1.0, 11.0 / 10.3, 1.0), rot=(-8, 0, 0), segs=36, rsegs=5)
    parts = builder_object(b)
    return join([rubber, parts], 'SM_HH_Mask_Gas')


# Bags ------------------------------------------------------------------------------------

def _straps(b, mat, back_y=10.5, bottom_z=112.0):
    for s in (-1, 1):
        pts = [(s * 7.5, back_y + 1.0, 141.0), (s * 9.0, 4.0, 150.5), (s * 9.5, -6.0, 148.0), (s * 9.5, -11.4, 136.0),
               (s * 9.5, -10.4, 120.0), (s * 12.0, -2.0, 112.0), (s * 11.0, back_y + 1.5, bottom_z)]
        b.tube(pts, 0.55, mat, segs=6, bend=3.0)


def back_rucksack():
    F = sdf.Field((-18, 6, 100), (18, 32, 152), 0.6)
    F.box((0, 16.5, 126.0), (12.5, 6.5, 17.0), radius=4.5)
    F.box((0, 16.0, 142.0), (12.0, 7.0, 3.0), radius=2.5, k=1.2)                 # lid
    F.box((0, 23.5, 118.0), (8.5, 2.5, 7.5), radius=2.0, k=1.5)                  # front pocket
    bag = sdf_object('SM_HH_Back_Rucksack', F, 'MI_Char_Nylon')
    b = Builder('_straps')
    _straps(b, 'MI_Char_Nylon')
    b.box((1.2, 0.6, 1.6), (0, 26.1, 121.0), 'MI_Char_Metal', bevel=0.2)
    return join([bag, builder_object(b)], 'SM_HH_Back_Rucksack')


def back_tactical():
    F = sdf.Field((-22, 6, 98), (22, 32, 154), 0.6)
    F.box((0, 16.0, 126.0), (13.5, 6.0, 19.0), radius=2.0)
    for s in (-1, 1):
        F.box((s * 15.5, 15.0, 117.0), (3.0, 4.5, 7.0), radius=1.5, k=1.0)
    F.box((0, 23.0, 120.0), (9.5, 2.5, 9.0), radius=1.5, k=0.8)
    bag = sdf_object('SM_HH_Back_Tactical', F, 'MI_Char_Nylon')
    b = Builder('_straps')
    _straps(b, 'MI_Char_Nylon2')
    for z in (112, 117, 122, 127, 132, 137):
        b.box((24.0, 0.5, 1.2), (0, 22.3, z), 'MI_Char_Nylon2', bevel=0.15)
    return join([bag, builder_object(b)], 'SM_HH_Back_Tactical')


def back_duffel():
    F = sdf.Field((-29, 4, 88), (29, 34, 152), 0.65)
    F.cone((-17.0, 17.0, 104.0), (16.0, 17.0, 137.0), 9.0, 9.0)
    bag = sdf_object('SM_HH_Back_Duffel', F, 'MI_Char_Nylon')
    b = Builder('_strap')
    b.tube([(13.0, 9.0, 139.0), (8.0, 3.0, 151.0), (0.0, -9.0, 145.0), (-8.0, -12.0, 128.0), (-14.0, -6.0, 113.0),
            (-15.0, 9.0, 108.0)], 0.6, 'MI_Char_Nylon', segs=6, bend=3.0)
    b.tube([(-24.0, 17.0, 108.0), (22.0, 17.0, 132.0)], 0.35, 'MI_Char_Metal', segs=6)
    return join([bag, builder_object(b)], 'SM_HH_Back_Duffel')


# Accessories ------------------------------------------------------------------------------

def acc_glasses():
    b = Builder('SM_HH_Acc_Glasses')
    for s in (-1, 1):
        b.torus(1.9, 0.22, (s * 3.5, -11.3, 166.4), 'MI_Char_Plastic', rot=(90, 0, 0), segs=24, rsegs=6)
        b.cyl(1.85, 0.12, (s * 3.5, -11.3, 166.4), 'MI_Char_Glass', rot=(90, 0, 0), segs=24)
        b.tube([(s * 5.4, -11.2, 166.8), (s * 8.4, -8.0, 167.2), (s * 8.7, 0.5, 167.0), (s * 8.4, 2.4, 164.0)], 0.18,
               'MI_Char_Plastic', segs=5, bend=1.0)
    b.tube([(-1.6, -11.3, 166.9), (0, -11.9, 167.3), (1.6, -11.3, 166.9)], 0.2, 'MI_Char_Plastic', segs=5, bend=0.6)
    obj = builder_object(b)
    return obj


def acc_earpiece():
    b = Builder('SM_HH_Acc_Earpiece')
    b.sphere(0.95, (-8.9, -0.2, 165.0), 'MI_Char_Plastic', scale=(0.8, 1.0, 1.1), segs=12, rings=8)
    pts = [(-8.9, 0.5, 164.0)]
    for i in range(1, 22):
        t = i / 21
        pts.append((-8.6 + 0.9 * math.sin(t * 40), 2.0 + 0.9 * math.cos(t * 40) + t * 2.0, 163.5 - t * 15.0))
    b.tube(pts, 0.14, 'MI_Char_Plastic', segs=5, caps=True)
    return builder_object(b)


def acc_watch():
    b = Builder('SM_HH_Acc_Watch')
    el, wr = EL[1], WR[1]
    d = (wr - el).normalized()
    c = el + d * 21.0
    q = Vector((0, 0, 1)).rotation_difference(d)
    rot = [math.degrees(a) for a in q.to_euler()]
    b.torus(3.05, 0.55, tuple(c), 'MI_Char_Leather', rot=rot, segs=24, rsegs=6, scale=(1.0, 0.82, 1.6))
    up = d.cross(Vector((0, 1, 0))).normalized()      # back of the wrist
    face = c + up * 2.6
    q2 = Vector((0, 0, 1)).rotation_difference(up)
    rot2 = [math.degrees(a) for a in q2.to_euler()]
    b.cyl(1.55, 0.6, tuple(face), 'MI_Char_Metal', rot=rot2, segs=20)
    b.cyl(1.35, 0.15, tuple(face + up * 0.34), 'MI_Char_Glass', rot=rot2, segs=20)
    return builder_object(b)


def acc_chain():
    b = Builder('SM_HH_Acc_Chain')
    pts = []
    for i in range(41):
        a = 2 * math.pi * i / 40
        x = 7.6 * math.sin(a)
        y = 1.0 - 7.4 * math.cos(a)
        z = 146.5 - 6.5 * max(0.0, math.cos(a)) ** 2
        y -= 2.2 * max(0.0, math.cos(a)) ** 2
        pts.append((x, y, z))
    b.tube(pts, 0.32, 'MI_Char_Metal', segs=6, caps=False)
    return builder_object(b)


ATTACHED = {
    'SM_HH_Hair_Crop': hair_crop, 'SM_HH_Hair_Messy': hair_messy, 'SM_HH_Hair_Tied': hair_tied,
    'SM_HH_Hair_Curly': hair_curly, 'SM_HH_Hat_Beanie': hat_beanie, 'SM_HH_Hat_Cap': hat_cap,
    'SM_HH_Hat_FlatCap': hat_flatcap, 'SM_HH_Mask_Balaclava': mask_balaclava, 'SM_HH_Mask_PaperBag': mask_paperbag,
    'SM_HH_Mask_Hockey': mask_hockey, 'SM_HH_Mask_Doll': mask_doll, 'SM_HH_Mask_Domino': mask_domino,
    'SM_HH_Mask_Gas': mask_gas, 'SM_HH_Back_Rucksack': back_rucksack, 'SM_HH_Back_Tactical': back_tactical,
    'SM_HH_Back_Duffel': back_duffel, 'SM_HH_Acc_Glasses': acc_glasses, 'SM_HH_Acc_Earpiece': acc_earpiece,
    'SM_HH_Acc_Watch': acc_watch, 'SM_HH_Acc_Chain': acc_chain,
}


# =======================================================================================
# Build, export, icons

def build_all(body, arm, manifest, preview=False, only=None):
    import build_character as BC
    garments = build_garments(body, arm)
    attached = {}
    for name, fn in ATTACHED.items():
        if only and name not in only:
            continue
        obj = fn()
        obj.name = name
        obj.data.name = name
        attached[name] = obj
        print('built', name, len(obj.data.polygons), flush=True)
    for name, obj in garments.items():
        rel = f'{OUT_REL}/{name}.fbx'
        BC.export_skeletal([obj], arm, os.path.join(H.PROJECT, rel))
        manifest.add(name, rel, [m.name for m in obj.data.materials], 'Cosmetics', 'none', False, kind='skeletal',
                     extra=dict(skeleton='SK_HH_Body'))
        print('exported', name, flush=True)
    for name, obj in attached.items():
        rel = f'{OUT_REL}/{name}.fbx'
        H.export_mesh_object(obj, os.path.join(H.PROJECT, rel))
        manifest.add(name, rel, [m.name for m in obj.data.materials], 'Cosmetics', 'none', False)
        print('exported', name, flush=True)
    render_icons(body, arm, garments, attached)
    if preview:
        preview_outfits(body, arm, garments, attached)
    return garments, attached


# -- Icons ------------------------------------------------------------------------------------

ICON_DIR = os.path.join(H.PROJECT, 'SourceArt', 'Textures', 'Icons')


def icon_scene():
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'CPU'
    scene.cycles.samples = 32
    scene.cycles.use_denoising = True
    scene.render.resolution_x = 256
    scene.render.resolution_y = 256
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGBA'
    world = bpy.data.worlds.get('IconWorld') or bpy.data.worlds.new('IconWorld')
    world.color = (0.18, 0.17, 0.16)
    scene.world = world
    try:
        scene.view_settings.view_transform = 'AgX'
    except TypeError:
        pass
    cam = bpy.data.objects.get('IconCam')
    if cam is None:
        cam = H.link(bpy.data.objects.new('IconCam', bpy.data.cameras.new('IconCam')))
        for name, energy, rot in (('Key', 4.0, (55, 0, -40)), ('Rim', 2.5, (60, 0, 150)), ('Fill', 1.0, (80, 0, 60))):
            sun = bpy.data.lights.new('Icon' + name, 'SUN')
            sun.energy = energy
            sun.color = (1.0, 0.86, 0.7) if name == 'Key' else (0.7, 0.8, 1.0)
            so = H.link(bpy.data.objects.new('Icon' + name, sun))
            so.rotation_euler = [math.radians(a) for a in rot]
    scene.camera = cam
    return scene, cam


def frame_objects(cam, objs, direction=(0.55, -1.0, 0.25), margin=1.15, lens=85, keep=None):
    pts = []
    for o in objs:
        dg = bpy.context.evaluated_depsgraph_get()
        eo = o.evaluated_get(dg)
        me = eo.to_mesh()
        pts.extend(p for p in (eo.matrix_world @ v.co for v in me.vertices) if keep is None or keep(p))
        eo.to_mesh_clear()
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    centre = (lo + hi) / 2
    radius = (hi - lo).length / 2 * margin
    d = Vector(direction).normalized()
    cam.data.lens = lens
    fov = 2 * math.atan(18.0 / lens)
    dist = radius / math.sin(fov / 2)
    cam.location = centre + d * dist
    cam.rotation_euler = (centre - cam.location).to_track_quat('-Z', 'Y').to_euler()
    cam.data.clip_start = dist * 0.05
    cam.data.clip_end = dist * 4


def tinted(obj, tint, tint2):
    """Temporarily swap materials for tinted copies (preview only)."""
    saved = [s.material for s in obj.material_slots]
    for slot in obj.material_slots:
        base = slot.material
        if base is None:
            continue
        spec = H.CATALOG.get(base.name, {})
        colour = tint2 if spec.get('use_tint2') else tint
        if colour is None or spec.get('master') not in ('Character', 'Surface'):
            continue
        m = base.copy()
        for node in m.node_tree.nodes:
            if node.bl_idname == 'ShaderNodeVectorMath' and node.operation == 'MULTIPLY' and not node.inputs[1].is_linked:
                node.inputs[1].default_value = colour
        slot.material = m
    return saved


def restore(obj, saved):
    for slot, mat in zip(obj.material_slots, saved):
        slot.material = mat


def render_icons(body, arm, garments, attached):
    os.makedirs(ICON_DIR, exist_ok=True)
    scene, cam = icon_scene()
    everything = [body] + list(garments.values()) + list(attached.values())
    for o in everything:
        o.hide_render = True
    arm.animation_data.action = None
    for pb in arm.pose.bones:
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.location = (0, 0, 0)
    for item in catalog.COSMETICS:
        mesh = item['mesh']
        shown = []
        head_ctx = item['slot'] in ('Hair', 'Hat', 'Mask', 'Accessory') and item['bone'] == 'head'
        if item['skin']:
            shown = [body]
        elif mesh in garments:
            shown = [garments[mesh]]
        elif mesh in attached:
            shown = [attached[mesh]]
        if not shown:
            continue
        ctx = [body] if head_ctx and body not in shown else []
        saved = {}
        for o in shown:
            o.hide_render = False
            saved[o] = tinted(o, item['tint'], item['tint2'])
        for o in ctx:
            o.hide_render = False
            saved[o] = tinted(o, (0.42, 0.40, 0.38), None)
        if item['slot'] == 'Gloves':
            frame_objects(cam, shown, direction=(0.9, -1.0, 0.35), keep=lambda p: p.x > 40)
        elif mesh == 'SM_HH_Acc_Earpiece':
            frame_objects(cam, [body], direction=(-1.0, -0.55, 0.15), margin=1.0, keep=lambda p: p.z > 150)
        elif item['skin'] or head_ctx:
            bust = [attached[mesh]] if mesh in attached else []
            frame_objects(cam, bust or [body], direction=(0.6, -1.0, 0.12), margin=1.25 if bust else 1.0)
            if not bust:
                cam.location = Vector((22, -55, 172))
                cam.rotation_euler = (Vector((0, 0, 162)) - cam.location).to_track_quat('-Z', 'Y').to_euler()
                cam.data.lens = 70
        else:
            frame_objects(cam, shown)
        scene.render.filepath = os.path.join(ICON_DIR, f"T_Icon_{item['id']}.png")
        bpy.ops.render.render(write_still=True)
        for o in shown + ctx:
            restore(o, saved[o])
            o.hide_render = True
        print('icon', item['id'], flush=True)
    for o in everything:
        o.hide_render = False


def preview_outfits(body, arm, garments, attached):
    """A line-up of outfits for QA (SourceArt/Previews/Character_Outfits.jpg)."""
    import build_character as BC
    scene = bpy.context.scene
    scene.render.film_transparent = False
    cam = BC.preview_setup(scene)
    looks = [
        ('SK_HH_Top_Tee', 'SK_HH_Pants_Jeans', 'SK_HH_Shoes_Sneakers', 'SM_HH_Hair_Crop', None),
        ('SK_HH_Top_Hoodie', 'SK_HH_Pants_Cargo', 'SK_HH_Shoes_Boots', 'SM_HH_Mask_Hockey', 'SM_HH_Back_Rucksack'),
        ('SK_HH_Top_Sweater', 'SK_HH_Jacket_Rain', 'SK_HH_Pants_Slacks', 'SM_HH_Hair_Tied', 'SK_HH_Shoes_Dress'),
        ('SK_HH_Top_Turtleneck', 'SK_HH_Jacket_Leather', 'SK_HH_Pants_Jeans', 'SM_HH_Mask_Balaclava', 'SK_HH_Gloves'),
        ('SK_HH_Top_Tee', 'SK_HH_Jacket_Vest', 'SK_HH_Pants_Cargo', 'SM_HH_Mask_Gas', 'SM_HH_Back_Tactical'),
        ('SK_HH_Top_Hoodie', 'SK_HH_Jacket_Bomber', 'SK_HH_Pants_Jeans', 'SM_HH_Hat_Beanie', 'SM_HH_Acc_Glasses'),
        ('SK_HH_Top_Tee', 'SK_HH_Jacket_Work', 'SK_HH_Pants_Jeans', 'SM_HH_Hat_Cap', 'SM_HH_Back_Duffel'),
        ('SK_HH_Top_Turtleneck', 'SK_HH_Pants_Slacks', 'SK_HH_Shoes_Dress', 'SM_HH_Mask_Doll', 'SM_HH_Hair_Curly'),
    ]
    tmp = os.path.join(H.PROJECT, 'SourceArt', 'Previews', '_char')
    paths = []
    allobjs = list(garments.values()) + list(attached.values())
    walk = bpy.data.actions.get('A_HH_Walk')
    for i, look in enumerate(looks):
        for o in allobjs:
            o.hide_render = o.name not in look
        for pose in (0, 1):
            if pose == 1 and walk:
                arm.animation_data.action = walk
                if hasattr(arm.animation_data, 'action_slot') and walk.slots:
                    arm.animation_data.action_slot = walk.slots[0]
                scene.frame_set(6)
                BC.aim(cam, (300, -420, 120), (0, 0, 90))
            else:
                arm.animation_data.action = None
                for pb in arm.pose.bones:
                    pb.rotation_quaternion = (1, 0, 0, 0)
                    pb.location = (0, 0, 0)
                BC.aim(cam, (160, -500, 110), (0, 0, 92))
            path = os.path.join(tmp, f'look_{i}_{pose}.png')
            BC.render(scene, path)
            paths.append(path)
    BC.preview_sheet(paths, os.path.join(H.PROJECT, 'SourceArt', 'Previews', 'Character_Outfits.jpg'), 8)
    for o in allobjs:
        o.hide_render = False
    arm.animation_data.action = None
