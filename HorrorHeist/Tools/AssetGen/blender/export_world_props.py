"""
Exports the props_world.py meshes to FBX for Unreal:

  python3 Tools/AssetGen/blender/export_world_props.py [--group loot|house|town|nature|yard] [SM_Name ...]
      [--merge-manifest]

  -> SourceArt/Meshes/<folder>/<SM_Name>.fbx
  -> SourceArt/Meshes/meshes_world.json   (manifest entries in the meshes.json format)

meshes.json (read by Content/Python/hh_setup/importers.py) is only updated with --merge-manifest,
so the world props can be reviewed before they are imported.
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402,F401

import hhblend as H  # noqa: E402
import props as P  # noqa: E402
import props_world as W  # noqa: E402
import props_town2  # noqa: E402,F401
from hhmaterials import CATALOG  # noqa: E402


def main():
    group = sys.argv[sys.argv.index('--group') + 1] if '--group' in sys.argv else 'all'
    names = [a for a in sys.argv[1:] if a.startswith('SM_')] or W.names(group)
    H.reset()
    side_path = os.path.join(H.MESH_DIR, 'meshes_world.json')
    side = {}
    if os.path.exists(side_path):
        with open(side_path, 'r', encoding='utf-8') as fh:
            side = json.load(fh)
    manifest = H.MeshManifest(os.path.join(H.MESH_DIR, 'meshes.json')) if '--merge-manifest' in sys.argv else None
    for name in names:
        spec = P.PROPS[name]
        b = P.build(name)
        missing = [m for m in b.mats if m not in CATALOG]
        if missing:
            raise SystemExit(f'{name}: material slots not in the catalogue: {missing}')
        obj = H.link(b.object())
        rel = '/'.join(('SourceArt', 'Meshes', spec['folder'], name + '.fbx'))
        H.export_mesh_object(obj, os.path.join(H.PROJECT, rel))
        mats = [m.name for m in obj.data.materials]
        side[name] = dict(file=rel, materials=mats, folder=spec['folder'], collision=spec['collision'],
                          nanite=spec['nanite'], kind='static')
        if manifest is not None:
            manifest.add(name, rel, mats, spec['folder'], spec['collision'], spec['nanite'])
        bpy.data.objects.remove(obj)
        bpy.context.view_layer.update()
        print('exported', name, flush=True)
    with open(side_path, 'w', encoding='utf-8') as fh:
        json.dump(side, fh, indent=1, sort_keys=True)
    if manifest is not None:
        manifest.save()
    print(f'{len(names)} meshes -> {side_path}' + (' (+ meshes.json)' if manifest is not None else ''))


if __name__ == '__main__':
    main()
