"""The tomb at the deepest level, from "Egypt chamber for AR / VR games" by arloopa.

LICENSE: Sketchfab Standard. The model may be used inside the game but its files must not be redistributed,
so the output of this script is written to a folder that is excluded from the public repository. The game
runs without it (the passage to the tomb is then blocked by a cave-in).

Only the underground part is used: stairs, hallway, pillared hall, side rooms and the burial chamber. The dune
on top is dropped; the entrance pit sits inside a cave chamber that build_house.py carves for it.
"""
import io
import os
import numpy as np
from PIL import Image
from gltfio import Gltf
from meshutil import flatten, merge_by_material
from glbout import GlbFile, MaterialSpec, stats
from build_house import TOMB_ENTRY

CREDIT = '"Egypt chamber for AR / VR games" by arloopa (https://sketchfab.com/arloopa), Sketchfab Standard License - do not redistribute'
S = 0.0055
T_IN = np.array([-13.65, 7.4, 33.5])           # start of the plank walkway in scaled model space
SKIP = ('ground near',)
DOOR = ('closed door', 'lines')
CUBES = ('cube1', 'cube2', 'cube3', 'cube4', 'cube5')
CHUNKS = (('SM_Tomb_Entrance', 14.0, 99.0), ('SM_Tomb_Hall', -6.2, 14.0), ('SM_Tomb_Rooms', -99.0, -6.2))   # by model z of the part centre


def to_site(p):
    """Scaled model space -> site coordinates (quarter turn: the tomb runs towards -X)."""
    p = np.asarray(p, dtype=np.float64)
    return np.stack([TOMB_ENTRY[0] + (p[..., 2] - T_IN[2]), TOMB_ENTRY[1] + (p[..., 1] - T_IN[1]), TOMB_ENTRY[2] - (p[..., 0] - T_IN[0])], axis=-1)


def build(src_dir, out_dir):
    g = Gltf(os.path.join(src_dir, 'scene.gltf'))
    js = g.js
    mats, cache = {}, {}
    for i, m in enumerate(js['materials']):
        pbr = m.get('pbrMetallicRoughness', {})
        img = None
        if 'baseColorTexture' in pbr:
            src = js['textures'][pbr['baseColorTexture']['index']]['source']
            if src not in cache:
                cache[src] = Image.open(io.BytesIO(g.image_bytes(src))).convert('RGB')
            img = cache[src]
        mats[i] = MaterialSpec('M_Tomb_%02d' % i, base=img, base_factor=None if img is not None else pbr.get('baseColorFactor'), roughness=0.92, max_size=1024,
                               double_sided=bool(m.get('doubleSided')))
    out = GlbFile(mats, CREDIT)
    manifest, pts = {}, {'anchors': {}, 'lights': [], 'cubes': []}
    groups = {name: [] for name, _, _ in CHUNKS}
    door, cubes = [], {c: [] for c in CUBES}
    for p in flatten(g):
        name = (p.name or '').split('_')[0].strip()
        if name in SKIP:
            continue
        model = p.pos * S
        p.pos = to_site(model)
        n = p.nrm
        p.nrm = np.stack([n[:, 2], n[:, 1], -n[:, 0]], axis=1)
        if name in DOOR:
            door.append(p)
        elif name in cubes:
            cubes[name].append(p)
        else:
            cz = model[:, 2].mean()
            for cname, z0, z1 in CHUNKS:
                if z0 <= cz < z1:
                    groups[cname].append(p)
            if name.startswith('luys'):
                c = p.pos.mean(axis=0)
                pts['lights'].append(dict(id='Tomb%d' % len(pts['lights']), pos=(float(c[0]), float(c[1]) - 0.25, float(c[2])), radius=9.0, intensity=0.0,
                                          color=(1.0, 0.74, 0.42), kind='none', flicker=1, powered=0))

    def add(name, parts, pivot=None):
        parts = merge_by_material(parts)
        if pivot is not None:
            for q in parts:
                q.translate(-np.asarray(pivot))
        out.add_mesh(name, parts)
        manifest[name] = dict(stats(parts), kind='static', file='Tomb.glb', group='tomb')
    for name, parts in groups.items():
        add(name, parts)
    lo = np.min([p.pos.min(axis=0) for p in door], axis=0)
    hi = np.max([p.pos.max(axis=0) for p in door], axis=0)
    door_pivot = np.array([(lo[0] + hi[0]) / 2, lo[1], (lo[2] + hi[2]) / 2])
    add('SM_Tomb_Door', door, door_pivot)
    for c in CUBES:
        ctr = np.mean([p.pos.mean(axis=0) for p in cubes[c]], axis=0)
        add('SM_Tomb_' + c.capitalize(), cubes[c], ctr)
        pts['cubes'].append(tuple(float(v) for v in ctr))
    out.save(os.path.join(out_dir, 'Tomb.glb'))
    m = lambda x, y, z: tuple(float(v) for v in to_site(np.array([x, y, z])))
    pts['anchors'].update({
        'TombWalkway': m(-13.65, 7.45, 31.0), 'TombStairsTop': m(-13.65, 7.4, 27.2), 'TombStairsBottom': m(-13.65, 3.05, 19.0), 'TombHallway': m(-13.4, 3.0, 13.0),
        'TombHall': m(-14.0, 3.15, 1.0), 'TombAnte': m(-13.9, 3.05, -9.3), 'TombDoor': tuple(float(v) for v in door_pivot), 'TombChamber': m(-14.1, 3.05, -13.0),
        'Sarcophagus': m(-14.1, 4.25, -16.0), 'TombLeftRoom': m(-22.8, 3.05, -9.3), 'TombRightRoom': m(-5.0, 3.05, -9.3),
    })
    pts['door'] = dict(pos=tuple(float(v) for v in door_pivot), size=tuple(float(v) for v in (hi - lo)))
    pts['nav'] = [m(-13.65, 7.4, 30.0), m(-13.65, 7.4, 27.4), m(-13.65, 3.05, 19.3), m(-13.4, 3.0, 13.0), m(-14.0, 3.15, 5.0), m(-14.0, 3.15, -2.0)]
    pts['zone'] = dict(lo=tuple(float(v) for v in np.min([to_site(np.array(c)) for c in ((-25.5, 2.5, -19.0), (-2.0, 9.0, 28.0))], axis=0)),
                       hi=tuple(float(v) for v in np.max([to_site(np.array(c)) for c in ((-25.5, 2.5, -19.0), (-2.0, 9.0, 28.0))], axis=0)))
    return manifest, pts


if __name__ == '__main__':
    import sys
    import json
    mf, p = build(sys.argv[1], sys.argv[2])
    print(json.dumps({k: (v['tris'], v['size_m']) for k, v in mf.items()}), json.dumps(p['anchors']))
