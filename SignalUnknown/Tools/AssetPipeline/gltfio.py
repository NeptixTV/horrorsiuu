"""Minimal glTF 2.0 reader/writer helpers (numpy). Reads .gltf+.bin and .glb, writes .glb."""
import json, struct, os, base64
import numpy as np

CT = {5120: np.int8, 5121: np.uint8, 5122: np.int16, 5123: np.uint16, 5125: np.uint32, 5126: np.float32}
NC = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}


class Gltf:
    def __init__(self, path):
        self.path = path
        if path.endswith('.glb'):
            data = open(path, 'rb').read()
            _, _, length = struct.unpack_from('<III', data, 0)
            off = 12
            self.buffers = []
            while off < length:
                clen, ctype = struct.unpack_from('<II', data, off)
                off += 8
                chunk = data[off:off + clen]
                off += clen
                if ctype == 0x4E4F534A:
                    self.js = json.loads(chunk)
                elif ctype == 0x004E4942:
                    self.buffers.append(chunk)
        else:
            self.js = json.load(open(path))
            self.buffers = []
            for b in self.js.get('buffers', []):
                uri = b.get('uri')
                if uri.startswith('data:'):
                    self.buffers.append(base64.b64decode(uri.split(',', 1)[1]))
                else:
                    self.buffers.append(open(os.path.join(os.path.dirname(path), uri), 'rb').read())

    def accessor(self, index):
        a = self.js['accessors'][index]
        bv = self.js['bufferViews'][a['bufferView']]
        dt = CT[a['componentType']]
        n = NC[a['type']]
        off = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
        stride = bv.get('byteStride', 0)
        buf = self.buffers[bv['buffer']]
        itemsize = np.dtype(dt).itemsize * n
        if stride and stride != itemsize:
            raw = np.frombuffer(buf, dtype=np.uint8, count=stride * (a['count'] - 1) + itemsize, offset=off)
            idx = (np.arange(a['count'])[:, None] * stride + np.arange(itemsize)[None, :]).reshape(-1)
            arr = np.frombuffer(raw[idx].tobytes(), dtype=dt).reshape(a['count'], n)
        else:
            arr = np.frombuffer(buf, dtype=dt, count=a['count'] * n, offset=off).reshape(a['count'], n)
        if a.get('normalized'):
            arr = arr.astype(np.float32) / float(np.iinfo(dt).max)
        return arr.copy()

    def image_bytes(self, index):
        im = self.js['images'][index]
        if 'uri' in im:
            if im['uri'].startswith('data:'):
                return base64.b64decode(im['uri'].split(',', 1)[1])
            return open(os.path.join(os.path.dirname(self.path), im['uri'].replace('%20', ' ')), 'rb').read()
        bv = self.js['bufferViews'][im['bufferView']]
        return self.buffers[bv['buffer']][bv.get('byteOffset', 0):bv.get('byteOffset', 0) + bv['byteLength']]

    def node_local(self, i):
        n = self.js['nodes'][i]
        if 'matrix' in n:
            return np.array(n['matrix'], dtype=np.float64).reshape(4, 4).T
        return trs_matrix(n.get('translation', [0, 0, 0]), n.get('rotation', [0, 0, 0, 1]), n.get('scale', [1, 1, 1]))

    def world_matrices(self):
        nodes = self.js['nodes']
        world = [None] * len(nodes)
        parent = [-1] * len(nodes)
        for i, n in enumerate(nodes):
            for c in n.get('children', []):
                parent[c] = i

        def get(i):
            if world[i] is None:
                world[i] = self.node_local(i) if parent[i] < 0 else get(parent[i]) @ self.node_local(i)
            return world[i]
        for i in range(len(nodes)):
            get(i)
        return world, parent


def quat_matrix(q):
    x, y, z, w = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]], dtype=np.float64)


def trs_matrix(t, q, s):
    m = np.eye(4)
    m[:3, :3] = quat_matrix(q) * np.array(s, dtype=np.float64)[None, :]
    m[:3, 3] = t
    return m


class GlbWriter:
    """Builds a single-buffer GLB."""

    def __init__(self, generator='SignalUnknown asset pipeline'):
        self.js = {'asset': {'version': '2.0', 'generator': generator}, 'scene': 0, 'scenes': [{'nodes': []}], 'nodes': [], 'meshes': [],
                   'accessors': [], 'bufferViews': [], 'buffers': [{}], 'materials': [], 'textures': [], 'images': [], 'samplers': [{'magFilter': 9729, 'minFilter': 9987, 'wrapS': 10497, 'wrapT': 10497}]}
        self.blob = bytearray()

    def _view(self, data, target=None):
        while len(self.blob) % 4:
            self.blob.append(0)
        off = len(self.blob)
        self.blob += data
        bv = {'buffer': 0, 'byteOffset': off, 'byteLength': len(data)}
        if target:
            bv['target'] = target
        self.js['bufferViews'].append(bv)
        return len(self.js['bufferViews']) - 1

    def accessor(self, arr, kind, target=None, minmax=False, normalized=False):
        arr = np.ascontiguousarray(arr)
        ct = {np.dtype(np.float32): 5126, np.dtype(np.uint32): 5125, np.dtype(np.uint16): 5123, np.dtype(np.uint8): 5121}[arr.dtype]
        a = {'bufferView': self._view(arr.tobytes(), target), 'componentType': ct, 'count': int(arr.shape[0]), 'type': kind}
        if normalized:
            a['normalized'] = True
        if minmax:
            flat = arr.reshape(arr.shape[0], -1)
            a['min'] = [float(v) for v in flat.min(axis=0)]
            a['max'] = [float(v) for v in flat.max(axis=0)]
        self.js['accessors'].append(a)
        return len(self.js['accessors']) - 1

    def image(self, data, mime, name=None):
        im = {'bufferView': self._view(data), 'mimeType': mime}
        if name:
            im['name'] = name
        self.js['images'].append(im)
        self.js['textures'].append({'source': len(self.js['images']) - 1, 'sampler': 0})
        return len(self.js['textures']) - 1

    def material(self, name, base_tex=None, normal_tex=None, orm_tex=None, base_factor=None, metallic=0.0, roughness=0.9,
                 alpha_mode=None, alpha_cutoff=0.5, double_sided=False, emissive=None, emissive_tex=None):
        pbr = {'metallicFactor': float(metallic), 'roughnessFactor': float(roughness)}
        if base_tex is not None:
            pbr['baseColorTexture'] = {'index': base_tex}
        if base_factor is not None:
            pbr['baseColorFactor'] = [float(v) for v in base_factor]
        if orm_tex is not None:
            pbr['metallicRoughnessTexture'] = {'index': orm_tex}
        m = {'name': name, 'pbrMetallicRoughness': pbr}
        if normal_tex is not None:
            m['normalTexture'] = {'index': normal_tex}
        if orm_tex is not None:
            m['occlusionTexture'] = {'index': orm_tex}
        if alpha_mode:
            m['alphaMode'] = alpha_mode
            if alpha_mode == 'MASK':
                m['alphaCutoff'] = float(alpha_cutoff)
        if double_sided:
            m['doubleSided'] = True
        if emissive is not None:
            m['emissiveFactor'] = [float(v) for v in emissive]
        if emissive_tex is not None:
            m['emissiveTexture'] = {'index': emissive_tex}
        self.js['materials'].append(m)
        return len(self.js['materials']) - 1

    def primitive(self, pos, nrm, uv, idx, material, extra=None):
        """One triangle list. Arrays are converted to the glTF types; returns the primitive dict."""
        pos = np.asarray(pos, dtype=np.float32)
        attrs = {'POSITION': self.accessor(pos, 'VEC3', 34962, minmax=True)}
        if nrm is not None:
            attrs['NORMAL'] = self.accessor(np.asarray(nrm, dtype=np.float32), 'VEC3', 34962)
        if uv is not None:
            attrs['TEXCOORD_0'] = self.accessor(np.asarray(uv, dtype=np.float32), 'VEC2', 34962)
        for k, (arr, kind, norm) in (extra or {}).items():
            attrs[k] = self.accessor(arr, kind, 34962, normalized=norm)
        idx = np.asarray(idx).reshape(-1)
        idx = idx.astype(np.uint16) if len(pos) < 65535 else idx.astype(np.uint32)
        p = {'attributes': attrs, 'indices': self.accessor(idx, 'SCALAR', 34963), 'mode': 4}
        if material is not None:
            p['material'] = material
        return p

    def mesh(self, name, primitives):
        self.js['meshes'].append({'name': name, 'primitives': primitives})
        return len(self.js['meshes']) - 1

    def node(self, name, mesh=None, translation=None, rotation=None, scale=None, children=None, root=True, skin=None):
        n = {'name': name}
        if mesh is not None:
            n['mesh'] = mesh
        if skin is not None:
            n['skin'] = skin
        if translation is not None:
            n['translation'] = [float(v) for v in translation]
        if rotation is not None:
            n['rotation'] = [float(v) for v in rotation]
        if scale is not None:
            n['scale'] = [float(v) for v in scale]
        if children:
            n['children'] = list(children)
        self.js['nodes'].append(n)
        i = len(self.js['nodes']) - 1
        if root:
            self.js['scenes'][0]['nodes'].append(i)
        return i

    def save(self, path):
        js = dict(self.js)
        for k in ('materials', 'textures', 'images', 'meshes', 'skins', 'animations'):
            if k in js and not js[k]:
                del js[k]
        if 'textures' not in js:
            js.pop('samplers', None)
        while len(self.blob) % 4:
            self.blob.append(0)
        js['buffers'] = [{'byteLength': len(self.blob)}]
        jb = json.dumps(js, separators=(',', ':')).encode('utf-8')
        while len(jb) % 4:
            jb += b' '
        total = 12 + 8 + len(jb) + 8 + len(self.blob)
        with open(path, 'wb') as f:
            f.write(struct.pack('<III', 0x46546C67, 2, total))
            f.write(struct.pack('<II', len(jb), 0x4E4F534A))
            f.write(jb)
            f.write(struct.pack('<II', len(self.blob), 0x004E4942))
            f.write(self.blob)
