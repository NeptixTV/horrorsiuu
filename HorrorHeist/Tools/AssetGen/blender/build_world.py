"""
Builds the whole world of The Quiet Job in one Blender session and exports it for Unreal:

  L_Hideout       persistent hub map: the basement hideout (+ clock, environment); streams L_Town
  L_Town          the town of Hollowmere (streamed, always loaded): streets, shops, houses, the
                  three job houses with their rooms, furniture, doors and lights
  L_Job_Vance     first job (playable): night clock, van, crew starts, loot, containers, spare key,
                  the haunting; streams L_Town
  L_Job_Marrow    prepared job map (no loot yet); streams L_Town
  L_Job_Rectory   prepared job map (no loot yet); streams L_Town

Output: SourceArt/Meshes/**.fbx + meshes.json, SourceArt/Layout/<level>.json, optional previews.

  python3 Tools/AssetGen/blender/build_world.py [--render] [--fast] [--no-export] [--save]

Everything is deterministic (fixed seeds), so re-running produces the same files; the Unreal
setup replaces the generated actors of each map instead of adding duplicates.
"""
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

import hhblend as H  # noqa: E402
import build_hideout as BH  # noqa: E402
import items  # noqa: E402,F401  (equipment props)
import props_world  # noqa: E402,F401  (town, house and loot props)
import worldkit  # noqa: E402,F401  (doors, glow cards, keys)
import placement as PL  # noqa: E402
import town as T  # noqa: E402
import vance as V  # noqa: E402
import prepared as PR  # noqa: E402
import worldkit as K  # noqa: E402

LAYOUT_DIR = os.path.join(H.PROJECT, 'SourceArt', 'Layout')
MANIFEST_PATH = os.path.join(H.MESH_DIR, 'meshes.json')
LEVELS = {}


def level(name, **meta):
    LEVELS[name] = dict(level=name, units='cm', actors=[], **meta)
    BH.ACTORS = LEVELS[name]['actors']


def environment(**overrides):
    """HHEnvironment pseudo-actor: height fog + post process volume (see hh_setup/level.py)."""
    p = dict(fog_density=0.02, fog_height_falloff=0.2, fog_color=[0.05, 0.06, 0.075], volumetric_albedo=[0.85] * 3,
             volumetric_extinction=1.0, exposure_min=-0.5, exposure_max=6.0, exposure_bias=0.2, bloom=0.5,
             vignette=0.4, grain=0.1, contrast=1.05, saturation=0.9, shadows_tint=[0.92, 0.97, 1.04],
             highlights_tint=[1.04, 1.0, 0.95], lens_dirt='T_LensDirt', sky_light_intensity=1.0)
    p.update(overrides)
    BH.ACTORS.append(dict(name='LevelEnvironment', cls='HHEnvironment', folder='Environment', location=[0, 0, 0],
                          quat=[0, 0, 0, 1], scale=[1, 1, 1], properties=p))


def time_of_day(night=False, speed=1.0, dark_houses=(), **props):
    p = {'bNightMap': bool(night), 'MapSpeed': float(speed), 'DarkHouseTags': list(dark_houses)}
    p.update(props)
    BH.actor('HHTimeOfDay', K.mat((0, 40, 30)), 'Environment', properties=p, name='TimeOfDay')


def hideout_plug():
    """Job maps stream the town without the hideout: close what would look into the empty basement."""
    plug = H.Builder('SM_Town_HideoutPlug')
    plug.box_minmax((-5.4, 3.9, -0.3), (-1.8, 4.95, 3.0), 'MI_Concrete_Raw')
    plug.box_minmax((0.8, 4.4, 2.3), (2.2, 4.95, 3.3), 'MI_Concrete_Raw')
    plug.box_minmax((-7.0, 2.3, 2.3), (-6.5, 3.7, 3.3), 'MI_Concrete_Raw')
    plug.box_minmax((5.9, 4.4, 2.0), (6.9, 4.95, 4.4), 'MI_Concrete_Raw')
    PL.place(PL.unique(plug, collision='complex'), collection='Plug')
    PL.place('SM_Door_Steel', (5.95, 5.125, 2.2), collection='Plug')


# =======================================================================================
# Export

def mesh_hash(me):
    """Content hash of a mesh (geometry, UVs, material slots), independent of face order (some
    bmesh operators emit faces in a different order each run). FBX files embed timestamps and
    random object ids, so the files themselves can't be compared between runs."""
    import hashlib
    uv = me.uv_layers[0].data if me.uv_layers else None
    faces = []
    for poly in me.polygons:
        corners = []
        for li in poly.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            u = uv[li].uv if uv else (0.0, 0.0)
            corners.append('%.4f,%.4f,%.4f/%.4f,%.4f' % (co.x + 0.0, co.y + 0.0, co.z + 0.0, u[0] + 0.0, u[1] + 0.0))
        k = corners.index(min(corners))
        corners = corners[k:] + corners[:k]
        faces.append(me.materials[poly.material_index].name + ':' + ';'.join(corners))
    faces.sort()
    h = hashlib.sha1('|'.join(m.name for m in me.materials).encode())
    for f in faces:
        h.update(f.encode())
    return h.hexdigest()


def _nearly_same_file(a, b, limit=600):
    """Fallback for entries without a hash yet: same size and only a few bytes (timestamps,
    object ids) differ."""
    if len(a) != len(b):
        return False
    diff = sum(1 for x, y in zip(a, b) if x != y)
    return diff <= limit


def export_if_changed(obj, path, digest, previous):
    """Export unless the mesh is unchanged since the last export (same hash, file present)."""
    if previous and previous.get('hash') == digest and os.path.exists(path):
        return False
    tmp = path + '.tmp.fbx'
    H.export_mesh_object(obj, tmp)
    if not (previous or {}).get('hash') and os.path.exists(path):
        with open(tmp, 'rb') as fh:
            new = fh.read()
        with open(path, 'rb') as fh:
            old = fh.read()
        if _nearly_same_file(old, new):
            os.remove(tmp)
            return False
    os.replace(tmp, path)
    return True


def referenced_props(levels):
    """Props used only as component meshes (lever, haunted screen...) still need exporting."""
    import props as P
    found = set()

    def walk(v):
        if isinstance(v, dict):
            if 'asset' in v and v['asset'] in P.PROPS:
                found.add(v['asset'])
            for x in v.values():
                walk(x)
        elif isinstance(v, list):
            for x in v:
                walk(x)
    for data in levels.values():
        walk(data['actors'])
    for name in sorted(found):
        if name not in BH.MESHES or not any(o.data and o.data.name == name for o in bpy.data.objects
                                            if o.type == 'MESH'):
            obj = BH.obj_from_mesh(name, K.mat((0, 0, -50)), 'ComponentMeshes')
            obj.hide_render = True


def export(levels):
    referenced_props(levels)
    manifest = H.MeshManifest(MANIFEST_PATH)
    first_obj = {}
    for obj in bpy.context.scene.objects:
        if obj.type == 'MESH' and obj.data.name in BH.MESHES and obj.data.name not in first_obj:
            first_obj[obj.data.name] = obj
    count = 0
    for name, (me, folder, collision, nanite) in sorted(BH.MESHES.items()):
        obj = first_obj.get(name)
        if obj is None:
            continue
        rel = os.path.join('SourceArt', 'Meshes', folder, name + '.fbx')
        digest = mesh_hash(me)
        if export_if_changed(obj, os.path.join(H.PROJECT, rel), digest, manifest.data.get(name)):
            count += 1
        manifest.add(name, rel.replace(os.sep, '/'), [m.name for m in me.materials], folder, collision, nanite,
                     extra={'hash': digest})
    manifest.save()
    os.makedirs(LAYOUT_DIR, exist_ok=True)
    for name, data in levels.items():
        with open(os.path.join(LAYOUT_DIR, name + '.json'), 'w', encoding='utf-8') as fh:
            json.dump(data, fh, indent=1)
        print(f'layout {name}: {len(data["actors"])} actors', flush=True)
    print(f'exported {count} changed meshes ({len(BH.MESHES)} in the world)', flush=True)


# =======================================================================================
# Preview renders (Cycles): a map of the town and a few street views

def _camera(name, loc, target, fov=60.0, ortho=None):
    cam_data = bpy.data.cameras.new(name)
    if ortho:
        cam_data.type = 'ORTHO'
        cam_data.ortho_scale = ortho
    else:
        cam_data.angle = math.radians(fov)
    cam_data.clip_end = 1000.0
    cam = bpy.data.objects.new(name, cam_data)
    cam.matrix_world = BH.look_at_matrix(loc, target)
    H.link(cam, H.collection('Cameras'))
    return cam


def render(fast=False):
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'CPU'
    scene.cycles.samples = 12 if fast else 48
    scene.cycles.use_denoising = True
    scene.cycles.max_bounces = 4
    world = bpy.data.worlds.new('World')
    world.color = (0.35, 0.42, 0.5)
    scene.world = world
    sun = bpy.data.lights.new('Sun', 'SUN')
    sun.energy = 2.5
    sun.angle = math.radians(3)
    so = bpy.data.objects.new('Sun', sun)
    so.rotation_euler = (math.radians(50), 0, math.radians(35))
    H.link(so)
    for col in ('PreviewLights',):
        if col in bpy.data.collections:
            bpy.data.collections[col].hide_render = True
    # A flashlight at the camera for interior shots (the houses are dark at night, like in the game).
    tl = bpy.data.lights.new('Torch', 'SPOT')
    tl.energy = 140.0
    tl.spot_size = math.radians(75)
    tl.spot_blend = 0.6
    tl.color = (1.0, 0.92, 0.8)
    torch = bpy.data.objects.new('Torch', tl)
    H.link(torch)
    try:
        scene.view_settings.view_transform = 'AgX'
    except TypeError:
        pass
    out = os.path.join(H.PROJECT, 'SourceArt', 'Previews')
    os.makedirs(out, exist_ok=True)
    shots = [
        ('Town_Map', (0, 64, 200), (0, 64, 0), 60, 185.0, (1400, 1400)),
        ('Town_MainStreet', (-24, 18.5, 4.2), (10, 22, 4.0), 62, None, (1280, 720)),
        ('Town_LaundromatLot', (8.5, 18.0, 4.1), (0, 4.0, 5.0), 62, None, (1280, 720)),
        ('Town_Vance', (49.0, 46.5, 4.2), (62, 57, 4.8), 60, None, (1280, 720)),
        ('Town_Ashgrove', (48.0, 28.0, 5.0), (48, 70, 4.0), 55, None, (1280, 720)),
        ('Town_Museum', (6.0, 19.0, 4.0), (6.0, 34.0, 5.5), 62, None, (1280, 720)),
        ('Town_Church', (-12.0, 101.0, 4.0), (-20.0, 120.0, 9.0), 62, None, (1280, 720)),
        ('Town_Marrow', (-46.0, 72.0, 4.2), (-62.0, 84.0, 6.0), 60, None, (1280, 720)),
        ('Vance_Living', V.L(-1.4, 4.9, V.G + 1.6), V.L(-4.5, 1.5, V.G + 0.8), 75, None, (1280, 720)),
        ('Vance_Kitchen', V.L(1.5, 4.4, V.G + 1.6), V.L(4.8, 8.0, V.G + 0.9), 75, None, (1280, 720)),
        ('Vance_Master', V.L(-1.5, 4.9, V.U + 1.6), V.L(-4.6, 1.0, V.U + 0.8), 75, None, (1280, 720)),
        ('Vance_Hall', V.L(-0.5, 0.6, V.G + 1.6), V.L(-0.4, 6.0, V.G + 1.4), 75, None, (1280, 720)),
        ('Vance_Basement', V.L(0.6, 1.2, V.B + 1.6), V.L(-3.5, 7.0, V.B + 0.6), 75, None, (1280, 720)),
    ]
    only = [a for a in sys.argv if a.startswith('--shot=')]
    if only:
        names = only[0][7:].split(',')
        shots = [s for s in shots if s[0] in names]
    for name, loc, target, fov, ortho, res in shots:
        cam = _camera('CAM_' + name, loc, target, fov, ortho)
        scene.camera = cam
        scene.render.resolution_x = res[0] if not fast else res[0] // 2
        scene.render.resolution_y = res[1] if not fast else res[1] // 2
        interior = name.startswith('Vance_')
        so.hide_render = interior
        torch.hide_render = not interior
        torch.matrix_world = cam.matrix_world
        if interior:
            world.color = (0.02, 0.02, 0.025)
            for col in ('PreviewLights',):
                if col in bpy.data.collections:
                    bpy.data.collections[col].hide_render = False
        scene.render.filepath = os.path.join(out, name + '.jpg')
        scene.render.image_settings.file_format = 'JPEG'
        bpy.ops.render.render(write_still=True)
        print('rendered', name, flush=True)


# =======================================================================================

def main():
    H.reset()
    # --- Hub ---------------------------------------------------------------------------------
    level('L_Hideout', kind='persistent', streaming=['L_Town'])
    BH.build_all()
    time_of_day(night=False, speed=1.0, DayFogDensity=0.016, NightFogDensity=0.022,
                NightFogColor=[0.03, 0.035, 0.045])
    # --- The town ----------------------------------------------------------------------------
    level('L_Town', kind='sublevel')
    T.build()
    V.build()
    PR.build()
    # --- Jobs ----------------------------------------------------------------------------------
    level('L_Job_Vance', kind='persistent', streaming=['L_Town'], game_mode='HHHeistGameMode')
    V.job_layout()
    hideout_plug()
    environment()
    time_of_day(night=True, speed=0.35, dark_houses=[T.HOUSE_TAGS['vance']])
    level('L_Job_Marrow', kind='persistent', streaming=['L_Town'], game_mode='HHHeistGameMode')
    PR.marrow_job()
    hideout_plug()
    environment()
    time_of_day(night=True, speed=0.35, dark_houses=[T.HOUSE_TAGS['marrow']])
    level('L_Job_Rectory', kind='persistent', streaming=['L_Town'], game_mode='HHHeistGameMode')
    PR.rectory_job()
    hideout_plug()
    environment()
    time_of_day(night=True, speed=0.35, dark_houses=[T.HOUSE_TAGS['rectory']])

    if PL.MISSING:
        print('WARNING: missing meshes:', sorted(PL.MISSING), flush=True)
    if '--no-export' not in sys.argv:
        export(LEVELS)
    if '--save' in sys.argv:
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(LAYOUT_DIR, 'World.blend'))
    if '--render' in sys.argv:
        render(fast='--fast' in sys.argv)


if __name__ == '__main__':
    main()
