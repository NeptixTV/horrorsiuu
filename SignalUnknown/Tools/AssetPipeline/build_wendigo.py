"""Wendigo: rig + skin weights + animations for the static A-pose model.

Source: "Wendigo" by DannyAg (https://sketchfab.com/DannyAg), CC-BY-4.0. The source has no skeleton.
This script builds one (22 joints), computes skin weights and bakes the animation clips
Idle, Walk, Run, Attack and Scream. Scaled so the skull top is at 2.55 m (antler tips ~3 m).

Conventions: +Y up, the creature faces +Z, its left side is +X. All rest rotations are identity,
so local rotations are rotations about world axes in the bind pose:
  +X rotation: a hanging limb swings BACK, the torso leans FORWARD, the head nods DOWN
  +Y rotation: turn to the creature's left
  +Z rotation: the left side goes up (lean / roll to the right)
"""
import os
import numpy as np
from PIL import Image
from gltfio import Gltf, GlbWriter
from meshutil import flatten, tri_components, Part
import textures as tex
from rig import (Skeleton, Clip, write_skinned, seg_distance, top4, smooth_weights, q_axis, q_mul, IDENT, smooth, pulse)

CREDIT = '"Wendigo" by DannyAg (https://sketchfab.com/DannyAg), CC-BY-4.0, rigged and animated for Signal Unknown'
GROUND = -8.37
SKULL_TOP = 184.0
TARGET_HEIGHT = 2.55

# joint, parent, position in source units
JOINTS = [
    ('Root', None, (0, GROUND, 0)),
    ('Pelvis', 'Root', (0, 84, -2)),
    ('Spine1', 'Pelvis', (0, 100, -8)),
    ('Spine2', 'Spine1', (0, 118, -6)),
    ('Chest', 'Spine2', (0, 134, -5)),
    ('Neck', 'Chest', (0, 150, -1)),
    ('Head', 'Neck', (0, 160, 6)),
    ('Clavicle_L', 'Chest', (8, 142, -5)),
    ('UpperArm_L', 'Clavicle_L', (27, 138, -6)),
    ('ForeArm_L', 'UpperArm_L', (43, 106, -16)),
    ('Hand_L', 'ForeArm_L', (72, 62, 8)),
    ('Clavicle_R', 'Chest', (-8, 142, -5)),
    ('UpperArm_R', 'Clavicle_R', (-27, 138, -6)),
    ('ForeArm_R', 'UpperArm_R', (-43, 106, -16)),
    ('Hand_R', 'ForeArm_R', (-72, 62, 8)),
    ('Thigh_L', 'Pelvis', (13, 80, 0)),
    ('Shin_L', 'Thigh_L', (17.5, 45, 3)),
    ('Foot_L', 'Shin_L', (18, 4, -3)),
    ('Thigh_R', 'Pelvis', (-13, 80, 0)),
    ('Shin_R', 'Thigh_R', (-17.5, 45, 3)),
    ('Foot_R', 'Shin_R', (-18, 4, -3)),
    ('LoinFront', 'Pelvis', (0, 98, 6)),
    ('LoinBack', 'Pelvis', (0, 98, -12)),
    ('SU_Front', 'Root', (0, GROUND, 1.0 / (2.55 / (184.0 - GROUND)))),     # marker 1 m in front: lets the game detect the facing direction
]

# bone segments used for the automatic weights: joint -> tail position
TAILS = {
    'Pelvis': (0, 96, -6), 'Spine1': (0, 116, -6), 'Spine2': (0, 132, -5), 'Chest': (0, 148, -2), 'Neck': (0, 158, 5), 'Head': (0, 176, 12),
    'Clavicle_L': (25, 139, -6), 'UpperArm_L': (43, 106, -16), 'ForeArm_L': (72, 62, 8), 'Hand_L': (77, 46, 10),
    'Clavicle_R': (-25, 139, -6), 'UpperArm_R': (-43, 106, -16), 'ForeArm_R': (-72, 62, 8), 'Hand_R': (-77, 46, 10),
    'Thigh_L': (17.5, 45, 3), 'Shin_L': (18, 4, -3), 'Foot_L': (19, -6, 8),
    'Thigh_R': (-17.5, 45, 3), 'Shin_R': (-18, 4, -3), 'Foot_R': (-19, -6, 8),
}


def body_weights(sk, pos, idx):
    names = sk.names
    head = {n: np.array(p, dtype=np.float64) for n, _, p in JOINTS}
    n = len(pos)
    w = np.zeros((n, len(names)))
    x, y = pos[:, 0], pos[:, 1]
    for name, tail in TAILS.items():
        d, _ = seg_distance(pos, head[name], np.array(tail, dtype=np.float64))
        wi = 1.0 / np.power(d + 1.5, 4)
        side = 1 if name.endswith('_L') else (-1 if name.endswith('_R') else 0)
        limb = name.split('_')[0]
        allowed = np.ones(n, dtype=bool)
        if side:
            allowed &= (x * side > -2.0)                       # never across the body
        if limb in ('UpperArm', 'ForeArm', 'Hand'):
            allowed &= (np.abs(x) > 19.0) & (y > 40.0) & ~((np.abs(x) < 28.0) & (y < 118.0))   # not the torso, not the legs
        if limb in ('ForeArm', 'Hand'):
            allowed &= (np.abs(x) > 36.0)
        if limb == 'Hand':
            allowed &= (y < 72.0)
        if limb in ('Thigh', 'Shin', 'Foot'):
            allowed &= (y < 92.0) & (np.abs(x) < 30.0)
        if limb in ('Shin', 'Foot'):
            allowed &= (y < 60.0)
        if limb == 'Foot':
            allowed &= (y < 14.0)
        if limb in ('Pelvis', 'Spine1', 'Spine2', 'Chest', 'Neck', 'Head'):
            arm = (np.abs(x) > 30.0) & (y < 126.0)
            leg = (y < 62.0)
            allowed &= ~arm & ~leg
        if limb in ('Spine1', 'Spine2', 'Chest', 'Neck', 'Head', 'Clavicle'):
            allowed &= (y > 86.0)
        if limb == 'Clavicle':
            allowed &= (y > 122.0) & (np.abs(x) < 34.0)
        if limb in ('Neck', 'Head'):
            allowed &= (y > 144.0) & (np.abs(x) < 14.0)
        w[:, sk.index[name]] = wi * allowed
    # hands and feet are rigid
    hand = (np.abs(x) > 60.0) & (y < 60.0)
    for s, nm in ((1, 'Hand_L'), (-1, 'Hand_R')):
        m = hand & (x * s > 0)
        w[m] = 0.0
        w[m, sk.index[nm]] = 1.0
    for s, nm in ((1, 'Foot_L'), (-1, 'Foot_R')):
        m = (y < 0.0) & (x * s > 0)
        w[m] = 0.0
        w[m, sk.index[nm]] = 1.0
    assert (w.sum(axis=1) > 0).all(), 'vertex without any allowed bone'
    w /= w.sum(axis=1, keepdims=True)
    lock = hand | (y < 0.0)
    return smooth_weights(w, pos, idx, iterations=5, weld_tol=1e-2, lock=lock)


def rigid(sk, n, joint):
    w = np.zeros((n, len(sk.names)))
    w[:, sk.index[joint]] = 1.0
    return w


def blend_y(sk, pos, low_joint, high_joint, y0, y1):
    w = np.zeros((len(pos), len(sk.names)))
    t = np.clip((pos[:, 1] - y0) / (y1 - y0), 0.0, 1.0)
    w[:, sk.index[low_joint]] = 1 - t
    w[:, sk.index[high_joint]] = t
    return w


def loin_weights(sk, pos):
    w = np.zeros((len(pos), len(sk.names)))
    hang = smooth((98.0 - pos[:, 1]) / 14.0)        # 0 at the belt, 1 from 14 units below
    front = pos[:, 2] > -5.0
    w[:, sk.index['Pelvis']] = 1 - hang
    w[front, sk.index['LoinFront']] = hang[front]
    w[~front, sk.index['LoinBack']] = hang[~front]
    return w


# ------------------------------------------------------------------ animation
X, Y, Z = (1, 0, 0), (0, 1, 0), (0, 0, 1)


def rx(d): return q_axis(X, d)
def ry(d): return q_axis(Y, d)
def rz(d): return q_axis(Z, d)


def mul(*qs):
    out = IDENT
    for q in qs:
        out = q_mul(out, q)
    return out


ELBOW_AXIS = {}


def elbow_axis(side):
    """Hinge axis of the elbow from the bind pose (upper arm x forearm)."""
    if side not in ELBOW_AXIS:
        j = {n: np.array(p, dtype=np.float64) for n, _, p in JOINTS}
        s = '_L' if side > 0 else '_R'
        up = j['ForeArm' + s] - j['UpperArm' + s]
        fo = j['Hand' + s] - j['ForeArm' + s]
        a = np.cross(up, fo)
        ELBOW_AXIS[side] = a / np.linalg.norm(a)
    return ELBOW_AXIS[side]


def leg(p, fwd, back, knee_stance, knee_swing):
    """Thigh / knee / foot angles (degrees) for a leg at cycle position p in 0..1 (0 = heel strike)."""
    p = p % 1.0
    if p < 0.55:                       # stance
        u = p / 0.55
        thigh = -fwd + (fwd + back) * u
        knee = knee_stance * (1.0 + 0.8 * np.sin(np.pi * u))
        foot = -(thigh + knee) * 0.85 + 14.0 * smooth((u - 0.75) / 0.25)       # heel lifts before toe-off
    else:                              # swing
        u = (p - 0.55) / 0.45
        thigh = back - (fwd + back) * smooth(u)
        knee = knee_stance + knee_swing * np.sin(np.pi * min(u * 1.15, 1.0))
        foot = 14.0 * (1 - smooth(u * 2.0)) - 8.0 * np.sin(np.pi * u)
    return thigh, knee, foot


def gait(phase, fwd, back, knee_stance, knee_swing, lean, arm_swing, bob, sway, hip_yaw, head_up, elbow, reach=0.0):
    q, t = {}, {}
    tl, kl, fl = leg(phase, fwd, back, knee_stance, knee_swing)
    tr, kr, fr = leg(phase + 0.5, fwd, back, knee_stance, knee_swing)
    s2 = np.sin(2 * np.pi * phase)
    c4 = np.cos(4 * np.pi * phase)
    q['Thigh_L'], q['Shin_L'], q['Foot_L'] = rx(tl), rx(kl), rx(fl)
    q['Thigh_R'], q['Shin_R'], q['Foot_R'] = rx(tr), rx(kr), rx(fr)
    # pelvis: lowest in double support, shifted over the stance leg, turned towards the forward leg
    pelvis = np.array([0.0, 84.0 - GROUND, -2.0])
    t['Pelvis'] = pelvis + np.array([sway * np.sin(2 * np.pi * (phase - 0.02)), -bob * (0.5 + 0.5 * c4) - 0.6 * (knee_stance), 0.0])
    q['Pelvis'] = mul(ry(-hip_yaw * np.cos(2 * np.pi * phase)), rz(-2.5 * s2))
    q['Spine1'] = mul(rx(lean * 0.45), ry(hip_yaw * 0.7 * np.cos(2 * np.pi * phase)))
    q['Spine2'] = mul(rx(lean * 0.35), ry(hip_yaw * 0.6 * np.cos(2 * np.pi * phase)), rz(2.0 * s2))
    q['Chest'] = rx(lean * 0.2 + 1.5 * c4)
    q['Neck'] = rx(-lean * 0.45 - head_up * 0.4)
    q['Head'] = mul(rx(-lean * 0.4 - head_up * 0.6), ry(3.0 * np.sin(2 * np.pi * phase + 0.6)))
    # arms swing against the legs (left arm with the right leg); +X rotation = back
    al = arm_swing * np.cos(2 * np.pi * phase) * -1.0
    ar = -al
    q['UpperArm_L'] = mul(rx(al - reach), rz(-10.0))
    q['UpperArm_R'] = mul(rx(ar - reach), rz(10.0))
    q['ForeArm_L'] = q_axis(elbow_axis(1), elbow + 10.0 * max(0.0, -al / max(arm_swing, 1e-6)))
    q['ForeArm_R'] = q_axis(elbow_axis(-1), elbow + 10.0 * max(0.0, -ar / max(arm_swing, 1e-6)))
    q['Hand_L'] = rx(6.0 * np.sin(2 * np.pi * phase))
    q['Hand_R'] = rx(-6.0 * np.sin(2 * np.pi * phase))
    # loincloth flaps follow the leading / trailing thigh
    q['LoinFront'] = rx(min(tl, tr, 0.0) * 0.75 - 4.0)
    q['LoinBack'] = rx(max(tl, tr, 0.0) * 0.7 + 3.0)
    return q, t


def pose_idle(time, phase):
    q, t = {}, {}
    b = np.sin(2 * np.pi * phase * 2)                 # breathing, two breaths per loop
    s = np.sin(2 * np.pi * phase)
    t['Pelvis'] = np.array([1.2 * s, 84.0 - GROUND - 1.0 + 0.35 * b, -2.0])
    q['Pelvis'] = rz(-1.5 * s)
    q['Spine1'] = mul(rx(6.0 + 0.6 * b), rz(1.0 * s))
    q['Spine2'] = mul(rx(5.0 + 1.0 * b), rz(0.8 * s))
    q['Chest'] = rx(2.0 + 1.6 * b)
    q['Neck'] = mul(rx(-4.0 - 0.8 * b), ry(9.0 * np.sin(2 * np.pi * phase + 1.0)))
    q['Head'] = mul(rx(-5.0), ry(11.0 * np.sin(2 * np.pi * phase + 0.4)), rz(7.0 * np.sin(2 * np.pi * phase * 1.0 + 2.0)))
    for side, sfx in ((1, '_L'), (-1, '_R')):
        q['UpperArm' + sfx] = mul(rx(2.0 * np.sin(2 * np.pi * phase + side)), rz(-side * (11.0 + 1.0 * b)))
        q['ForeArm' + sfx] = q_axis(elbow_axis(side), 12.0 + 3.0 * np.sin(2 * np.pi * phase + 2.0 * side))
        q['Hand' + sfx] = rx(5.0 * np.sin(2 * np.pi * phase * 2 + side))
        q['Thigh' + sfx] = mul(rx(-3.0), rz(side * 1.5 * s))
        q['Shin' + sfx] = rx(6.0)
        q['Foot' + sfx] = rx(-3.0)
    q['LoinFront'] = rx(-3.0 + 1.5 * np.sin(2 * np.pi * phase * 2 + 0.5))
    q['LoinBack'] = rx(2.0 + 1.5 * np.sin(2 * np.pi * phase * 2 + 2.0))
    return q, t


def pose_walk(time, phase):
    return gait(phase, fwd=24.0, back=20.0, knee_stance=7.0, knee_swing=48.0, lean=16.0, arm_swing=13.0, bob=2.2, sway=2.2, hip_yaw=6.0, head_up=2.0, elbow=6.0)


def pose_run(time, phase):
    return gait(phase, fwd=44.0, back=34.0, knee_stance=14.0, knee_swing=82.0, lean=34.0, arm_swing=34.0, bob=4.5, sway=1.2, hip_yaw=10.0, head_up=10.0, elbow=38.0, reach=10.0)


def pose_attack(time, phase):
    """Right arm: wind up (0-0.38), strike (0.38-0.55), recover."""
    q, t = pose_idle(0.0, 0.0)
    wind = smooth(phase / 0.36) * (1.0 - smooth((phase - 0.36) / 0.16))
    strike = smooth((phase - 0.36) / 0.16) * (1.0 - smooth((phase - 0.60) / 0.40))
    t['Pelvis'] = np.array([0.0, 84.0 - GROUND - 1.0 - 5.0 * strike, -2.0 + 9.0 * strike - 3.0 * wind])
    q['Pelvis'] = ry(-10.0 * wind + 14.0 * strike)
    q['Spine1'] = mul(rx(6.0 - 8.0 * wind + 18.0 * strike), ry(-12.0 * wind + 16.0 * strike))
    q['Spine2'] = mul(rx(5.0 - 6.0 * wind + 14.0 * strike), ry(-14.0 * wind + 18.0 * strike))
    q['Chest'] = rx(2.0 - 5.0 * wind + 8.0 * strike)
    q['Neck'] = rx(-4.0 - 6.0 * strike)
    q['Head'] = mul(rx(-5.0 - 12.0 * strike + 8.0 * wind), ry(8.0 * wind - 6.0 * strike))
    # right arm (-X side): raise it up and back, then swipe forward and down across the body
    q['Clavicle_R'] = rz(-14.0 * wind + 4.0 * strike)
    q['UpperArm_R'] = mul(rx(55.0 * wind - 95.0 * strike), rz(-70.0 * wind - 25.0 * strike), ry(-30.0 * strike))
    q['ForeArm_R'] = q_axis(elbow_axis(-1), 6.0 + 55.0 * wind - 5.0 * strike)
    q['Hand_R'] = rx(-20.0 * wind + 25.0 * strike)
    q['UpperArm_L'] = mul(rx(-18.0 * wind + 28.0 * strike), rz(-6.0 - 10.0 * strike))
    q['ForeArm_L'] = q_axis(elbow_axis(1), 10.0 + 25.0 * strike)
    q['Thigh_L'] = rx(-3.0 - 24.0 * strike + 6.0 * wind)
    q['Shin_L'] = rx(6.0 + 26.0 * strike)
    q['Foot_L'] = rx(-3.0 - 2.0 * strike)
    q['Thigh_R'] = rx(-3.0 + 14.0 * strike - 4.0 * wind)
    q['Shin_R'] = rx(6.0 + 8.0 * strike)
    q['LoinFront'] = rx(-3.0 - 16.0 * strike)
    q['LoinBack'] = rx(2.0 + 10.0 * strike)
    return q, t


def pose_scream(time, phase):
    q, t = pose_idle(0.0, 0.0)
    up = smooth(phase / 0.22) * (1.0 - smooth((phase - 0.78) / 0.22))
    shake = up * np.sin(time * 46.0) * (0.6 + 0.4 * np.sin(time * 17.0))
    t['Pelvis'] = np.array([0.0, 84.0 - GROUND - 1.0 + 2.5 * up, -2.0 - 2.0 * up])
    q['Spine1'] = rx(6.0 - 14.0 * up + 0.6 * shake)
    q['Spine2'] = rx(5.0 - 12.0 * up + 0.8 * shake)
    q['Chest'] = rx(2.0 - 10.0 * up + shake)
    q['Neck'] = rx(-4.0 - 18.0 * up + 1.5 * shake)
    q['Head'] = mul(rx(-5.0 - 26.0 * up + 2.5 * shake), rz(3.0 * shake))
    for side, sfx in ((1, '_L'), (-1, '_R')):
        q['Clavicle' + sfx] = rz(side * 10.0 * up)
        q['UpperArm' + sfx] = mul(rx(22.0 * up), rz(side * (34.0 * up + 1.5 * shake)))
        q['ForeArm' + sfx] = q_axis(elbow_axis(side), 6.0 + 24.0 * up + 2.0 * shake)
        q['Hand' + sfx] = rx(-24.0 * up)
        q['Thigh' + sfx] = mul(rx(-3.0 - 5.0 * up), rz(side * 4.0 * up))
        q['Shin' + sfx] = rx(6.0 + 10.0 * up)
    q['LoinFront'] = rx(-3.0 + 2.0 * shake)
    q['LoinBack'] = rx(2.0 + 5.0 * up)
    return q, t


CLIPS = [('Idle', 6.0, True, pose_idle), ('Walk', 1.5, True, pose_walk), ('Run', 0.72, True, pose_run),
         ('Attack', 1.1, False, pose_attack), ('Scream', 2.4, False, pose_scream)]


def build(src_dir, out_dir):
    g = Gltf(os.path.join(src_dir, 'scene.gltf'))
    parts = {p.name: p for p in flatten(g)}
    sk = Skeleton.from_world_positions([(n, p, pos) for n, p, pos in JOINTS])
    scale = TARGET_HEIGHT / (SKULL_TOP - GROUND)

    weighted = []   # (Part, weights)
    # skull, antlers, rib cage (Object_2) and jaw (Object_3)
    o2 = parts['Object_2']
    comp = tri_components(o2, 1e-2)
    for c in range(comp.max() + 1):
        sub = o2.select_tris(comp == c)
        lo, hi = sub.bounds()
        if hi[1] < 140.0:
            w = blend_y(sk, sub.pos, 'Spine2', 'Chest', 114.0, 132.0)
        else:
            w = rigid(sk, len(sub.pos), 'Head')
        weighted.append((sub, w))
    weighted.append((parts['Object_3'], rigid(sk, len(parts['Object_3'].pos), 'Head')))
    # body and loincloth (Object_4)
    o4 = parts['Object_4']
    comp = tri_components(o4, 1e-2)
    big = int(np.argmax(np.bincount(comp)))
    body = o4.select_tris(comp == big)
    weighted.append((body, body_weights(sk, body.pos, body.idx)))
    cloth = o4.select_tris(comp != big)
    weighted.append((cloth, loin_weights(sk, cloth.pos)))

    tdir = os.path.join(src_dir, 'textures')

    def orm(ao_file, roughness):
        ao = Image.open(os.path.join(tdir, ao_file)).convert('L').resize((1024, 1024), Image.LANCZOS)
        a = np.asarray(ao)
        return Image.fromarray(np.stack([a, np.full_like(a, int(roughness * 255)), np.zeros_like(a)], axis=-1), 'RGB')

    w = GlbWriter()
    w.js['asset']['copyright'] = CREDIT
    mats = {}
    for mi, (prefix, rough) in enumerate((('blinn10SG', 0.62), ('blinn11SG', 0.7))):
        base = w.image(*tex.encode(Image.open(os.path.join(tdir, prefix + '_diffuse.png')), 2048, 88))
        nrm = w.image(*tex.encode(Image.open(os.path.join(tdir, prefix + '_normal.png')), 2048, 92))
        o = w.image(*tex.encode(orm(prefix + '_occlusion.png', rough), 1024, 88))
        mats[mi] = w.material('M_Wendigo_' + ('Bone' if mi == 0 else 'Body'), base, nrm, o, metallic=1.0, roughness=1.0)

    prims = []
    for mi in (0, 1):
        group = [(p, ww) for p, ww in weighted if p.material == mi]
        pos = np.concatenate([p.pos for p, _ in group])
        nrm = np.concatenate([p.nrm for p, _ in group])
        uv = np.concatenate([p.uv for p, _ in group])
        offs = np.cumsum([0] + [len(p.pos) for p, _ in group[:-1]])
        idx = np.concatenate([p.idx + o for (p, _), o in zip(group, offs)])
        wt = np.concatenate([ww for _, ww in group])
        joints, weights = top4(wt)
        pos_m = (pos - np.array([0.0, GROUND, 0.0])) * scale
        prims.append(w.primitive(pos_m, nrm, uv, idx, mats[mi], extra={'JOINTS_0': (joints, 'VEC4', False), 'WEIGHTS_0': (weights, 'VEC4', False)}))

    # the skeleton lives in a ground-relative frame as well
    sk_m = Skeleton.from_world_positions([(n, p, (pos[0], pos[1] - GROUND, pos[2])) for n, p, pos in JOINTS])
    clips = [Clip(name, dur, 30, loop).bake(sk_m, fn) for name, dur, loop, fn in CLIPS]
    write_skinned(w, sk_m, prims, clips, 'SK_Wendigo', scale=scale)
    w.save(os.path.join(out_dir, 'SK_Wendigo.glb'))
    return {'SK_Wendigo': {'kind': 'skeletal', 'file': 'SK_Wendigo.glb', 'height_m': TARGET_HEIGHT, 'total_height_m': round((218.2 - GROUND) * scale, 2),
                           'animations': [c[0] for c in CLIPS], 'walk_speed_mps': 1.25, 'run_speed_mps': 5.6}}
