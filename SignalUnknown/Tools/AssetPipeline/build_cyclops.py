"""Cyclops creature from "Lesser Cyclops Variant Rig" by DM-913 (CC-BY-4.0).

The source is a Blender IK rig baked to glTF: 100 joints, of which 62 deform the mesh; hands and feet hang
under IK targets and the only animation is a 15 s "Lurk". This script

  * keeps the original mesh and skin weights,
  * rebuilds a clean forward-kinematics skeleton that contains only the deforming joints
    (world aligned bind frames, so poses can be written with plain axis rotations),
  * converts "Lurk" to that skeleton (it becomes the idle clip),
  * authors new clips (Walk, Run, Attack, Scream) with two-bone IK for arms and legs,
  * scales the creature to a believable size and turns it so that it faces +Z.
"""
import io
import os
import re
import numpy as np
from PIL import Image
from gltfio import Gltf, GlbWriter
import textures as tex
from rig import (Skeleton, Clip, write_skinned, q_axis, q_mul, q_conj, q_norm, q_rotate, q_from_matrix, q_between, q_slerp,
                 IDENT, smooth, pulse)

CREDIT = '"Lesser Cyclops Variant Rig" by DM-913 (https://sketchfab.com/SuperKapoo913), CC-BY-4.0, modified'
SCALE = 0.052          # source units -> meters: 4.05 m when standing upright, about 1.7 m in its crouch
FPS = 30

FINGERS = ['IndexFinger', 'MFinger', 'RingFinger', 'Pinky']


def _hierarchy():
    """(source joint name, parent source joint name) in parent-before-child order."""
    h = [('Pelvis', None), ('Spine1', 'Pelvis'), ('Spine2', 'Spine1'), ('Spine3', 'Spine2'), ('Spine4', 'Spine3'), ('Chest', 'Spine4'),
         ('Neck1', 'Chest'), ('Neck2', 'Neck1'), ('Neck3', 'Neck2'), ('Neck4', 'Neck3'), ('Head', 'Neck4'), ('jaw', 'Head')]
    for s in ('L', 'R'):
        h += [('Pelvis.' + s, 'Pelvis'), ('Thigh.' + s, 'Pelvis.' + s), ('Knee.' + s, 'Thigh.' + s), ('Shin.' + s, 'Knee.' + s),
              ('Foot.' + s, 'Shin.' + s), ('Toes.' + s, 'Foot.' + s),
              ('Shoulder.' + s, 'Chest'), ('UpperArm.' + s, 'Shoulder.' + s), ('Elbow.' + s, 'UpperArm.' + s), ('ForeArm.' + s, 'Elbow.' + s),
              ('Hand.' + s, 'ForeArm.' + s)]
        for f in FINGERS:
            h += [('%sBase.%s' % (f, s), 'Hand.' + s), ('%s1.%s' % (f, s), '%sBase.%s' % (f, s)), ('%s2.%s' % (f, s), '%s1.%s' % (f, s)),
                  ('%s3.%s' % (f, s), '%s2.%s' % (f, s))]
        h += [('Thumb1.' + s, 'Hand.' + s), ('Thumb2.' + s, 'Thumb1.' + s), ('Thumb3.' + s, 'Thumb2.' + s)]
    return h


def clean(name):
    return 'Jaw' if name == 'jaw' else name.replace('.', '_')


def mirror_q(q):
    """Mirror a rotation across the plane X = 0."""
    return np.array([q[0], -q[1], -q[2], q[3]])


def mirror_name(name):
    if name.endswith('_L'):
        return name[:-2] + '_R'
    if name.endswith('_R'):
        return name[:-2] + '_L'
    return name


# ---------------------------------------------------------------------------------------------- source
class Source:
    def __init__(self, path):
        g = Gltf(path)
        self.g = g
        js = g.js
        skin = js['skins'][0]
        self.joint_nodes = skin['joints']
        self.ibm = g.accessor(skin['inverseBindMatrices']).reshape(-1, 4, 4).transpose(0, 2, 1).astype(np.float64)
        self.name_of = {ni: re.sub(r'_\d+$', '', js['nodes'][ni]['name']) for ni in self.joint_nodes}
        self.joint_index = {self.name_of[ni]: k for k, ni in enumerate(self.joint_nodes)}
        nodes = js['nodes']
        self.parent = [-1] * len(nodes)
        for i, n in enumerate(nodes):
            for c in n.get('children', []):
                self.parent[c] = i
        self.order = []
        seen = set()

        def visit(i):
            if i in seen:
                return
            if self.parent[i] >= 0:
                visit(self.parent[i])
            seen.add(i)
            self.order.append(i)
        for i in range(len(nodes)):
            visit(i)
        self.rest = [(np.array(n.get('translation', [0, 0, 0]), dtype=np.float64), np.array(n.get('rotation', [0, 0, 0, 1]), dtype=np.float64),
                      np.array(n.get('scale', [1, 1, 1]), dtype=np.float64)) for n in nodes]
        self.fixed = {i: g.node_local(i) for i, n in enumerate(nodes) if 'matrix' in n}
        anim = js['animations'][0]
        self.tracks = {}
        self.duration = 0.0
        for c in anim['channels']:
            s = anim['samplers'][c['sampler']]
            times = g.accessor(s['input']).reshape(-1).astype(np.float64)
            vals = g.accessor(s['output']).astype(np.float64)
            self.tracks[(c['target']['node'], c['target']['path'])] = (times, vals)
            self.duration = max(self.duration, float(times[-1]))
        self.t0 = min(float(t[0]) for t, _ in self.tracks.values())

    def _sample(self, node, path, t, default):
        tr = self.tracks.get((node, path))
        if tr is None:
            return default
        times, vals = tr
        if t <= times[0]:
            return vals[0]
        if t >= times[-1]:
            return vals[-1]
        k = int(np.searchsorted(times, t)) - 1
        u = (t - times[k]) / max(times[k + 1] - times[k], 1e-9)
        if path == 'rotation':
            return q_slerp(vals[k], vals[k + 1], u)
        return vals[k] * (1 - u) + vals[k + 1] * u

    def world_at(self, t):
        """World matrix of every node at animation time t."""
        nodes = self.g.js['nodes']
        world = [None] * len(nodes)
        for i in self.order:
            if i in self.fixed:
                m = self.fixed[i]
            else:
                rt, rq, rs = self.rest[i]
                tt = self._sample(i, 'translation', t, rt)
                qq = q_norm(self._sample(i, 'rotation', t, rq))
                ss = self._sample(i, 'scale', t, rs)
                m = np.eye(4)
                m[:3, :3] = _qm(qq) * ss[None, :]
                m[:3, 3] = tt
            world[i] = m if self.parent[i] < 0 else world[self.parent[i]] @ m
        return world


def _qm(q):
    x, y, z, w = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def _rigid(m):
    """Closest rotation of a 3x3 matrix (drops the rig's squash and stretch)."""
    u, _, vt = np.linalg.svd(m)
    r = u @ vt
    if np.linalg.det(r) < 0:
        u[:, -1] *= -1
        r = u @ vt
    return r


# ---------------------------------------------------------------------------------------------- pose tools
class Pose:
    """Local rotations / translations of the clean skeleton plus helpers to edit them."""

    def __init__(self, sk, lq, lt):
        self.sk = sk
        self.lq = {k: np.array(v) for k, v in lq.items()}
        self.lt = {k: np.array(v) for k, v in lt.items()}
        self.wq, self.wp = {}, {}

    def copy(self):
        return Pose(self.sk, self.lq, self.lt)

    def fk(self):
        sk = self.sk
        for i, name in enumerate(sk.names):
            p = sk.parent[i]
            if p < 0:
                self.wq[name] = self.lq[name]
                self.wp[name] = self.lt[name]
            else:
                pn = sk.names[p]
                self.wq[name] = q_norm(q_mul(self.wq[pn], self.lq[name]))
                self.wp[name] = self.wp[pn] + q_rotate(self.wq[pn], self.lt[name])
        return self

    def bend(self, name, x=0.0, y=0.0, z=0.0):
        """Rotate a joint in its own frame: x = flex forward / jaw open, y = turn left, z = lean (left side up)."""
        d = q_mul(q_axis([0, 1, 0], y), q_mul(q_axis([1, 0, 0], x), q_axis([0, 0, 1], z)))
        self.lq[name] = q_norm(q_mul(self.lq[name], d))

    def set_world_rot(self, name, wq):
        """Give a joint a world rotation (parents must be final; call fk() first)."""
        p = self.sk.parent[self.sk.index[name]]
        pq = self.wq[self.sk.names[p]] if p >= 0 else IDENT
        self.lq[name] = q_norm(q_mul(q_conj(pq), wq))
        self.wq[name] = q_norm(wq)

    def subtree(self, name):
        out, i0 = [name], self.sk.index[name]
        for i in range(i0 + 1, len(self.sk.names)):
            p = self.sk.parent[i]
            if p >= 0 and self.sk.names[p] in out:
                out.append(self.sk.names[i])
        return out

    def refresh(self, name):
        """Recompute world transforms below (and including) a joint after its local values changed."""
        sk = self.sk
        for n in self.subtree(name):
            p = sk.parent[sk.index[n]]
            pn = sk.names[p]
            self.wq[n] = q_norm(q_mul(self.wq[pn], self.lq[n]))
            self.wp[n] = self.wp[pn] + q_rotate(self.wq[pn], self.lt[n])

    def limb(self, a, mid, b, end, target, pole, end_rot=None):
        """Two-bone IK. Chain a -> mid (short knee / elbow joint) -> b -> end. Moves `end` to `target`,
        bending towards `pole` (a direction in world space)."""
        target = np.asarray(target, dtype=np.float64)
        goal = target.copy()
        for _ in range(3):
            self.refresh(a)
            root = self.wp[a]
            centre = (self.wp[mid] + self.wp[b]) * 0.5
            l1 = np.linalg.norm(centre - root)
            l2 = np.linalg.norm(self.wp[end] - centre)
            d = goal - root
            dist = float(np.clip(np.linalg.norm(d), abs(l1 - l2) + 1e-4, (l1 + l2) * 0.999))
            dn = d / max(np.linalg.norm(d), 1e-9)
            pp = np.asarray(pole, dtype=np.float64) - dn * float(np.dot(pole, dn))
            pp /= max(np.linalg.norm(pp), 1e-9)
            ca = (l1 * l1 + dist * dist - l2 * l2) / (2 * l1 * dist)
            sa = np.sqrt(max(0.0, 1 - ca * ca))
            elbow = root + dn * (l1 * ca) + pp * (l1 * sa)
            tip = root + dn * dist
            qa = q_mul(q_between(centre - root, elbow - root), self.wq[a])
            self.set_world_rot(a, qa)
            self.refresh(a)
            centre2 = (self.wp[mid] + self.wp[b]) * 0.5
            qb = q_mul(q_between(self.wp[end] - centre2, tip - elbow), self.wq[b])
            self.set_world_rot(mid, q_slerp(self.wq[a], qb, 0.5))
            self.refresh(mid)
            self.set_world_rot(b, qb)
            self.refresh(b)
            goal = goal + (target - self.wp[end]) * 0.9
        if end_rot is not None:
            self.set_world_rot(end, end_rot)
            self.refresh(end)

    def out(self):
        return dict(self.lq), dict(self.lt)


# ---------------------------------------------------------------------------------------------- build
def build(src_path, out_dir):
    src = Source(src_path)
    g = src.g
    hier = _hierarchy()
    # final space: scale and half turn (the source faces -Z)
    A = np.diag([-SCALE, SCALE, -SCALE, 1.0])
    Ainv = np.linalg.inv(A)

    sk = Skeleton()
    sk.add('Root', None, [0, 0, 0])
    bind = {'Root': np.zeros(3)}
    for name, parent in hier:
        k = src.joint_index[name]
        head = np.linalg.inv(src.ibm[k])[:3, 3]
        p = (A @ np.append(head, 1.0))[:3]
        cn, cp = clean(name), clean(parent) if parent else 'Root'
        bind[cn] = p
        sk.add(cn, cp, p - bind[cp])
    sk.add('SU_Front', 'Root', [0.0, 0.0, 1.0])
    names = sk.names
    src_k = {clean(n): src.joint_index[n] for n, _ in hier}

    def locals_at(t):
        """Lurk at time t on the clean skeleton -> local rotations / translations."""
        world = src.world_at(t)
        wq, wp = {'Root': IDENT, 'SU_Front': IDENT}, {'Root': np.zeros(3), 'SU_Front': np.array([0.0, 0.0, 1.0])}
        for n in names:
            if n not in src_k:
                continue
            k = src_k[n]
            s = A @ (world[src.joint_nodes[k]] @ src.ibm[k]) @ Ainv      # skinning matrix in final space
            r = _rigid(s[:3, :3])
            wq[n] = q_from_matrix(r)
            wp[n] = r @ bind[n] + s[:3, 3] if np.allclose(r, s[:3, :3], atol=0.02) else s[:3, :3] @ bind[n] + s[:3, 3]
        lq, lt = {}, {}
        for i, n in enumerate(names):
            p = sk.parent[i]
            if p < 0:
                lq[n], lt[n] = wq[n], wp[n]
            else:
                pn = names[p]
                lq[n] = q_norm(q_mul(q_conj(wq[pn]), wq[n]))
                lt[n] = q_rotate(q_conj(wq[pn]), wp[n] - wp[pn])
        return lq, lt

    # ------------------------------------------------------------------ idle = the original Lurk
    t0 = src.t0
    lurk_len = src.duration - t0
    idle = Clip('Idle', lurk_len, FPS, loop=True)
    first, last = locals_at(t0), locals_at(src.duration)
    blend = 0.6                                      # seconds used to ease the end back into the first frame

    def idle_pose(t, _ph):
        lq, lt = locals_at(t0 + t)
        u = smooth((t - (lurk_len - blend)) / blend)
        if u > 0:
            lq = {n: q_slerp(lq[n], first[0][n], u) for n in lq}
            lt = {n: lt[n] * (1 - u) + first[1][n] * u for n in lt}
        return lq, lt
    idle.bake(sk, idle_pose)

    # ------------------------------------------------------------------ symmetric base pose for new clips
    ref_lq, ref_lt = first
    base_lq, base_lt = {}, {}
    for n in names:
        if n.endswith('_L'):
            r = mirror_name(n)
            base_lq[n] = mirror_q(ref_lq[r])
            base_lt[n] = ref_lt[r] * np.array([-1.0, 1.0, 1.0])
        elif n.endswith('_R'):
            base_lq[n], base_lt[n] = ref_lq[n], ref_lt[n]
        else:
            q = ref_lq[n]
            m = mirror_q(q)
            base_lq[n] = q_norm(q + (m if np.dot(q, m) >= 0 else -m))
            base_lt[n] = ref_lt[n] * np.array([0.0, 1.0, 1.0])
    # bones keep their bind length in the authored clips (the Lurk rig stretches a little)
    for n in names:
        if n not in ('Root', 'Pelvis', 'SU_Front'):
            base_lt[n] = sk.rest_t[sk.index[n]].copy()
    base = Pose(sk, base_lq, base_lt).fk()
    hand_flat = {s: base.wq['Hand_' + s].copy() for s in 'LR'}
    foot_rest = {s: base.wq['Foot_' + s].copy() for s in 'LR'}
    ground_hand = float(base.wp['Hand_R'][1])
    ground_foot = float(base.wp['Foot_R'][1])
    pelvis0 = base.wp['Pelvis'].copy()

    def side_sign(s):
        return 1.0 if s == 'L' else -1.0     # creature left = +X

    def curl(p, s, amount):
        """Curl the fingers of one hand (0 = as in the base pose)."""
        sg = -side_sign(s)
        for f in FINGERS:
            for k in (1, 2, 3):
                p.lq['%s%d_%s' % (f, k, s)] = q_norm(q_mul(base_lq['%s%d_%s' % (f, k, s)], q_axis([0, 0, 1], sg * amount)))
        for k in (2, 3):
            p.lq['Thumb%d_%s' % (k, s)] = q_norm(q_mul(base_lq['Thumb%d_%s' % (k, s)], q_axis([0, 1, 0], -sg * amount * 0.6)))

    def legs_arms(p, feet, hands, knee_out=0.45, elbow_out=0.75, elbow_back=-1.0, hand_rot=None, foot_pitch=None):
        for s in 'LR':
            sg = side_sign(s)
            fq = foot_rest[s]
            if foot_pitch and foot_pitch.get(s):
                fq = q_mul(q_axis([1, 0, 0], foot_pitch[s]), fq)
            p.limb('Thigh_' + s, 'Knee_' + s, 'Shin_' + s, 'Foot_' + s, feet[s], [sg * knee_out, 0.25, 1.0], fq)
            hq = hand_flat[s] if hand_rot is None else hand_rot.get(s)
            p.limb('UpperArm_' + s, 'Elbow_' + s, 'ForeArm_' + s, 'Hand_' + s, hands[s], [sg * elbow_out, 0.35, elbow_back], hq)

    def step(phase, offset, stance):
        """Foot cycle: returns (forward position -1..1, lift 0..1). Stance moves back at constant speed."""
        u = (phase + offset) % 1.0
        if u < stance:
            return 1.0 - 2.0 * u / stance, 0.0
        v = (u - stance) / (1.0 - stance)
        return -1.0 + 2.0 * smooth(v), float(np.sin(np.pi * v))

    def gait(duration, name, stride, stance, lift, pelvis_h, lean, bob, sway, twist, head_up, hand_fwd, foot_back, hand_wide):
        clip = Clip(name, duration, FPS, loop=True)

        def pose(_t, ph):
            p = base.copy()
            a = 2 * np.pi * ph
            bounce = np.cos(2 * a)                       # two beats per cycle
            p.lt['Pelvis'] = pelvis0 + np.array([sway * np.sin(a), pelvis_h + bob * bounce, 0.0])
            p.bend('Pelvis', x=lean, y=twist * np.sin(a), z=2.0 * np.sin(a))
            for n, f in (('Spine1', 0.3), ('Spine2', 0.3), ('Spine3', 0.25), ('Spine4', 0.15)):
                p.bend(n, x=lean * 0.18 * f + 1.5 * bounce, y=-twist * f * np.sin(a), z=-2.0 * f * np.sin(a))
            for n in ('Neck1', 'Neck2', 'Neck3', 'Neck4'):
                p.bend(n, x=-head_up * 0.2 - 0.8 * bounce, y=-twist * 0.1 * np.sin(a))
            p.bend('Head', x=-head_up * 0.2 + 2.0 * np.cos(2 * a + 0.6))
            p.bend('Jaw', x=6.0 + 3.0 * np.sin(2 * a))
            p.fk()
            feet, hands, fpitch = {}, {}, {}
            for s in 'LR':
                sg = side_sign(s)
                # diagonal pairs: left hand with right foot
                f_pos, f_up = step(ph, 0.0 if s == 'L' else 0.5, stance)
                h_pos, h_up = step(ph, 0.5 if s == 'L' else 0.0, stance)
                fb = base.wp['Foot_' + s]
                feet[s] = np.array([fb[0] * 1.6, ground_foot + lift * f_up, foot_back + stride * 0.5 * f_pos])
                fpitch[s] = 28.0 * f_up * (1.0 if f_pos < 0 else -0.4)
                hb = base.wp['Hand_' + s]
                hands[s] = np.array([sg * hand_wide, ground_hand + lift * 1.15 * h_up, hand_fwd + stride * 0.5 * h_pos])
                curl(p, s, 14.0 * h_up)
                _ = hb
            legs_arms(p, feet, hands, foot_pitch=fpitch)
            return p.out()
        clip.bake(sk, pose)
        return clip

    walk_stride, walk_len = 0.95, 1.25
    run_stride, run_len = 1.55, 0.62
    walk = gait(walk_len, 'Walk', walk_stride, 0.62, 0.20, pelvis_h=0.28, lean=14.0, bob=0.022, sway=0.05, twist=5.0, head_up=10.0,
                hand_fwd=0.95, foot_back=-0.05, hand_wide=0.62)
    run = gait(run_len, 'Run', run_stride, 0.42, 0.34, pelvis_h=0.36, lean=22.0, bob=0.06, sway=0.03, twist=8.0, head_up=22.0,
               hand_fwd=1.15, foot_back=-0.15, hand_wide=0.68)

    # ------------------------------------------------------------------ attack: rear up, right arm slams down
    def reared(p, up, lean_back, head_up):
        """Shared part of Attack and Scream: stand up on the hind legs."""
        p.lt['Pelvis'] = pelvis0 + np.array([0.0, 0.55 * up, -0.1 * up])
        p.bend('Pelvis', x=-lean_back * 0.35 * up)
        for n in ('Spine1', 'Spine2', 'Spine3', 'Spine4'):
            p.bend(n, x=-lean_back * 0.14 * up)
        p.bend('Chest', x=-lean_back * 0.1 * up)
        for n in ('Neck1', 'Neck2', 'Neck3', 'Neck4'):
            p.bend(n, x=-head_up * 0.2 * up)
        p.bend('Head', x=-head_up * 0.2 * up)

    def free_hand(p, s):
        return q_mul(p.wq['ForeArm_' + s], q_axis([0, 1, 0], 0.0))

    attack = Clip('Attack', 1.3, FPS, loop=False)

    def attack_pose(t, ph):
        p = base.copy()
        wind = smooth(ph / 0.38) * (1.0 - smooth((ph - 0.46) / 0.12))        # wind up
        hit = smooth((ph - 0.44) / 0.12) * (1.0 - smooth((ph - 0.68) / 0.3))  # strike and recover
        up = max(wind, hit * 0.55)
        reared(p, up, 46.0, 20.0)
        for n in ('Spine2', 'Spine3', 'Spine4'):
            p.bend(n, y=10.0 * wind - 12.0 * hit, x=16.0 * hit)
        p.bend('Head', x=10.0 * hit)
        p.bend('Jaw', x=8.0 + 24.0 * max(wind, hit))
        p.fk()
        feet = {s: np.array([base.wp['Foot_' + s][0] * 1.6, ground_foot, -0.05]) for s in 'LR'}
        sh = p.wp['UpperArm_R']
        rest_r = np.array([-0.62, ground_hand, 0.75])
        raised = sh + np.array([-0.35, 1.25, -0.25])
        slam = np.array([-0.15, ground_hand + 0.05, 1.75])
        hand_r = rest_r * (1 - wind) + raised * wind
        hand_r = hand_r * (1 - hit) + slam * hit + np.array([0.0, 0.9 * pulse(ph, 0.44, 0.60), 0.0])
        hand_l = np.array([0.62, ground_hand + 0.35 * up, 0.75 - 0.25 * up])
        in_air = max(wind, 0.0) * (1 - hit)
        legs_arms(p, feet, {'L': hand_l, 'R': hand_r}, elbow_back=-1.0 + 1.2 * in_air,
                  hand_rot={'L': hand_flat['L'], 'R': None if in_air > 0.3 else hand_flat['R']})
        curl(p, 'R', 34.0 * in_air)
        return p.out()
    attack.bake(sk, attack_pose)

    # ------------------------------------------------------------------ scream: rear up, arms wide, roar
    scream = Clip('Scream', 2.6, FPS, loop=False)

    def scream_pose(t, ph):
        p = base.copy()
        up = smooth(ph / 0.22) * (1.0 - smooth((ph - 0.8) / 0.2))
        roar = smooth((ph - 0.2) / 0.1) * (1.0 - smooth((ph - 0.74) / 0.1))
        shake = np.sin(t * 38.0) * roar
        reared(p, up, 62.0, 34.0)
        p.bend('Chest', y=2.0 * shake)
        p.bend('Head', y=5.0 * shake, z=3.0 * np.sin(t * 27.0) * roar)
        p.bend('Jaw', x=6.0 + 36.0 * roar + 3.0 * shake)
        p.fk()
        feet = {s: np.array([base.wp['Foot_' + s][0] * 1.8, ground_foot, -0.05]) for s in 'LR'}
        hands = {}
        for s in 'LR':
            sg = side_sign(s)
            down = np.array([sg * 0.62, ground_hand, 0.75])
            wide = p.wp['UpperArm_' + s] + np.array([sg * 0.95, -0.25 + 0.03 * shake, 0.55])
            hands[s] = down * (1 - up) + wide * up
            curl(p, s, 30.0 * up)
        legs_arms(p, feet, hands, elbow_out=0.9, elbow_back=-1.0 + 0.6 * up,
                  hand_rot={s: (None if up > 0.3 else hand_flat[s]) for s in 'LR'})
        return p.out()
    scream.bake(sk, scream_pose)

    # ------------------------------------------------------------------ mesh
    w = GlbWriter()
    w.js['asset']['copyright'] = CREDIT
    js = g.js
    mesh_node = [n for n in js['nodes'] if 'mesh' in n][0]
    new_index = np.zeros(len(src.joint_nodes), dtype=np.int64)
    for n, k in src_k.items():
        new_index[k] = sk.index[n]
    deforming = set(src_k.values())
    mat = js['materials'][0]

    def image(tex_index, max_size, mode=None):
        im = Image.open(io.BytesIO(g.image_bytes(js['textures'][tex_index]['source'])))
        im = im.convert(mode or 'RGB')
        data, mime = tex.encode(im, max_size=max_size, jpeg_quality=90)
        return w.image(data, mime)
    pbr = mat['pbrMetallicRoughness']
    material = w.material('M_Cyclops', image(pbr['baseColorTexture']['index'], 2048),
                          image(mat['normalTexture']['index'], 2048) if 'normalTexture' in mat else None,
                          image(pbr['metallicRoughnessTexture']['index'], 1024) if 'metallicRoughnessTexture' in pbr else None,
                          metallic=pbr.get('metallicFactor', 1.0), roughness=pbr.get('roughnessFactor', 1.0))
    prims = []
    lo, hi, tris = np.full(3, 1e9), np.full(3, -1e9), 0
    for pr in js['meshes'][mesh_node['mesh']]['primitives']:
        a = pr['attributes']
        pos = g.accessor(a['POSITION']).astype(np.float64) @ A[:3, :3].T
        nrm = g.accessor(a['NORMAL']).astype(np.float64) * np.array([-1.0, 1.0, -1.0])
        uv = g.accessor(a['TEXCOORD_0'])
        idx = g.accessor(pr['indices']).reshape(-1)
        J = g.accessor(a['JOINTS_0']).astype(np.int64)
        W = g.accessor(a['WEIGHTS_0']).astype(np.float64)
        W = np.where(np.isin(J, list(deforming)), W, 0.0)
        W = W / np.maximum(W.sum(axis=1, keepdims=True), 1e-9)
        Jn = np.where(W > 0, new_index[J], 0).astype(np.uint16)
        prims.append(w.primitive(pos, nrm, uv, idx, material, extra={'JOINTS_0': (Jn, 'VEC4', False), 'WEIGHTS_0': (W.astype(np.float32), 'VEC4', False)}))
        lo, hi, tris = np.minimum(lo, pos.min(axis=0)), np.maximum(hi, pos.max(axis=0)), tris + len(idx) // 3
    clips = [idle, walk, run, attack, scream]
    write_skinned(w, sk, prims, clips, 'SK_Cyclops', scale=1.0)
    w.save(os.path.join(out_dir, 'SK_Cyclops.glb'))
    crouch_top = float(max(base.wp[n][1] for n in names)) + 0.25
    return {'SK_Cyclops': {
        'kind': 'skeletal', 'file': 'SK_Cyclops.glb', 'tris': int(tris), 'joints': len(names),
        'bind_height_m': round(float(hi[1] - lo[1]), 3), 'crouch_height_m': round(crouch_top, 3),
        'animations': {c.name: {'duration_s': round(c.duration, 3), 'loop': c.loop} for c in clips},
        'walk_speed_mps': round(walk_stride / (0.62 * walk_len), 3), 'run_speed_mps': round(run_stride / (0.42 * run_len), 3),
        'front_marker_bone': 'SU_Front'}}


if __name__ == '__main__':
    import sys
    import json
    print(json.dumps(build(sys.argv[1], sys.argv[2]), indent=1))
