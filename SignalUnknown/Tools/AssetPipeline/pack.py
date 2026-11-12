"""Packs pipeline GLB files into .sumodel files, the binary format the game loads at start.

Everything is converted to Unreal's space here (centimetres, +Z up, left-handed, +X forward) so the C++
loader only has to read arrays:

    Unreal (X, Y, Z) = (pipeline z, -pipeline x, pipeline y) * 100

Layout (little endian), strings = u16 length + UTF-8:
    'SUMD', u32 version
    textures : u32 n, each  name, u8 kind (0 colour, 1 normal), u8 encoding (0 PNG, 1 JPEG), u32 w, u32 h, u32 size, bytes
    materials: u32 n, each  name, i32 base texture, i32 normal texture, f32 colour[4], f32 roughness, f32 metallic,
                            u8 masked, f32 cutoff, u8 two sided
    meshes   : u32 n, each  name, u8 skinned, f32 min[3], f32 max[3], u32 sections, each section:
                            i32 material, u32 vertices, u32 indices, f32 pos[3n], f32 normal[3n], f32 tangent[4n], f32 uv[2n],
                            u32 index[], (skinned: u8 joint[4n], f32 weight[4n])
    joints   : u32 n, each  name, i32 parent, f32 rest translation[3], f32 rest rotation[4], f32 inverse bind[12] (3x4, row major)
    clips    : u32 n, each  name, f32 duration, u8 loop, u32 frames, per joint f32 rotation[4 * frames], f32 translation[3 * frames]
    checks   : u32 n, each  u32 clip, f32 time, u32 vertex (mesh 0, section 0), f32 position[3]   (reference values for the tests)
    meta     : u32 n, each  key, f32 value
"""
import io
import struct
import numpy as np
from PIL import Image
from gltfio import Gltf

VERSION = 1


def ue_vec(v):
    v = np.asarray(v, dtype=np.float64)
    return np.stack([v[..., 2], -v[..., 0], v[..., 1]], axis=-1)


def ue_quat(q):
    q = np.asarray(q, dtype=np.float64)
    return np.stack([-q[..., 2], q[..., 0], -q[..., 1], q[..., 3]], axis=-1)


def q_matrix(q):
    x, y, z, w = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def tangents(pos, nrm, uv, idx):
    """Per vertex tangent (xyz) and handedness sign for Unreal: bitangent = cross(N, T) * sign points along +V."""
    tri = idx.reshape(-1, 3)
    p0, p1, p2 = pos[tri[:, 0]], pos[tri[:, 1]], pos[tri[:, 2]]
    d1, d2 = uv[tri[:, 1]] - uv[tri[:, 0]], uv[tri[:, 2]] - uv[tri[:, 0]]
    det = d1[:, 0] * d2[:, 1] - d2[:, 0] * d1[:, 1]
    det = np.where(np.abs(det) < 1e-12, 1e-12, det)
    t = ((p1 - p0) * d2[:, 1:2] - (p2 - p0) * d1[:, 1:2]) / det[:, None]
    b = ((p2 - p0) * d1[:, 0:1] - (p1 - p0) * d2[:, 0:1]) / det[:, None]
    tan, bit = np.zeros_like(pos), np.zeros_like(pos)
    for k in range(3):
        np.add.at(tan, tri[:, k], t)
        np.add.at(bit, tri[:, k], b)
    tan = tan - nrm * (tan * nrm).sum(axis=1, keepdims=True)
    ln = np.linalg.norm(tan, axis=1, keepdims=True)
    fallback = np.cross(nrm, np.where(np.abs(nrm[:, 2:3]) < 0.9, [[0.0, 0.0, 1.0]], [[1.0, 0.0, 0.0]]))
    tan = np.where(ln > 1e-9, tan / np.maximum(ln, 1e-9), fallback / np.maximum(np.linalg.norm(fallback, axis=1, keepdims=True), 1e-9))
    sign = np.where((np.cross(nrm, tan) * bit).sum(axis=1) < 0, -1.0, 1.0)
    return np.concatenate([tan, sign[:, None]], axis=1)


class Writer:
    def __init__(self):
        self.b = io.BytesIO()

    def u8(self, v): self.b.write(struct.pack('<B', int(v)))
    def u32(self, v): self.b.write(struct.pack('<I', int(v)))
    def i32(self, v): self.b.write(struct.pack('<i', int(v)))
    def f32(self, *v): self.b.write(struct.pack('<%df' % len(v), *[float(x) for x in v]))
    def arr(self, a, dtype): self.b.write(np.ascontiguousarray(a, dtype=dtype).tobytes())

    def s(self, text):
        d = text.encode('utf-8')
        self.b.write(struct.pack('<H', len(d)))
        self.b.write(d)


def pack(glb_path, out_path, meta=None, max_texture=2048, roughness_override=None):
    g = Gltf(glb_path)
    js = g.js
    w = Writer()
    w.b.write(b'SUMD')
    w.u32(VERSION)

    # ---- textures
    tex_slots, blobs = {}, []
    normal_images = set()
    for m in js.get('materials', []):
        if 'normalTexture' in m:
            normal_images.add(js['textures'][m['normalTexture']['index']]['source'])

    def texture(tex_index, kind):
        src = js['textures'][tex_index]['source']
        key = (src, kind)
        if key not in tex_slots:
            img = Image.open(io.BytesIO(g.image_bytes(src)))
            if max(img.size) > max_texture:
                sc = max_texture / max(img.size)
                img = img.resize((round(img.size[0] * sc), round(img.size[1] * sc)), Image.LANCZOS)
            buf = io.BytesIO()
            if kind == 1:
                a = np.asarray(img.convert('RGB')).copy()
                a[..., 1] = 255 - a[..., 1]                  # glTF normal maps are +Y up, Unreal expects +Y down
                Image.fromarray(a, 'RGB').save(buf, 'JPEG', quality=92)
                enc = 1
            elif img.mode in ('RGBA', 'LA', 'P') and np.asarray(img.convert('RGBA'))[..., 3].min() < 250:
                img.convert('RGBA').save(buf, 'PNG')
                enc = 0
            else:
                img.convert('RGB').save(buf, 'JPEG', quality=90)
                enc = 1
            tex_slots[key] = len(blobs)
            blobs.append(('T_%d' % src, kind, enc, img.size, buf.getvalue()))
        return tex_slots[key]
    mats = []
    for m in js.get('materials', []):
        pbr = m.get('pbrMetallicRoughness', {})
        base = texture(pbr['baseColorTexture']['index'], 0) if 'baseColorTexture' in pbr else -1
        nrm = texture(m['normalTexture']['index'], 1) if 'normalTexture' in m else -1
        rough = pbr.get('roughnessFactor', 1.0)
        if 'metallicRoughnessTexture' in pbr:
            rough = roughness_override if roughness_override is not None else 0.7
        mats.append((m.get('name', 'M'), base, nrm, pbr.get('baseColorFactor', [1, 1, 1, 1]), rough,
                     0.0 if 'metallicRoughnessTexture' in pbr else pbr.get('metallicFactor', 1.0),
                     1 if m.get('alphaMode') == 'MASK' else 0, m.get('alphaCutoff', 0.5), 1 if m.get('doubleSided') else 0))
    w.u32(len(blobs))
    for name, kind, enc, size, data in blobs:
        w.s(name); w.u8(kind); w.u8(enc); w.u32(size[0]); w.u32(size[1]); w.u32(len(data)); w.b.write(data)
    w.u32(len(mats))
    for name, base, nrm, col, rough, metal, masked, cutoff, two in mats:
        w.s(name); w.i32(base); w.i32(nrm); w.f32(*col); w.f32(rough, metal); w.u8(masked); w.f32(cutoff); w.u8(two)

    # ---- meshes
    skin = js['skins'][0] if js.get('skins') else None
    mesh_nodes = [n for n in js['nodes'] if 'mesh' in n]
    w.u32(len(mesh_nodes))
    first_skinned = None
    for n in mesh_nodes:
        mesh = js['meshes'][n['mesh']]
        skinned = 'skin' in n
        secs = []
        lo, hi = np.full(3, 1e30), np.full(3, -1e30)
        for p in mesh['primitives']:
            a = p['attributes']
            pos = ue_vec(g.accessor(a['POSITION'])) * 100.0
            nrm = ue_vec(g.accessor(a['NORMAL']))
            nrm /= np.maximum(np.linalg.norm(nrm, axis=1, keepdims=True), 1e-9)
            uv = g.accessor(a['TEXCOORD_0']).astype(np.float64) if 'TEXCOORD_0' in a else np.zeros((len(pos), 2))
            idx = g.accessor(p['indices']).reshape(-1).astype(np.int64)
            sec = dict(mat=p.get('material', -1), pos=pos, nrm=nrm, uv=uv, idx=idx, tan=tangents(pos, nrm, uv, idx))
            if skinned:
                sec['joints'] = g.accessor(a['JOINTS_0']).astype(np.uint8)
                sec['weights'] = g.accessor(a['WEIGHTS_0']).astype(np.float32)
            secs.append(sec)
            lo, hi = np.minimum(lo, pos.min(axis=0)), np.maximum(hi, pos.max(axis=0))
        w.s(mesh.get('name', n.get('name', 'Mesh'))); w.u8(1 if skinned else 0); w.f32(*lo); w.f32(*hi); w.u32(len(secs))
        for sec in secs:
            w.i32(sec['mat']); w.u32(len(sec['pos'])); w.u32(len(sec['idx']))
            w.arr(sec['pos'], '<f4'); w.arr(sec['nrm'], '<f4'); w.arr(sec['tan'], '<f4'); w.arr(sec['uv'], '<f4'); w.arr(sec['idx'], '<u4')
            if skinned:
                w.arr(sec['joints'], np.uint8); w.arr(sec['weights'], '<f4')
        if skinned and first_skinned is None:
            first_skinned = secs

    # ---- skeleton + clips
    checks = []
    if skin:
        joints = skin['joints']
        jn = {node: k for k, node in enumerate(joints)}
        parent = [-1] * len(joints)
        for node in joints:
            for c in js['nodes'][node].get('children', []):
                if c in jn:
                    parent[jn[c]] = jn[node]
        rest_t = np.array([ue_vec(js['nodes'][n].get('translation', [0, 0, 0])) * 100.0 for n in joints])
        rest_q = np.array([ue_quat(js['nodes'][n].get('rotation', [0, 0, 0, 1])) for n in joints])

        def world(lq, lt):
            out = [None] * len(joints)
            for k in range(len(joints)):
                m = np.eye(4)
                m[:3, :3] = q_matrix(lq[k])
                m[:3, 3] = lt[k]
                out[k] = m if parent[k] < 0 else out[parent[k]] @ m
            return out
        assert all(parent[k] < k for k in range(len(joints))), 'joints must be ordered parent first'
        inv_bind = [np.linalg.inv(m) for m in world(rest_q, rest_t)]
        w.u32(len(joints))
        for k, node in enumerate(joints):
            w.s(js['nodes'][node]['name']); w.i32(parent[k]); w.f32(*rest_t[k]); w.f32(*rest_q[k]); w.arr(inv_bind[k][:3, :], '<f4')
        anims = js.get('animations', [])
        w.u32(len(anims))
        loops = (meta or {}).get('_loops', {})
        for ci, an in enumerate(anims):
            times = None
            rot = {k: None for k in range(len(joints))}
            tr = {k: None for k in range(len(joints))}
            for ch in an['channels']:
                sm = an['samplers'][ch['sampler']]
                tt = g.accessor(sm['input']).reshape(-1)
                times = tt if times is None else times
                assert len(tt) == len(times)
                k = jn[ch['target']['node']]
                vals = g.accessor(sm['output']).astype(np.float64)
                if ch['target']['path'] == 'rotation':
                    rot[k] = ue_quat(vals)
                elif ch['target']['path'] == 'translation':
                    tr[k] = ue_vec(vals) * 100.0
            frames = len(times)
            w.s(an['name']); w.f32(float(times[-1])); w.u8(1 if loops.get(an['name'], True) else 0); w.u32(frames)
            full_q = [rot[k] if rot[k] is not None else np.tile(rest_q[k], (frames, 1)) for k in range(len(joints))]
            full_t = [tr[k] if tr[k] is not None else np.tile(rest_t[k], (frames, 1)) for k in range(len(joints))]
            for k in range(len(joints)):
                w.arr(full_q[k], '<f4'); w.arr(full_t[k], '<f4')
            # reference positions for the native test: skin a few vertices at a frame in the middle
            f = frames // 3
            wm = world([full_q[k][f] for k in range(len(joints))], [full_t[k][f] for k in range(len(joints))])
            sk = [wm[k] @ inv_bind[k] for k in range(len(joints))]
            sec = first_skinned[0]
            for v in np.linspace(0, len(sec['pos']) - 1, 6).astype(int):
                p = np.append(sec['pos'][v], 1.0)
                acc = np.zeros(3)
                for j, wt in zip(sec['joints'][v], sec['weights'][v]):
                    acc += wt * (sk[j] @ p)[:3]
                checks.append((ci, float(times[f]), int(v), acc))
    else:
        w.u32(0)
        w.u32(0)
    w.u32(len(checks))
    for ci, t, v, p in checks:
        w.u32(ci); w.f32(t); w.u32(v); w.f32(*p)
    items = [(k, v) for k, v in (meta or {}).items() if not k.startswith('_')]
    w.u32(len(items))
    for k, v in items:
        w.s(k); w.f32(v)
    with open(out_path, 'wb') as f:
        f.write(w.b.getvalue())
    return len(w.b.getvalue())
