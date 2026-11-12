"""Skeleton / animation toolkit for the asset pipeline.

Quaternions are (x, y, z, w) like glTF. Matrices are 4x4 with column vectors.
"""
import numpy as np


# ------------------------------------------------------------------ quaternion helpers
def q_axis(axis, deg):
    axis = np.asarray(axis, dtype=np.float64)
    axis = axis / max(np.linalg.norm(axis), 1e-12)
    a = np.radians(deg) * 0.5
    return np.array([axis[0] * np.sin(a), axis[1] * np.sin(a), axis[2] * np.sin(a), np.cos(a)])


def q_mul(a, b):
    """Rotation b first, then a."""
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return np.array([aw * bx + ax * bw + ay * bz - az * by,
                     aw * by - ax * bz + ay * bw + az * bx,
                     aw * bz + ax * by - ay * bx + az * bw,
                     aw * bw - ax * bx - ay * by - az * bz])


def q_conj(q):
    return np.array([-q[0], -q[1], -q[2], q[3]])


def q_norm(q):
    return q / max(np.linalg.norm(q), 1e-12)


def q_rotate(q, v):
    return (q_matrix(q) @ np.asarray(v, dtype=np.float64))


def q_matrix(q):
    x, y, z, w = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def q_from_matrix(m):
    m = np.asarray(m, dtype=np.float64)
    t = m[0, 0] + m[1, 1] + m[2, 2]
    if t > 0:
        s = np.sqrt(t + 1.0) * 2
        q = [(m[2, 1] - m[1, 2]) / s, (m[0, 2] - m[2, 0]) / s, (m[1, 0] - m[0, 1]) / s, 0.25 * s]
    elif m[0, 0] > m[1, 1] and m[0, 0] > m[2, 2]:
        s = np.sqrt(1.0 + m[0, 0] - m[1, 1] - m[2, 2]) * 2
        q = [0.25 * s, (m[0, 1] + m[1, 0]) / s, (m[0, 2] + m[2, 0]) / s, (m[2, 1] - m[1, 2]) / s]
    elif m[1, 1] > m[2, 2]:
        s = np.sqrt(1.0 + m[1, 1] - m[0, 0] - m[2, 2]) * 2
        q = [(m[0, 1] + m[1, 0]) / s, 0.25 * s, (m[1, 2] + m[2, 1]) / s, (m[0, 2] - m[2, 0]) / s]
    else:
        s = np.sqrt(1.0 + m[2, 2] - m[0, 0] - m[1, 1]) * 2
        q = [(m[0, 2] + m[2, 0]) / s, (m[1, 2] + m[2, 1]) / s, 0.25 * s, (m[1, 0] - m[0, 1]) / s]
    return q_norm(np.array(q))


def q_between(a, b):
    """Shortest rotation taking direction a to direction b."""
    a = np.asarray(a, dtype=np.float64)
    b = np.asarray(b, dtype=np.float64)
    a = a / max(np.linalg.norm(a), 1e-12)
    b = b / max(np.linalg.norm(b), 1e-12)
    d = float(np.dot(a, b))
    if d > 1 - 1e-9:
        return np.array([0.0, 0.0, 0.0, 1.0])
    if d < -1 + 1e-9:
        axis = np.cross(a, [1.0, 0.0, 0.0])
        if np.linalg.norm(axis) < 1e-6:
            axis = np.cross(a, [0.0, 1.0, 0.0])
        return q_axis(axis, 180.0)
    axis = np.cross(a, b)
    return q_norm(np.array([axis[0], axis[1], axis[2], 1.0 + d]))


def q_slerp(a, b, t):
    a = np.asarray(a, dtype=np.float64)
    b = np.asarray(b, dtype=np.float64)
    d = float(np.dot(a, b))
    if d < 0:
        b, d = -b, -d
    if d > 0.9995:
        return q_norm(a + (b - a) * t)
    th = np.arccos(np.clip(d, -1, 1))
    return (np.sin((1 - t) * th) * a + np.sin(t * th) * b) / np.sin(th)


IDENT = np.array([0.0, 0.0, 0.0, 1.0])


def trs(t, q, s=(1.0, 1.0, 1.0)):
    m = np.eye(4)
    m[:3, :3] = q_matrix(q) * np.asarray(s, dtype=np.float64)[None, :]
    m[:3, 3] = t
    return m


def smooth(x):
    x = np.clip(x, 0.0, 1.0)
    return x * x * (3 - 2 * x)


def pulse(phase, start, end):
    """0 -> 1 -> 0 bump between start and end (phase units), smooth."""
    if phase <= start or phase >= end:
        return 0.0
    u = (phase - start) / (end - start)
    return float(np.sin(np.pi * u) ** 2)


# ------------------------------------------------------------------ skeleton
class Skeleton:
    """Joints with rest local TRS. parents must come before children."""

    def __init__(self):
        self.names, self.parent, self.rest_t, self.rest_q, self.rest_s = [], [], [], [], []
        self.index = {}

    def add(self, name, parent, t, q=IDENT, s=(1.0, 1.0, 1.0)):
        self.index[name] = len(self.names)
        self.names.append(name)
        self.parent.append(-1 if parent is None else self.index[parent])
        self.rest_t.append(np.asarray(t, dtype=np.float64))
        self.rest_q.append(np.asarray(q, dtype=np.float64))
        self.rest_s.append(np.asarray(s, dtype=np.float64))
        return self.index[name]

    @staticmethod
    def from_world_positions(joints):
        """joints: list of (name, parent or None, world position). Rest rotations are identity."""
        sk = Skeleton()
        pos = {}
        for name, parent, p in joints:
            p = np.asarray(p, dtype=np.float64)
            pos[name] = p
            sk.add(name, parent, p - (pos[parent] if parent else 0.0))
        return sk

    def world(self, local_q=None, local_t=None, local_s=None):
        """World matrices for a pose. Missing joints use the rest values."""
        out = [None] * len(self.names)
        for i, name in enumerate(self.names):
            q = self.rest_q[i] if not local_q or name not in local_q else local_q[name]
            t = self.rest_t[i] if not local_t or name not in local_t else local_t[name]
            s = self.rest_s[i] if not local_s or name not in local_s else local_s[name]
            m = trs(t, q, s)
            out[i] = m if self.parent[i] < 0 else out[self.parent[i]] @ m
        return out

    def children(self, name):
        i = self.index[name]
        return [self.names[k] for k, p in enumerate(self.parent) if p == i]


class Clip:
    """Sampled animation: per joint local rotation / translation / scale tracks at fixed fps."""

    def __init__(self, name, duration, fps=30, loop=True):
        self.name, self.duration, self.fps, self.loop = name, float(duration), fps, loop
        n = int(round(duration * fps))
        self.times = np.linspace(0.0, duration, n + 1)
        self.rot, self.trans, self.scale = {}, {}, {}

    def bake(self, skeleton, pose_fn):
        """pose_fn(time, phase 0..1) -> (dict joint->quat, dict joint->local translation[, dict joint->scale])."""
        frames = []
        for t in self.times:
            res = pose_fn(t, t / self.duration)
            frames.append(res)
        joints_q = set().union(*[set(f[0].keys()) for f in frames])
        joints_t = set().union(*[set(f[1].keys()) for f in frames])
        for j in joints_q:
            i = skeleton.index[j]
            track = np.array([f[0].get(j, skeleton.rest_q[i]) for f in frames])
            for k in range(1, len(track)):       # keep neighbouring quaternions in the same hemisphere
                if np.dot(track[k], track[k - 1]) < 0:
                    track[k] = -track[k]
            self.rot[j] = track
        for j in joints_t:
            i = skeleton.index[j]
            self.trans[j] = np.array([f[1].get(j, skeleton.rest_t[i]) for f in frames])
        if self.loop:
            for d in (self.rot, self.trans):
                for j in d:
                    d[j][-1] = d[j][0]
        return self


def write_skinned(writer, skeleton, mesh_prims, clips, mesh_name, scale=1.0, extra_root_children=None):
    """Adds joints, skin, mesh node and animations to a GlbWriter.

    mesh_prims: list of primitive dicts created with writer.primitive(...) including JOINTS_0 / WEIGHTS_0.
    Positions (rest translations, animation translations) are multiplied by `scale`.
    """
    base = len(writer.js['nodes'])
    for i, name in enumerate(skeleton.names):
        node = {'name': name, 'translation': [float(v) for v in skeleton.rest_t[i] * scale]}
        if not np.allclose(skeleton.rest_q[i], IDENT):
            node['rotation'] = [float(v) for v in skeleton.rest_q[i]]
        if not np.allclose(skeleton.rest_s[i], 1.0):
            node['scale'] = [float(v) for v in skeleton.rest_s[i]]
        writer.js['nodes'].append(node)
    for i, p in enumerate(skeleton.parent):
        if p >= 0:
            writer.js['nodes'][base + p].setdefault('children', []).append(base + i)
    # inverse bind matrices from the rest pose (scaled translations)
    sk2 = Skeleton()
    for i, name in enumerate(skeleton.names):
        sk2.add(name, None if skeleton.parent[i] < 0 else skeleton.names[skeleton.parent[i]], skeleton.rest_t[i] * scale, skeleton.rest_q[i], skeleton.rest_s[i])
    world = sk2.world()
    ibm = np.array([np.linalg.inv(m).T.reshape(-1) for m in world], dtype=np.float32)   # column-major
    writer.js.setdefault('skins', []).append({'joints': [base + i for i in range(len(skeleton.names))],
                                              'inverseBindMatrices': writer.accessor(ibm, 'MAT4'), 'skeleton': base})
    mesh = writer.mesh(mesh_name, mesh_prims)
    mesh_node = writer.node(mesh_name, mesh=mesh, skin=len(writer.js['skins']) - 1, root=True)
    roots = [base + i for i, p in enumerate(skeleton.parent) if p < 0]
    writer.js['scenes'][0]['nodes'].extend(roots)
    anims = writer.js.setdefault('animations', [])
    for clip in clips:
        samplers, channels = [], []
        t_acc = writer.accessor(clip.times.astype(np.float32).reshape(-1, 1), 'SCALAR', minmax=True)
        for j, track in clip.rot.items():
            samplers.append({'input': t_acc, 'output': writer.accessor(track.astype(np.float32), 'VEC4'), 'interpolation': 'LINEAR'})
            channels.append({'sampler': len(samplers) - 1, 'target': {'node': base + skeleton.index[j], 'path': 'rotation'}})
        for j, track in clip.trans.items():
            samplers.append({'input': t_acc, 'output': writer.accessor((track * scale).astype(np.float32), 'VEC3'), 'interpolation': 'LINEAR'})
            channels.append({'sampler': len(samplers) - 1, 'target': {'node': base + skeleton.index[j], 'path': 'translation'}})
        for j, track in clip.scale.items():
            samplers.append({'input': t_acc, 'output': writer.accessor(track.astype(np.float32), 'VEC3'), 'interpolation': 'LINEAR'})
            channels.append({'sampler': len(samplers) - 1, 'target': {'node': base + skeleton.index[j], 'path': 'scale'}})
        anims.append({'name': clip.name, 'samplers': samplers, 'channels': channels})
    return mesh_node


def seg_distance(p, a, b):
    """Distance of points p (N,3) to segment a-b, and the parameter along it."""
    ab = b - a
    t = np.clip(((p - a) @ ab) / max(float(ab @ ab), 1e-12), 0.0, 1.0)
    return np.linalg.norm(p - (a + t[:, None] * ab), axis=1), t


def top4(weights):
    """(N, J) weights -> joints uint16 (N,4), weights float32 (N,4) normalized."""
    order = np.argsort(-weights, axis=1)[:, :4]
    w = np.take_along_axis(weights, order, axis=1)
    w = w / np.maximum(w.sum(axis=1, keepdims=True), 1e-12)
    w[w < 0.01] = 0.0
    w = w / np.maximum(w.sum(axis=1, keepdims=True), 1e-12)
    order = np.where(w > 0, order, 0)
    return order.astype(np.uint16), w.astype(np.float32)


def smooth_weights(weights, pos, idx, iterations=4, weld_tol=1e-3, lock=None):
    """Laplacian smoothing of skin weights over the (position welded) mesh graph."""
    key = np.round(pos / weld_tol).astype(np.int64)
    _, inv = np.unique(key, axis=0, return_inverse=True)
    inv = inv.reshape(-1)
    n = inv.max() + 1
    w = np.zeros((n, weights.shape[1]))
    cnt = np.zeros(n)
    np.add.at(w, inv, weights)
    np.add.at(cnt, inv, 1)
    w /= cnt[:, None]
    tri = inv[idx].reshape(-1, 3)
    edges = np.concatenate([tri[:, [0, 1]], tri[:, [1, 2]], tri[:, [2, 0]]])
    edges = np.concatenate([edges, edges[:, ::-1]])
    locked = None
    if lock is not None:
        locked = np.zeros(n, dtype=bool)
        np.logical_or.at(locked, inv, lock)
    for _ in range(iterations):
        acc = np.zeros_like(w)
        deg = np.zeros(n)
        np.add.at(acc, edges[:, 0], w[edges[:, 1]])
        np.add.at(deg, edges[:, 0], 1)
        new = (w + acc / np.maximum(deg, 1)[:, None]) * 0.5
        if locked is not None:
            new[locked] = w[locked]
        w = new
    w /= np.maximum(w.sum(axis=1, keepdims=True), 1e-12)
    return w[inv]
