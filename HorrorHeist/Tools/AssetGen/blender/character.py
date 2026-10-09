"""
The crew body: skeleton (UE-style bone names), a sculpted-by-metaballs body mesh, automatic
weights, and procedural animations (idle, walk, run, crouch idle/walk, fall).

Authored in centimetres (scene unit scale 0.01), facing -Y, feet on z = 0. In Unreal the asset
faces +Y, so the character definition rotates the mesh by -90 yaw.

Used by build_character.py; can be imported for the cosmetics (they reuse body + weights).
"""
import math

import bmesh
import bpy
from mathutils import Matrix, Quaternion, Vector

import hhblend as H

FORWARD = Vector((0, -1, 0))
FPS = 30

# name: (head, tail, parent)  - left side (+X); the right side is mirrored.
BONES = {
    'root': ((0, 0, 0), (0, 0, 12), None),
    'pelvis': ((0, 1, 95), (0, 1, 106), 'root'),
    'spine_01': ((0, 1, 106), (0, 0.5, 118), 'pelvis'),
    'spine_02': ((0, 0.5, 118), (0, 0, 131), 'spine_01'),
    'spine_03': ((0, 0, 131), (0, 0.5, 146), 'spine_02'),
    'neck_01': ((0, 0.5, 146), (0, -0.5, 155), 'spine_03'),
    'head': ((0, -0.5, 155), (0, -0.5, 176), 'neck_01'),
    'clavicle_l': ((2.5, -1.0, 143.5), (16.0, 1.5, 144.5), 'spine_03'),
    'upperarm_l': ((16.5, 1.5, 144.0), (36.3, 1.5, 124.2), 'clavicle_l'),
    'lowerarm_l': ((36.3, 1.5, 124.2), (53.4, -1.4, 106.3), 'upperarm_l'),
    'hand_l': ((53.4, -1.4, 106.3), (64.9, -3.4, 94.3), 'lowerarm_l'),
    'thigh_l': ((9.5, 1, 92), (9.5, -0.5, 51), 'pelvis'),
    'calf_l': ((9.5, -0.5, 51), (9.5, 2, 9), 'thigh_l'),
    'foot_l': ((9.5, 2, 9), (9.5, -11, 2), 'calf_l'),
    'ball_l': ((9.5, -11, 2), (9.5, -18, 2), 'foot_l'),
}


def all_bones():
    out = {}
    for name, (h, t, parent) in BONES.items():
        out[name] = (Vector(h), Vector(t), parent)
        if name.endswith('_l'):
            mirror = lambda v: Vector((-v[0], v[1], v[2]))  # noqa: E731
            pname = parent[:-2] + '_r' if parent and parent.endswith('_l') else parent
            out[name[:-2] + '_r'] = (mirror(h), mirror(t), pname)
    return out


# =======================================================================================
# Skeleton

def build_armature():
    arm = bpy.data.armatures.new('Armature')
    obj = bpy.data.objects.new('Armature', arm)
    H.link(obj)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode='EDIT')
    bones = all_bones()
    for name, (h, t, parent) in bones.items():
        eb = arm.edit_bones.new(name)
        eb.head = h
        eb.tail = t
    for name, (h, t, parent) in bones.items():
        eb = arm.edit_bones[name]
        if parent:
            eb.parent = arm.edit_bones[parent]
            eb.use_connect = (eb.parent.tail - eb.head).length < 0.01
        # Deterministic roll: local Z points forward (feet: up).
        eb.align_roll(Vector((0, 0, 1)) if name.startswith(('foot', 'ball')) else FORWARD)
    bpy.ops.object.mode_set(mode='OBJECT')
    obj.data.display_type = 'STICK'
    return obj


# =======================================================================================
# Body

R = 1.0 / 0.574     # metaball radius per cm of surface radius (threshold 0.6)


def _ball(mb, co, r, kind='BALL', size=(1, 1, 1), rot=None, stiff=2.0):
    e = mb.elements.new(type=kind)
    e.co = co
    e.radius = r * R
    e.stiffness = stiff
    if kind in ('ELLIPSOID', 'CAPSULE'):
        e.size_x, e.size_y, e.size_z = size
    if rot is not None:
        e.rotation = rot
    return e


def _capsule(mb, a, b, r, stiff=2.0):
    a, b = Vector(a), Vector(b)
    d = b - a
    q = Vector((1, 0, 0)).rotation_difference(d.normalized())
    _ball(mb, (a + b) / 2, r, 'CAPSULE', (d.length / 2, 1, 1), q, stiff)


def _limb(mb, a, b, r0, r1, steps=4, stiff=2.0):
    a, b = Vector(a), Vector(b)
    for i in range(steps):
        t0, t1 = i / steps, (i + 1) / steps
        _capsule(mb, a.lerp(b, t0), a.lerp(b, t1), r0 + (r1 - r0) * (t0 + t1) / 2, stiff)


def body_field(h=0.7):
    """Signed-distance description of the body (cm)."""
    import numpy as np
    import sdf
    F = sdf.Field((-72, -22, -1), (72, 20, 182), h)
    # Torso.
    F.ellipsoid((0, 1.5, 93), (15.0, 10.0, 11.5))
    F.ellipsoid((0, 0.8, 107), (13.2, 9.2, 11.0), k=9)
    F.ellipsoid((0, 0.6, 123), (15.2, 10.2, 14.0), k=9)
    F.ellipsoid((0, 0.4, 135.5), (17.0, 9.8, 10.0), k=8)
    for s in (-1, 1):
        F.ellipsoid((s * 8.0, -3.8, 133.5), (7.0, 5.0, 5.5), k=3)               # pecs
        F.ellipsoid((s * 6.6, 5.5, 89.0), (7.6, 6.2, 8.0), k=4)                 # glutes
        F.ellipsoid((s * 7.5, 5.0, 130), (7.5, 4.5, 12), k=4)                    # lats / back
        F.sphere((s * 16.8, 1.2, 141.6), 6.1, k=3)                                # deltoid caps
        F.cone((s * 3.5, 2.6, 146.5), (s * 15.0, 1.8, 143.0), 4.8, 3.6, k=4)    # trapezius
    # Neck & head.
    F.cone((0, 1.6, 140), (0, 0.6, 158), 6.0, 5.1, k=4)
    F.ellipsoid((0, 0.4, 168.6), (8.5, 9.9, 10.4), k=3)                          # cranium
    F.ellipsoid((0, -3.4, 163.2), (6.5, 7.6, 8.4), k=3)                          # face mass
    F.ellipsoid((0, -5.6, 157.8), (5.0, 4.8, 3.4), k=2.5)                        # chin / jaw
    F.cone((0, -9.2, 167.8), (0, -10.7, 163.9), 0.9, 1.45, k=1.2)                # nose
    F.sphere((0, -10.5, 163.4), 1.45, k=1.0)
    F.ellipsoid((0, -8.6, 160.6), (2.6, 1.2, 0.9), k=1.0)                        # lips
    for s in (-1, 1):
        F.ellipsoid((s * 7.9, 0.4, 165.4), (1.3, 2.4, 3.2), k=1.2)              # ears
        F.ellipsoid((s * 3.5, -8.4, 169.0), (2.4, 1.0, 0.6), k=1.6)             # brow
        F.sphere((s * 3.4, -9.9, 166.0), 1.45, k=1.0, op='subtract')             # eye sockets
    # Arms.
    for s in (-1, 1):
        sh = np.array((s * 16.5, 1.5, 144.0))
        el = np.array((s * 36.3, 1.5, 124.2))
        wr = np.array((s * 53.4, -1.4, 106.3))
        F.cone(sh, el, 5.2, 3.9, k=3)
        F.ellipsoid(sh + (el - sh) * 0.42 + (0, -1.6, 0), (7.5, 4.0, 3.9), k=2.5,
                    axes=sdf.orient(el - sh))                                    # biceps
        F.ellipsoid(sh + (el - sh) * 0.45 + (0, 1.8, 0), (7.5, 3.6, 3.6), k=2.5,
                    axes=sdf.orient(el - sh))                                    # triceps
        F.cone(el, wr, 4.0, 2.8, k=2)
        F.ellipsoid(el + (wr - el) * 0.3, (7.0, 4.0, 3.6), k=2.5, axes=sdf.orient(wr - el))   # forearm
        d = (wr - el) / np.linalg.norm(wr - el)
        ax = sdf.orient(d, up_hint=(0, -1, 0))
        F.ellipsoid(wr + d * 5.2, (5.0, 4.1, 1.75), k=1.8, axes=ax)              # palm
        F.ellipsoid(wr + d * 11.5, (4.4, 3.7, 1.35), k=1.5, axes=ax)             # fingers
        F.cone(wr + d * 2.0 + ax[1] * 2.3, wr + d * 7.5 + ax[1] * 4.0, 1.35, 1.05, k=1.0)   # thumb
    # Legs.
    for s in (-1, 1):
        hip = np.array((s * 9.5, 1.0, 92.0))
        kn = np.array((s * 9.5, -0.5, 51.0))
        an = np.array((s * 9.5, 2.0, 9.0))
        F.cone(hip, kn, 8.4, 5.3, k=5)
        F.ellipsoid(hip + (kn - hip) * 0.38 + (s * 0.6, -1.8, 0), (13.0, 6.6, 6.4), k=3,
                    axes=sdf.orient(kn - hip))                                   # quads
        F.sphere(kn + (0, -1.4, 0.5), 4.5, k=2)                                  # knee
        F.cone(kn, an, 5.0, 3.2, k=2.5)
        F.ellipsoid(kn + (an - kn) * 0.27 + (0, 2.8, 0), (10.0, 4.9, 4.6), k=3,
                    axes=sdf.orient(an - kn))                                    # calf
        F.sphere(an + (0, 0, -1.0), 3.5, k=2)                                    # ankle
        F.ellipsoid((s * 9.5, -4.8, 3.6), (4.1, 11.2, 3.5), k=2.5)              # foot
        F.sphere((s * 9.5, 4.4, 3.8), 3.4, k=2)                                  # heel
    F.plane_cut((0, 0, 0.0), (0, 0, 1), k=0.6)                                   # flat soles
    return F


def build_body_mesh():
    import sdf
    F = body_field()
    verts, quads = F.mesh()
    me = sdf.to_blender_mesh('SK_HH_Body', verts, quads)
    body = bpy.data.objects.new('SK_HH_Body', me)
    H.link(body)
    bpy.context.view_layer.objects.active = body
    H.deselect_all()
    body.select_set(True)
    dec = body.modifiers.new('Decimate', 'DECIMATE')
    dec.ratio = 0.16
    sm = body.modifiers.new('Smooth', 'LAPLACIANSMOOTH')
    sm.lambda_factor = 0.35
    sm.iterations = 3
    sm.use_volume_preserve = True
    for mod in list(body.modifiers):
        bpy.ops.object.modifier_apply(modifier=mod.name)
    me = body.data
    me.shade_smooth()
    _box_uv_cm(me)
    me.materials.append(H.material('MI_Char_Skin'))
    _add_eyes(body)
    return body


def _box_uv_cm(me, scale=0.01):
    bm = bmesh.new()
    bm.from_mesh(me)
    uv = bm.loops.layers.uv.verify()
    for f in bm.faces:
        n = f.normal
        ax = max(range(3), key=lambda i: abs(n[i]))
        for loop in f.loops:
            co = loop.vert.co * scale
            if ax == 2:
                loop[uv].uv = (co.x, co.y)
            elif ax == 0:
                loop[uv].uv = (co.y, co.z)
            else:
                loop[uv].uv = (co.x, co.z)
    bm.to_mesh(me)
    bm.free()


def _add_eyes(body):
    """Small dark glossy eyes (second material slot, weighted to the head later)."""
    me = body.data
    bm = bmesh.new()
    bm.from_mesh(me)
    uv = bm.loops.layers.uv.verify()
    me.materials.append(H.material('MI_Char_EyeWhite'))
    me.materials.append(H.material('MI_Char_Eye'))
    for s in (-1, 1):
        centre = Vector((s * 3.4, -9.0, 166.0))
        ret = bmesh.ops.create_uvsphere(bm, u_segments=16, v_segments=10, radius=1.2,
                                        matrix=Matrix.Translation(centre) @ Matrix.Diagonal((1, 0.85, 0.95, 1)))
        for f in {f for v in ret['verts'] for f in v.link_faces}:
            # Iris/pupil where the eye faces forward, sclera elsewhere.
            look = (f.calc_center_median() - centre).normalized()
            f.material_index = 2 if look.dot(Vector((0.0, -1.0, 0.03)).normalized()) > 0.8 else 1
            f.smooth = True
            for loop in f.loops:
                loop[uv].uv = (0.5, 0.5)
    bm.to_mesh(me)
    bm.free()


def skin(body, arm):
    H.deselect_all()
    body.select_set(True)
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.parent_set(type='ARMATURE_AUTO')
    # Eyes follow the head 100 %.
    me = body.data
    head = body.vertex_groups['head']
    eye_verts = {v for p in me.polygons if p.material_index >= 1 for v in p.vertices}
    for vg in body.vertex_groups:
        vg.remove(list(eye_verts))
    head.add(list(eye_verts), 1.0, 'REPLACE')
    # Keep the root bone weight-free.
    if 'root' in body.vertex_groups:
        body.vertex_groups.remove(body.vertex_groups['root'])
    limit_and_normalise(body)


def limit_and_normalise(obj, limit=4):
    me = obj.data
    names = {vg.index: vg.name for vg in obj.vertex_groups}
    for v in me.vertices:
        ws = sorted(((g.weight, g.group) for g in v.groups if g.weight > 0.001), reverse=True)
        keep = ws[:limit]
        total = sum(w for w, _ in keep) or 1.0
        for g in list(v.groups):
            obj.vertex_groups[names[g.group]].remove([v.index])
        for w, gi in keep:
            obj.vertex_groups[names[gi]].add([v.index], w / total, 'REPLACE')


# =======================================================================================
# Posing: absolute (armature-space) rotations per bone -> keyed local rotations

class Poser:
    def __init__(self, arm):
        self.arm = arm
        self.rest = {b.name: b.matrix_local.copy() for b in arm.data.bones}
        self.parent = {b.name: (b.parent.name if b.parent else None) for b in arm.data.bones}
        self.order = [b.name for b in arm.data.bones]   # parents come first
        self.length = {b.name: b.length for b in arm.data.bones}

    def rest_dir(self, name):
        return (self.rest[name].to_3x3() @ Vector((0, 1, 0))).normalized()

    def rest_head(self, name):
        return self.rest[name].to_translation()

    def solve(self, abs_rot, pelvis_offset=Vector()):
        """abs_rot: {bone: Quaternion in armature space}. Returns {bone: posed 4x4}."""
        posed = {}
        for name in self.order:
            rest = self.rest[name]
            parent = self.parent[name]
            if parent is None:
                head = rest.to_translation()
            else:
                head = (posed[parent] @ self.rest[parent].inverted() @ rest).to_translation()
            if name == 'pelvis':
                head = head + pelvis_offset
            rot = abs_rot.get(name)
            if rot is None:
                rot = posed[parent].to_quaternion() @ self.rest[parent].to_quaternion().inverted() if parent else Quaternion()
            m = rot.to_matrix() @ rest.to_3x3()
            pm = m.to_4x4()
            pm.translation = head
            posed[name] = pm
        return posed

    def key(self, frame, posed):
        pose = self.arm.pose
        for name in self.order:
            rest = self.rest[name]
            parent = self.parent[name]
            if parent is None:
                base = rest
            else:
                base = posed[parent] @ self.rest[parent].inverted() @ rest
            basis = base.inverted() @ posed[name]
            pb = pose.bones[name]
            pb.rotation_mode = 'QUATERNION'
            pb.rotation_quaternion = basis.to_quaternion()
            pb.location = basis.to_translation()
            pb.keyframe_insert('rotation_quaternion', frame=frame)
            pb.keyframe_insert('location', frame=frame)


def qaxis(axis, deg):
    return Quaternion(Vector(axis).normalized(), math.radians(deg))


X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))


def two_bone_ik(hip, target, l1, l2, pole):
    d = target - hip
    dist = min(d.length, (l1 + l2) * 0.999)
    u = d.normalized()
    cos_a = (l1 * l1 + dist * dist - l2 * l2) / (2 * l1 * dist)
    a = math.acos(max(-1.0, min(1.0, cos_a)))
    p = (pole - u * u.dot(pole)).normalized()
    knee = hip + u * (l1 * math.cos(a)) + p * (l1 * math.sin(a))
    ankle = hip + u * dist
    return knee, ankle


class Gait:
    """Parameters for one locomotion cycle."""

    def __init__(self, frames, speed, duty, lift, pelvis_z, bob, lean, arm_swing, elbow, elbow_swing, stride_bias=0.0,
                 heel_raise=30.0, sway=1.2, twist=4.0, foot_width=9.0, toe_up=8.0):
        self.frames = frames
        self.speed = speed
        self.duty = duty
        self.lift = lift
        self.pelvis_z = pelvis_z
        self.bob = bob
        self.lean = lean
        self.arm_swing = arm_swing
        self.elbow = elbow
        self.elbow_swing = elbow_swing
        self.stride_bias = stride_bias
        self.heel_raise = heel_raise
        self.sway = sway
        self.twist = twist
        self.foot_width = foot_width
        self.toe_up = toe_up

    @property
    def period(self):
        return self.frames / FPS


def smooth(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


class Animator:
    def __init__(self, arm):
        self.arm = arm
        self.P = Poser(arm)
        P = self.P
        self.l_thigh = (P.rest_head('calf_l') - P.rest_head('thigh_l')).length
        self.l_calf = (P.rest_head('foot_l') - P.rest_head('calf_l')).length
        self.ankle_from_ball = P.rest_head('foot_l') - P.rest_head('ball_l')

    # -- helpers -------------------------------------------------------------------
    def arm_rots(self, side, spine_rot, hang=37.0, swing=0.0, elbow=10.0, raise_side=0.0, forward_reach=0.0):
        """Absolute rotations for clavicle/upperarm/lowerarm/hand of one side ('l'/'r')."""
        P = self.P
        s = 1 if side == 'l' else -1
        up_dir = P.rest_dir('upperarm_' + side)
        # Lower the A-pose arm towards the body (rotation about the forward axis).
        q_hang = qaxis(Y, s * (hang - raise_side))
        q_swing = qaxis(X, -swing)                       # +swing = forward
        q_reach = qaxis(X, -forward_reach)
        q_upper = spine_rot @ q_reach @ q_swing @ q_hang
        bend_axis = up_dir.cross(FORWARD).normalized()
        q_lower = q_upper @ Quaternion(bend_axis, math.radians(elbow))
        return {
            'clavicle_' + side: spine_rot,
            'upperarm_' + side: q_upper,
            'lowerarm_' + side: q_lower,
            'hand_' + side: q_lower @ qaxis(bend_axis, 8.0),
        }

    def leg_rots(self, side, hip, ankle_target, foot_pitch, ball_pitch, pole=FORWARD):
        P = self.P
        knee, ankle = two_bone_ik(hip, ankle_target, self.l_thigh, self.l_calf, pole + Vector((0, 0, 0)))
        q_thigh = P.rest_dir('thigh_' + side).rotation_difference((knee - hip).normalized())
        q_calf = P.rest_dir('calf_' + side).rotation_difference((ankle - knee).normalized())
        toe_out = qaxis(Z, (6.0 if side == 'l' else -6.0))
        q_foot = toe_out @ qaxis(X, foot_pitch)
        q_ball = toe_out @ qaxis(X, ball_pitch)
        return {'thigh_' + side: q_thigh, 'calf_' + side: q_calf, 'foot_' + side: q_foot, 'ball_' + side: q_ball}

    def hip_position(self, side, pelvis_rot, pelvis_offset):
        P = self.P
        rest = P.rest
        pelvis = rest['pelvis'].copy()
        pm = pelvis_rot.to_matrix().to_4x4() @ rest['pelvis'].to_3x3().to_4x4()
        pm.translation = pelvis.to_translation() + pelvis_offset
        return (pm @ rest['pelvis'].inverted() @ rest['thigh_' + side]).to_translation()

    def spine_chain(self, pelvis_rot, lean, twist=0.0, breathe=0.0, head_pitch=0.0, head_yaw=0.0):
        # +X rotation leans forward (the character faces -Y); head_pitch > 0 looks up.
        q_sp1 = pelvis_rot @ qaxis(X, lean * 0.3) @ qaxis(Z, twist * 0.3)
        q_sp2 = q_sp1 @ qaxis(X, lean * 0.3 + breathe) @ qaxis(Z, twist * 0.35)
        q_sp3 = q_sp2 @ qaxis(X, lean * 0.4 - breathe * 1.5) @ qaxis(Z, twist * 0.35)
        # Keep the gaze level: undo most of the lean & twist in neck/head.
        q_neck = q_sp3 @ qaxis(X, -lean * 0.45 - head_pitch * 0.4) @ qaxis(Z, -twist * 0.5 + head_yaw * 0.4)
        q_head = q_neck @ qaxis(X, -lean * 0.45 - head_pitch * 0.6) @ qaxis(Z, -twist * 0.5 + head_yaw * 0.6)
        return {'pelvis': pelvis_rot, 'spine_01': q_sp1, 'spine_02': q_sp2, 'spine_03': q_sp3, 'neck_01': q_neck,
                'head': q_head}

    # -- foot path -------------------------------------------------------------------
    def foot(self, g, phase):
        """Ankle target (y, z) plus absolute foot and toe pitch at cycle phase (0 = heel strike).
        Stance: the ball slides back at the walk speed; late stance the heel peels up around the ball,
        early stance the toes come down around the heel. Swing: an arc forward."""
        stance = g.speed * g.period * g.duty
        centre = -12.0 + g.stride_bias
        front, back = centre - stance / 2, centre + stance / 2
        a = self.ankle_from_ball
        ground = self.P.rest_head('ball_l').z

        def place(ball, pitch):
            if pitch >= 0:
                return ball + qaxis(X, pitch) @ a                    # pivot on the ball (heel up)
            flat = ball + a
            heel = flat + Vector((0, 4.5, -7.0))
            return heel + qaxis(X, pitch) @ (flat - heel)            # pivot on the heel (toes up)

        if phase < g.duty:
            t = phase / g.duty
            ball = Vector((0, front + (back - front) * t, ground))
            heel_up = g.heel_raise * smooth((t - 0.62) / 0.38)
            toe_up = g.toe_up * (1 - smooth(t / 0.12))
            pitch = heel_up if heel_up > 0.01 else -toe_up
            ankle = place(ball, pitch)
            return ankle.y, ankle.z, pitch, (0.0 if pitch >= 0 else pitch)
        t = (phase - g.duty) / (1 - g.duty)
        y = back + (front - back) * smooth(t)
        lift = g.lift * math.sin(math.pi * t)
        pitch = g.heel_raise * (1 - smooth(t / 0.45)) - g.toe_up * smooth((t - 0.6) / 0.4)
        ankle = place(Vector((0, y, ground + lift)), pitch)
        return ankle.y, ankle.z, pitch, pitch * 0.6

    # -- cycles ----------------------------------------------------------------------
    def locomotion(self, g, crouch=False):
        frames = []
        for f in range(g.frames + 1):
            p = (f % g.frames) / g.frames
            bob = -math.cos(4 * math.pi * p)            # lowest at each heel strike
            pelvis_off = Vector((g.sway * math.sin(2 * math.pi * p), 0,
                                 g.pelvis_z - self.P.rest_head('pelvis').z + g.bob * bob))
            yaw = -g.twist * math.cos(2 * math.pi * p)   # left hip forward at the left heel strike
            pelvis_rot = qaxis(Z, yaw) @ qaxis(X, g.lean * 0.5) @ qaxis(Y, g.sway * 1.2 * math.sin(2 * math.pi * p))
            rots = self.spine_chain(pelvis_rot, g.lean * 0.5, twist=-2 * yaw, breathe=0.0,
                                    head_pitch=(14.0 if crouch else 0.0))
            for side, offset in (('l', 0.0), ('r', 0.5)):
                ph = (p + offset) % 1.0
                fy, fz, pitch, ball_pitch = self.foot(g, ph)
                s = 1 if side == 'l' else -1
                target = Vector((s * g.foot_width, fy, fz))
                hip = self.hip_position(side, pelvis_rot, pelvis_off)
                rots.update(self.leg_rots(side, hip, target, pitch, ball_pitch))
                # Arms swing opposite to the leg on the same side.
                arm_phase = -math.cos(2 * math.pi * ph)   # back while this leg is forward
                swing = g.arm_swing * arm_phase
                elbow = g.elbow + g.elbow_swing * max(0.0, arm_phase)
                rots.update(self.arm_rots(side, rots['spine_03'], hang=36.0, swing=swing, elbow=elbow,
                                          raise_side=3.0 if not crouch else 8.0,
                                          forward_reach=8.0 if crouch else 0.0))
            frames.append((f, rots, pelvis_off))
        return frames

    def idle(self, frames=120, crouch=False):
        out = []
        P = self.P
        for f in range(frames + 1):
            t = (f % frames) / frames
            breathe = 1.2 * math.sin(2 * math.pi * t * (2 if not crouch else 3))
            sway = math.sin(2 * math.pi * t)
            if crouch:
                pelvis_off = Vector((0.8 * sway, 4.0, 62 - P.rest_head('pelvis').z + 0.6 * math.sin(4 * math.pi * t)))
                pelvis_rot = qaxis(X, 24) @ qaxis(Z, 3 * sway)
                rots = self.spine_chain(pelvis_rot, 14, twist=-2 * sway, breathe=breathe, head_pitch=26,
                                        head_yaw=6 * math.sin(2 * math.pi * t + 1))
                feet = {'l': Vector((13, -14, 9)), 'r': Vector((-12, 12, 13))}
                pitches = {'l': (0.0, 0.0), 'r': (26.0, -26.0)}
            else:
                pelvis_off = Vector((1.2 * sway, 0, -1.0 + 0.4 * math.sin(4 * math.pi * t)))
                pelvis_rot = qaxis(Y, -1.5 * sway) @ qaxis(Z, 2 * sway)
                rots = self.spine_chain(pelvis_rot, 2, twist=-1.5 * sway, breathe=breathe, head_pitch=-2,
                                        head_yaw=7 * math.sin(2 * math.pi * t + 0.7))
                feet = {'l': Vector((11, -3, 9)), 'r': Vector((-10.5, 2, 9))}
                pitches = {'l': (0.0, 0.0), 'r': (0.0, 0.0)}
            for side in ('l', 'r'):
                hip = self.hip_position(side, pelvis_rot, pelvis_off)
                fp, bp = pitches[side]
                rots.update(self.leg_rots(side, hip, feet[side], fp, bp))
                s = 1 if side == 'l' else -1
                if crouch:
                    rots.update(self.arm_rots(side, rots['spine_03'], hang=40, swing=28 + 2 * breathe, elbow=55,
                                              raise_side=6))
                else:
                    rots.update(self.arm_rots(side, rots['spine_03'], hang=36 - 0.6 * breathe, swing=2 + s * 1.0,
                                              elbow=9 + 1.5 * sway * s))
            out.append((f, rots, pelvis_off))
        return out

    def fall(self, frames=30):
        out = []
        P = self.P
        for f in range(frames + 1):
            t = (f % frames) / frames
            w = math.sin(2 * math.pi * t)
            pelvis_off = Vector((0, 0, -2.0))
            pelvis_rot = qaxis(X, 4)
            rots = self.spine_chain(pelvis_rot, -6 + 2 * w, twist=3 * w, head_pitch=6)
            for side, k in (('l', 1.0), ('r', -1.0)):
                hip = self.hip_position(side, pelvis_rot, pelvis_off)
                s = 1 if side == 'l' else -1
                target = Vector((s * 13, -14 * k + 6 + 4 * w * k, 22 + 8 * k + 3 * w))
                rots.update(self.leg_rots(side, hip, target, -15 * k, 0.0))
                rots.update(self.arm_rots(side, rots['spine_03'], hang=10 + 6 * w * s, swing=18 + 10 * w * s, elbow=35))
            out.append((f, rots, pelvis_off))
        return out

    # -- writing actions -------------------------------------------------------------
    def bake(self, name, frames):
        arm = self.arm
        if arm.animation_data is None:
            arm.animation_data_create()
        action = bpy.data.actions.new(name)
        arm.animation_data.action = action
        for frame, rots, pelvis_off in frames:
            posed = self.P.solve(rots, pelvis_off)
            self.P.key(frame + 1, posed)
        # Smooth interpolation for loops.
        for fc in _fcurves(action):
            for kp in fc.keyframe_points:
                kp.interpolation = 'LINEAR'
        action.use_fake_user = True
        return action


def _fcurves(action):
    """Blender 4.4+ layered actions keep F-curves in channelbags."""
    if hasattr(action, 'fcurves') and len(getattr(action, 'fcurves', [])):
        return list(action.fcurves)
    out = []
    for layer in getattr(action, 'layers', []):
        for strip in layer.strips:
            for bag in getattr(strip, 'channelbags', []):
                out.extend(bag.fcurves)
    return out


GAITS = {
    'Walk': Gait(frames=24, speed=150.0, duty=0.58, lift=10.0, pelvis_z=89.0, bob=1.3, lean=4.0, arm_swing=17.0,
                 elbow=12.0, elbow_swing=14.0, heel_raise=28.0, sway=1.4, twist=4.5, foot_width=9.5),
    'Run': Gait(frames=19, speed=400.0, duty=0.34, lift=24.0, pelvis_z=86.0, bob=2.6, lean=14.0, arm_swing=34.0,
                elbow=78.0, elbow_swing=18.0, stride_bias=8.0, heel_raise=34.0, sway=1.0, twist=7.0,
                foot_width=8.5, toe_up=4.0),
    'CrouchWalk': Gait(frames=36, speed=120.0, duty=0.62, lift=9.0, pelvis_z=63.0, bob=1.0, lean=30.0,
                       arm_swing=10.0, elbow=50.0, elbow_swing=8.0, heel_raise=22.0, sway=1.6, twist=3.5,
                       foot_width=12.0),
}


def build_animations(arm):
    anim = Animator(arm)
    actions = {}
    actions['Idle'] = anim.bake('A_HH_Idle', anim.idle(120))
    actions['CrouchIdle'] = anim.bake('A_HH_CrouchIdle', anim.idle(90, crouch=True))
    for name, g in GAITS.items():
        actions[name] = anim.bake('A_HH_' + name, anim.locomotion(g, crouch=name.startswith('Crouch')))
    actions['Fall'] = anim.bake('A_HH_Fall', anim.fall(30))
    return actions


def action_frames(action):
    fr = action.frame_range
    return int(fr[0]), int(fr[1])
