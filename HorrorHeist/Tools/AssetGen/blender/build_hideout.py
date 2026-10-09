"""
Builds the hideout ("the basement under Brannigan & Sons") in Blender, then
  * exports every mesh it uses to SourceArt/Meshes/<Folder>/<Name>.fbx (+ meshes.json),
  * writes SourceArt/Layout/L_Hideout.json (actors in Unreal coordinates, cm),
  * optionally renders preview shots from the station cameras (--render).

  python Tools/AssetGen/blender/build_hideout.py [--render] [--no-export] [--fast]

Floor plan (Blender metres, +Y = north):
  garage interior x -7..7, y -5..5, ceiling 3.4
  north wall: roll-up door (van bay), basement window above the lounge, stair door (top landing)
  west wall: job board + desk, high window behind the van
  south wall: boiler-room door, workbench & pegboard, lockers, mirror corner, the fence's cage
  east wall: stairs up (north half), security desk (south half)
"""
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402
from mathutils import Euler, Matrix, Vector  # noqa: E402

import hhblend as H  # noqa: E402
import items  # noqa: E402,F401  (registers the equipment meshes as props)
import props as P  # noqa: E402
from hhblend import Builder  # noqa: E402

LAYOUT_PATH = os.path.join(H.PROJECT, 'SourceArt', 'Layout', 'L_Hideout.json')
MANIFEST_PATH = os.path.join(H.MESH_DIR, 'meshes.json')

ACTORS = []          # layout entries
MESHES = {}          # name -> (mesh datablock, folder, collision, nanite)


def asset(name):
    return {'asset': name}


def enum(enum_name, value):
    return {'enum': enum_name, 'value': value}


def text(value):
    return {'text': value}


# =======================================================================================
# Placement helpers

def mat(loc=(0, 0, 0), rot=(0, 0, 0), scale=1.0):
    if not isinstance(rot, (tuple, list)):
        rot = (0, 0, rot)
    s = scale if isinstance(scale, (tuple, list)) else (scale, scale, scale)
    return (Matrix.Translation(loc) @ Euler([math.radians(a) for a in rot], 'XYZ').to_matrix().to_4x4()
            @ Matrix.Diagonal((*s, 1.0)))


def get_mesh(name):
    if name not in MESHES:
        info = P.PROPS[name]
        MESHES[name] = (P.build(name).mesh(), info['folder'], info['collision'], info['nanite'])
    return MESHES[name][0]


def register_unique(builder, folder='Hideout', collision='complex', nanite=False):
    MESHES[builder.name] = (builder.mesh(), folder, collision, nanite)
    return builder.name


_counter = {}


def _unique_name(base):
    _counter[base] = _counter.get(base, 0) + 1
    return f'{base}_{_counter[base]}'


def obj_from_mesh(name, matrix, collection):
    me = MESHES[name][0] if name in MESHES else get_mesh(name)
    obj = H.mesh_object(me, name=_unique_name(name))
    obj.matrix_world = matrix
    H.link(obj, H.collection(collection))
    return obj


def static(name, loc=(0, 0, 0), rot=(0, 0, 0), scale=1.0, collection='Props', tags=None, parent=None, shadow=True):
    """A plain StaticMeshActor."""
    m = mat(loc, rot, scale)
    if parent is not None:
        m = parent @ m
    obj = obj_from_mesh(name, m, collection)
    entry = dict(name=obj.name, cls='StaticMeshActor', folder=collection, mesh=name, **H.ue_transform(obj))
    if tags:
        entry['tags'] = tags
    if not shadow:
        entry['cast_shadow'] = False
    ACTORS.append(entry)
    return obj


def empty(name, matrix, collection, size=0.2, display='ARROWS'):
    obj = bpy.data.objects.new(_unique_name(name), None)
    obj.empty_display_type = display
    obj.empty_display_size = size
    obj.matrix_world = matrix
    H.link(obj, H.collection(collection))
    return obj


def actor(cls, matrix, collection='Gameplay', mesh=None, mesh_component='Mesh', properties=None, components=None,
          tags=None, name=None, camera_like=False):
    """A gameplay actor of one of our C++ classes. 'mesh' is shown in Blender for previews."""
    if mesh:
        obj = obj_from_mesh(mesh, matrix, collection)
    else:
        obj = empty(name or cls, matrix, collection)
    comps = dict(components or {})
    if mesh:
        comps.setdefault(mesh_component, {})['StaticMesh'] = asset(mesh)
    entry = dict(name=name or obj.name, cls=cls, folder=collection, **H.ue_transform(obj, camera_like))
    if properties:
        entry['properties'] = properties
    if comps:
        entry['components'] = comps
    if tags:
        entry['tags'] = tags
    ACTORS.append(entry)
    return obj, entry


def look_at_matrix(loc, target, roll=0.0):
    loc = Vector(loc)
    q = (Vector(target) - loc).to_track_quat('-Z', 'Y')
    m = q.to_matrix().to_4x4()
    m.translation = loc
    if roll:
        m = m @ Matrix.Rotation(math.radians(roll), 4, 'Z')
    return m


CAMERAS = {}


def camera(name, loc, target, fov=60.0):
    """A Blender camera (for previews) - returns the Unreal world transform + FOV."""
    cam_data = bpy.data.cameras.new(name)
    cam_data.sensor_fit = 'HORIZONTAL'
    cam_data.angle = math.radians(fov)
    cam_data.clip_start = 0.05
    cam = bpy.data.objects.new(name, cam_data)
    cam.matrix_world = look_at_matrix(loc, target)
    H.link(cam, H.collection('Cameras'))
    CAMERAS[name] = cam
    t = H.ue_transform(cam, camera_like=True)
    return dict(world=dict(location=t['location'], quat=t['quat']), FieldOfView=fov)


def kelvin_rgb(k):
    """Approximate black-body colour (for Blender previews)."""
    t = k / 100.0
    r = 255 if t <= 66 else 329.7 * (t - 60) ** -0.1332
    g = 99.47 * math.log(t) - 161.12 if t <= 66 else 288.1 * (t - 60) ** -0.0755
    b = 255 if t >= 66 else (0 if t <= 19 else 138.5 * math.log(t - 10) - 305.04)
    return tuple(max(0.0, min(1.0, c / 255.0)) for c in (r, g, b))


PREVIEW_LIGHTS = []


def practical(kind, fixture, loc, rot=(0, 0, 0), light_offset=(0, 0, 0), light_dir=(0, 0, -1), intensity=200.0,
              temperature=2700, radius=900.0, flicker='Steady', strength=1.0, cone=(30.0, 60.0), source=3.0,
              rect=(30.0, 30.0), hum=None, tags=None, emissive=None, color=None, shadows=True, collection='Lights',
              volumetric=1.0, name=None, fixture_shadow=False):
    """HHPractical{Point,Spot,Rect}Light: fixture mesh + a light component at light_offset (fixture-local)."""
    m = mat(loc, rot)
    cls = {'point': 'HHPracticalPointLight', 'spot': 'HHPracticalSpotLight', 'rect': 'HHPracticalRectLight'}[kind]
    light = {
        'RelativeLocation': H.ue_vector(light_offset),
        'Intensity': intensity,
        'IntensityUnits': enum('ELightUnits', 'Candelas'),
        'bUseTemperature': color is None,
        'Temperature': float(temperature),
        'AttenuationRadius': radius,
        'CastShadows': shadows,
        'VolumetricScatteringIntensity': volumetric,
    }
    if kind != 'point':
        light['RelativeDirection'] = [round(light_dir[0], 4), round(-light_dir[1], 4), round(light_dir[2], 4)]
    if color is not None:
        light['LightColor'] = list(color)
    if kind == 'spot':
        light['InnerConeAngle'] = cone[0]
        light['OuterConeAngle'] = cone[1]
        light['SourceRadius'] = source
    elif kind == 'point':
        light['SourceRadius'] = source
    else:
        light['SourceWidth'] = rect[0]
        light['SourceHeight'] = rect[1]
        light['BarnDoorAngle'] = 75.0
    comps = {'Light': light}
    if fixture:
        # Fixtures wrap their own light source; only shades that should cut the light cast shadows.
        comps['Fixture'] = {'CastShadow': fixture_shadow}
    if hum:
        comps['Hum'] = {'Sound': asset(hum)}
    props = {'FlickerMode': enum('EHHFlickerMode', flicker), 'FlickerStrength': strength}
    if emissive is not None:
        props['EmissiveStrength'] = emissive
    obj, entry = actor(cls, m, collection, mesh=fixture, mesh_component='Fixture', properties=props,
                       components=comps, tags=tags, name=name)
    # Preview light in Blender.
    world_pos = m @ Vector(light_offset)
    bl_type = {'point': 'POINT', 'spot': 'SPOT', 'rect': 'AREA'}[kind]
    ld = bpy.data.lights.new((name or cls) + '_L', bl_type)
    ld.color = color if color is not None else kelvin_rgb(temperature)
    ld.energy = intensity * 4 * math.pi / 683.0 * 60.0 * (0.25 if kind == 'rect' else 1.0)
    if kind == 'spot':
        ld.spot_size = math.radians(cone[1] * 2)
        ld.spot_blend = 1 - cone[0] / max(cone[1], 1e-3)
        ld.shadow_soft_size = source / 100
    elif kind == 'point':
        ld.shadow_soft_size = source / 100
    else:
        ld.size = rect[0] / 100
    lo = bpy.data.objects.new(ld.name, ld)
    world_dir = m.to_3x3() @ Vector(light_dir)
    lo.matrix_world = Matrix.Translation(world_pos) @ world_dir.to_track_quat('-Z', 'Y').to_matrix().to_4x4()
    H.link(lo, H.collection('PreviewLights'))
    PREVIEW_LIGHTS.append(lo)
    return obj, entry


def emitter(loc, loop=None, oneshots=None, interval=(4.0, 14.0), scatter=150.0, caption=None, volume=(0.5, 1.0),
            name='Emitter'):
    props = {'MinInterval': interval[0], 'MaxInterval': interval[1], 'ScatterRadius': scatter * 1.0,
             'VolumeRange': list(volume)}
    if oneshots:
        props['OneShots'] = [asset(s) for s in oneshots]
    if caption:
        props['Caption'] = text(caption)
    comps = {'Loop': {'Sound': asset(loop)}} if loop else None
    return actor('HHAmbientEmitter', mat(loc), 'Audio', properties=props, components=comps, name=_unique_name(name))


def tag_point(tag, loc):
    return actor('TargetPoint', mat(loc), 'Gameplay', tags=[tag], name=_unique_name(tag))


def decal(material, loc, size, rot=(0, 0, 0), depth=0.3, name='Decal'):
    """Projects along local -Z (floor decals need no rotation). size = (w, h) metres."""
    m = mat(loc, rot)
    obj, entry = actor('DecalActor', m, 'Decals', name=_unique_name(name), camera_like=True,
                       properties={'DecalMaterial': asset(material)},
                       components={'Decal': {'DecalSize': [depth * 50, size[0] * 50, size[1] * 50]}})
    # Preview: a thin plane with the decal texture.
    b = Builder('_decal_preview')
    b.grid_plane(size[0], size[1], (0, 0, 0.002), material, nx=1, ny=1, uv_scale=1.0)
    me = b.mesh()
    for loop in me.uv_layers[0].data:
        loop.uv = (loop.uv[0] / size[0], loop.uv[1] / size[1])
    po = bpy.data.objects.new(entry['name'] + '_preview', me)
    po.matrix_world = m
    H.link(po, H.collection('PreviewDecals'))
    return obj


# =======================================================================================
# Architecture

PAINT_LINE = 1.1


def wall(b, axis, a0, a1, c0, c1, z0, z1, openings=(), lower='MI_Block_Lower', upper='MI_Block_Upper'):
    """Wall along 'axis' ('X' or 'Y') from a0 to a1, thickness from c0 to c1 on the other axis.
    openings: [(a0, a1, z0, z1)] holes."""
    cuts = sorted({a0, a1, *[o[0] for o in openings], *[o[1] for o in openings]})
    cuts = [c for c in cuts if a0 <= c <= a1]
    for s0, s1 in zip(cuts, cuts[1:]):
        if s1 - s0 < 1e-4:
            continue
        holes = sorted((o[2], o[3]) for o in openings if o[0] <= s0 + 1e-4 and o[1] >= s1 - 1e-4)
        spans = []
        z = z0
        for h0, h1 in holes:
            if h0 > z:
                spans.append((z, h0))
            z = max(z, h1)
        if z < z1:
            spans.append((z, z1))
        for zz0, zz1 in spans:
            parts = [(zz0, zz1)]
            if lower != upper and zz0 < PAINT_LINE < zz1:
                parts = [(zz0, PAINT_LINE), (PAINT_LINE, zz1)]
            for p0, p1 in parts:
                m = lower if p1 <= PAINT_LINE + 1e-4 else upper
                if axis == 'X':
                    b.box_minmax((s0, c0, p0), (s1, c1, p1), m)
                else:
                    b.box_minmax((c0, s0, p0), (c1, s1, p1), m)


GARAGE_DOOR = (-5.2, -2.0, 0.0, 2.9)
NORTH_WINDOW = (0.9, 2.1, 2.5, 3.1)
WEST_WINDOW = (2.4, 3.6, 2.5, 3.1)
STAIR_DOOR = (5.94, 6.86, 2.2, 4.26)
BOILER_DOOR = (-6.61, -5.69, 0.0, 2.06)
CEIL = 3.4
T = 0.25


def build_architecture():
    # Floor (main + boiler room) and parking lines.
    floor = Builder('SM_HO_Floor')
    floor.box_minmax((-7.25, -5.25, -0.3), (7.25, 5.25, 0.0), 'MI_Concrete_Floor')
    floor.box_minmax((-7.25, -8.45, -0.3), (-3.35, -5.25, 0.0), 'MI_Concrete_Floor')
    floor.box_minmax((-5.4, 5.25, -0.3), (-1.8, 5.6, 0.0), 'MI_Concrete_Floor')
    for x in (-5.0, -2.2):
        floor.box_minmax((x - 0.05, -1.4, 0.0), (x + 0.05, 4.8, 0.002), 'MI_Plastic_Yellow')
    static(register_unique(floor, nanite=True), collection='Architecture')

    walls = Builder('SM_HO_Walls')
    # North wall (two parts: the stair bay is taller).
    wall(walls, 'X', -7.25, 5.8, 5.0, 5.25, 0, CEIL, [GARAGE_DOOR, NORTH_WINDOW])
    wall(walls, 'X', 5.8, 7.25, 5.0, 5.25, 0, 4.6, [STAIR_DOOR])
    # South wall.
    wall(walls, 'X', -7.25, 7.25, -5.25, -5.0, 0, CEIL, [BOILER_DOOR])
    # West wall (+ the boiler room's).
    wall(walls, 'Y', -5.0, 5.0, -7.25, -7.0, 0, CEIL, [WEST_WINDOW])
    # East wall.
    wall(walls, 'Y', -5.0, 0.5, 7.0, 7.25, 0, CEIL)
    wall(walls, 'Y', 0.5, 5.0, 7.0, 7.25, 0, 4.6)
    # Stair shaft above the ceiling.
    wall(walls, 'Y', 0.25, 5.0, 5.55, 5.8, CEIL, 4.6, upper='MI_Plaster', lower='MI_Plaster')
    wall(walls, 'X', 5.55, 7.0, 0.25, 0.5, CEIL, 4.6, upper='MI_Plaster', lower='MI_Plaster')
    static(register_unique(walls, nanite=True), collection='Architecture')

    boiler = Builder('SM_HO_BoilerRoom')
    wall(boiler, 'Y', -8.45, -5.25, -7.25, -7.0, 0, 3.0, lower='MI_Block_Raw', upper='MI_Block_Raw')
    wall(boiler, 'Y', -8.45, -5.25, -3.6, -3.35, 0, 3.0, lower='MI_Block_Raw', upper='MI_Block_Raw')
    wall(boiler, 'X', -7.25, -3.35, -8.45, -8.2, 0, 3.0, lower='MI_Block_Raw', upper='MI_Block_Raw')
    boiler.box_minmax((-7.25, -8.45, 3.0), (-3.35, -5.25, 3.3), 'MI_Concrete_Ceiling')
    static(register_unique(boiler, nanite=True), collection='Architecture')

    ceiling = Builder('SM_HO_Ceiling')
    ceiling.box_minmax((-7.25, -5.25, CEIL), (5.8, 5.25, CEIL + 0.3), 'MI_Concrete_Ceiling')
    ceiling.box_minmax((5.8, -5.25, CEIL), (7.25, 0.5, CEIL + 0.3), 'MI_Concrete_Ceiling')
    ceiling.box_minmax((5.55, 0.25, 4.6), (7.25, 5.25, 4.9), 'MI_Concrete_Ceiling')
    static(register_unique(ceiling, nanite=True), collection='Architecture')

    # Stairs along the east wall, rising north to a landing at 2.2 m.
    stairs = Builder('SM_HO_Stairs')
    steps = 12
    y0, y1, top = 0.8, 3.9, 2.2
    run = (y1 - y0) / steps
    rise = top / steps
    for i in range(steps):
        z = (i + 1) * rise
        stairs.box_minmax((5.85, y0 + i * run, 0), (7.0, y0 + (i + 1) * run, z - 0.03), 'MI_Concrete_Raw')
        stairs.box_minmax((5.84, y0 + i * run - 0.02, z - 0.03), (7.0, y0 + (i + 1) * run, z), 'MI_Wood_Raw',
                          bevel=0.006)
    stairs.box_minmax((5.85, y1, 0), (7.0, 5.0, top - 0.03), 'MI_Concrete_Raw')
    stairs.box_minmax((5.84, y1 - 0.02, top - 0.03), (7.0, 5.0, top), 'MI_Wood_Raw', bevel=0.006)
    static(register_unique(stairs), collection='Architecture')

    railing = Builder('SM_HO_Railing')
    rail_pts = []
    for i in range(0, steps + 1, 3):
        y = y0 + i * run + run * 0.5
        z = min(top, (i + 1) * rise)
        railing.cyl(0.02, 0.95, (5.9, y, z + 0.475), 'MI_Metal_Black', segs=10)
        rail_pts.append((5.9, y, z + 0.95))
    rail_pts.append((5.9, 4.95, top + 0.95))
    railing.tube(rail_pts, 0.025, 'MI_Wood_Dark', segs=10, bend=0.1)
    railing.tube([(p[0], p[1], p[2] - 0.45) for p in rail_pts], 0.012, 'MI_Metal_Black', segs=8, bend=0.1)
    static(register_unique(railing, collision='complex'), collection='Architecture')

    # Outside: ramp up to the street, retaining walls, ground by the windows.
    outside = Builder('SM_HO_Outside')
    ground_z = 2.45
    ramp_end = 12.0
    for i in range(12):
        ya = 5.6 + (ramp_end - 5.6) * i / 12
        yb = 5.6 + (ramp_end - 5.6) * (i + 1) / 12
        za = ground_z * i / 12
        zb = ground_z * (i + 1) / 12
        outside.box_minmax((-5.4, ya, -0.3), (-1.8, yb, (za + zb) / 2), 'MI_Asphalt_Wet')
    outside.box_minmax((-5.4, ramp_end, -0.3), (-1.8, 18.0, ground_z), 'MI_Asphalt_Wet')
    for x0, x1 in ((-5.65, -5.4), (-1.8, -1.55)):
        outside.box_minmax((x0, 5.25, -0.3), (x1, ramp_end, ground_z + 0.4), 'MI_Concrete_Raw')
    outside.box_minmax((-16.0, 5.25, -0.3), (-5.65, 18.0, ground_z), 'MI_Asphalt_Wet')
    outside.box_minmax((-1.55, 5.25, -0.3), (16.0, 18.0, ground_z), 'MI_Asphalt_Wet')
    outside.box_minmax((-16.0, -10.0, -0.3), (-7.6, 5.25, ground_z), 'MI_Asphalt_Wet')
    # The building above grade: brick facades with the window and garage openings.
    wall(outside, 'X', -7.6, 7.25, 5.25, 5.6, ground_z, 7.0, [GARAGE_DOOR, NORTH_WINDOW], lower='MI_Brick',
         upper='MI_Brick')
    wall(outside, 'Y', -10.0, 5.25, -7.6, -7.25, ground_z, 7.0, [WEST_WINDOW], lower='MI_Brick', upper='MI_Brick')
    static(register_unique(outside, collision='complex', nanite=True), collection='Architecture')

    # Structure: beams, columns, ducts.
    for y in (-2.5, 0.0, 2.5):
        static('SM_IBeam', (0, y, CEIL), collection='Architecture')
    for x in (-1.2, 2.6):
        static('SM_Column_Steel', (x, 0, 0), collection='Architecture')
    for x in (-3.0, 1.0):
        static('SM_Duct', (x, 3.75, CEIL), collection='Architecture')
    static('SM_FloorDrain', (-0.6, 1.4, 0), collection='Architecture')

    # Pipes & conduit (one unique mesh).
    pipes = Builder('SM_HO_Pipes')
    pipes.tube([(7.0, -4.82, 3.12), (-5.0, -4.82, 3.12), (-5.0, -6.2, 3.12), (-5.0, -6.2, 2.7), (-4.6, -7.4, 2.7),
                (-4.6, -7.4, 2.25)], 0.045, 'MI_Metal_Black', segs=12, bend=0.25)
    pipes.tube([(7.0, -4.72, 3.0), (-4.7, -4.72, 3.0), (-4.7, -6.0, 3.0), (-4.7, -6.0, 2.6), (-3.85, -7.4, 2.6),
                (-3.85, -7.4, 1.3)], 0.028, 'MI_Copper', segs=10, bend=0.2)
    for x in range(-4, 7, 2):
        pipes.box((0.04, 0.3, 0.02), (x, -4.85, 3.18), 'MI_Metal_Black')
        pipes.box((0.04, 0.02, 0.24), (x, -4.98, 3.08), 'MI_Metal_Black')
    # Conduit from the breaker box to the lamps.
    pipes.tube([(-5.2, -4.98, 1.65), (-5.2, -4.98, 3.32), (-3.0, -4.98, 3.32), (-3.0, -3.0, 3.32), (-3.0, 0.0, 3.05),
                (-3.0, 1.0, 3.32), (-3.6, 1.0, 3.32)], 0.012, 'MI_Steel', segs=6, bend=0.08)
    pipes.tube([(-3.0, 1.0, 3.32), (1.5, 1.0, 3.32), (1.5, 2.9, 3.32)], 0.012, 'MI_Steel', segs=6, bend=0.08)
    pipes.tube([(-3.0, -3.0, 3.32), (2.0, -3.0, 3.32), (2.0, -3.3, 3.32)], 0.012, 'MI_Steel', segs=6, bend=0.08)
    for x, y in ((-3.0, -3.0), (-3.0, 1.0)):
        pipes.box((0.1, 0.1, 0.06), (x, y, 3.34), 'MI_Steel', bevel=0.005)
    static(register_unique(pipes, collision='none'), collection='Architecture')

    # Doors, frames, windows.
    static('SM_RollUpDoor', (-3.6, 5.0, 0), collection='Architecture')
    static('SM_Window_Basement', (1.5, 5.125, 2.8), collection='Architecture')
    static('SM_Window_Basement', (-7.125, 3.0, 2.8), rot=90, collection='Architecture')
    static('SM_DoorFrame', (6.4, 5.125, 2.2), collection='Architecture')
    static('SM_DoorFrame', (-6.15, -5.125, 0), collection='Architecture')
    # The stair door stays shut (the shop upstairs is not ours tonight).
    static('SM_Door_Steel', (5.95, 5.1, 2.2), collection='Architecture')
    tag_point('HH_StairDoor', (6.4, 4.9, 3.2))
    actor('HHDoorActor', mat((-6.6, -5.125, 0)), 'Gameplay', mesh='SM_Door_Wood', mesh_component='DoorMesh',
          properties={'OpenSound': asset('S_Door_Wood_Open'), 'CloseSound': asset('S_Door_Wood_Close'),
                      'CreakSound': asset('S_Door_Creak_Slow'), 'OpenAngle': 100.0},
          tags=['HH_CreepyDoor'], name='BoilerRoomDoor')


# =======================================================================================
# Areas

def van_area():
    van = mat((-3.6, 1.6, 0))
    static('SM_Van_Body', parent=van, collection='Van')
    for y in (-1.55, 1.45):
        static('SM_Van_Wheel', (0.88, y, 0.375), parent=van, collection='Van')
        static('SM_Van_Wheel', (-0.88, y, 0.375), rot=180, parent=van, collection='Van')
    static('SM_Van_Livery', (1.012, -1.38, 1.45), rot=90, parent=van, collection='Van', shadow=False)
    static('SM_Van_Livery', (-1.012, -0.2, 1.45), rot=-90, parent=van, collection='Van', shadow=False)
    actor('HHReadyStation', van @ mat((0.99, 0.2, 1.2)), 'Gameplay', mesh='SM_Van_SlidingDoor',
          properties={'InteractSound': asset('S_Van_Door_Slide')}, name='VanDoor_Ready')
    decal('MI_Decal_Oil', (-3.5, 2.0, 0), (2.2, 2.2), name='OilStain')
    decal('MI_Decal_Oil', (-3.9, -0.2, 0), (1.0, 1.0), rot=(0, 0, 40), name='OilStain')
    decal('MI_Decal_Water', (-1.6, 4.3, 0), (1.8, 1.8), name='WaterStain')
    decal('MI_Decal_Footprints', (-1.0, 3.2, 0), (2.6, 0.65), rot=(0, 0, -60), name='Footprints')
    decal('MI_Decal_Footprints', (0.2, 1.2, 0), (2.6, 0.65), rot=(0, 0, -40), name='Footprints')
    # Clutter around the bay.
    static('SM_Barrel', (-6.45, 4.45, 0), rot=20)
    static('SM_Barrel', (-6.4, 3.75, 0), rot=-35)
    static('SM_Tire', (-1.4, 4.5, 0))
    static('SM_Tire', (-1.38, 4.52, 0.2), rot=30)
    static('SM_Tire', (-1.42, 4.48, 0.4), rot=65)
    static('SM_Sawhorse', (-5.2, -1.6, 0), rot=10)
    static('SM_Box_Open', (-1.3, 3.5, 0), rot=12)
    static('SM_EQ_Duffel', (-1.85, 0.6, 0), rot=35)
    static('SM_EQ_Sack', (-5.3, 4.5, 0), rot=10)
    static('SM_EQ_Lantern', (-6.73, 0.55, 1.615), rot=90)
    static('SM_EQ_Camera', (6.4, -0.55, 0.762), rot=-110)
    static('SM_EQ_NoiseMeter', (-6.45, -1.75, 0.76), rot=80)
    static('SM_Shelf_Metal', (-6.73, 0.2, 0), rot=90)
    for z, items in ((0.135, ('SM_Box_A', 'SM_Box_B')), (0.615, ('SM_PaintCan', 'SM_PaintCan', 'SM_Bucket')),
                     (1.115, ('SM_Box_B', 'SM_Shoebox')), (1.615, ('SM_Box_A',))):
        for i, item in enumerate(items):
            static(item, (-6.73, -0.25 + i * 0.4, z), rot=90 + i * 13)
    static('SM_FireExtinguisher', (-6.85, 1.2, 0.0), rot=90)
    emitter((-3.6, 4.9, 2.2), loop='A_Rain_Window_Loop', name='Rain_GarageDoor')
    emitter((-3.6, 4.8, 1.5), oneshots=['A_Wind_Gust_01', 'A_Wind_Gust_02', 'A_Wind_Gust_03'], interval=(25, 60),
            scatter=100, caption='[Wind rattles the garage door]', name='Wind')


def job_board():
    wall_x = -7.0
    board = mat((wall_x, -2.5, 1.0), rot=90)
    cam = camera('CAM_Play', (-4.7, -1.55, 1.72), (-7.0, -2.65, 1.55), fov=56)
    actor('HHStationActor', board, 'Stations', mesh='SM_Corkboard',
          properties={'Screen': enum('EHHLobbyScreen', 'Play'), 'CameraDrift': 0.35,
                      'Prompt': text('Look at the jobs')},
          components={'ViewCamera': cam}, name='Station_Play')
    face = wall_x + 0.024
    layer = [0]

    def on_board(name, y, z, tilt=0.0, pin=True):
        layer[0] += 1
        static(name, (face + 0.0006 * layer[0], y, z), rot=(0, -tilt, 90), collection='Board', shadow=False)
        if pin:
            static('SM_Pushpin', (face + 0.002, y, z + (0.035 if name.startswith('SM_Polaroid') else 0.03)), rot=90,
                   collection='Board', shadow=False)

    on_board('SM_Board_Map', -2.62, 1.62, pin=False)
    for dy, dz in ((-0.4, 0.3), (0.4, 0.3), (-0.4, -0.3), (0.4, -0.3)):
        static('SM_Pushpin', (face + 0.003, -2.62 + dy, 1.62 + dz), rot=90, collection='Board', shadow=False)
    on_board('SM_Poster_Missing', -1.75, 1.68, tilt=-2)
    on_board('SM_Poster_Museum', -3.42, 1.62, tilt=1.5)
    for i, (y, z, tilt) in enumerate(((-3.45, 2.13, 3), (-3.0, 2.14, -4), (-2.55, 2.12, 2), (-2.1, 2.13, -3))):
        on_board(f'SM_Polaroid_{i}', y, z, tilt)
    for i, (y, z, tilt) in enumerate(((-1.45, 2.12, 6), (-1.95, 2.15, -5), (-1.5, 1.22, 4), (-3.1, 1.15, -6))):
        on_board(f'SM_Polaroid_{4 + i}', y, z, tilt)
    notes = ((-3.55, 1.2, -4), (-2.92, 1.18, 7), (-2.35, 1.17, -3), (-1.98, 1.23, 5), (-2.05, 2.02, -6),
             (-3.62, 2.05, 4), (-2.7, 2.18, -2), (-1.62, 1.95, 8))
    for i, (y, z, tilt) in enumerate(notes):
        on_board(f'SM_Note_{i}', y, z, tilt)
    # Red string from each job polaroid to its spot on the map.
    string = Builder('SM_HO_BoardString')
    links = (((-3.45, 2.1), (-2.85, 1.86)), ((-3.0, 2.1), (-2.38, 1.72)), ((-2.55, 2.1), (-2.33, 1.86)),
             ((-2.1, 2.1), (-2.27, 1.42)), ((-1.75, 1.95), (-2.38, 1.72)))
    for (ya, za), (yb, zb) in links:
        string.tube([(face + 0.012, ya, za + 0.03), (face + 0.016, (ya + yb) / 2, (za + zb) / 2 + 0.01),
                     (face + 0.012, yb, zb)], 0.0018, 'MI_Plastic_Red', segs=4)
    static(register_unique(string, folder='Props/Board', collision='none'), collection='Board', shadow=False)
    for y, z in ((-2.85, 1.86), (-2.38, 1.72), (-2.33, 1.86), (-2.27, 1.42)):
        static('SM_Pushpin', (face + 0.003, y, z), rot=90, collection='Board', shadow=False)

    # Desk under the board.
    static('SM_Desk_Wood', (-6.6, -2.5, 0), rot=90)
    top = 0.76
    practical('spot', 'SM_DeskLamp', (-6.78, -1.98, top), rot=180, light_offset=(0.19, 0, 0.415),
              light_dir=(0.42, 0, -0.9), intensity=180, temperature=2600, radius=450, cone=(25, 55), source=2.5,
              emissive=25, name='Light_DeskLamp')
    static('SM_Newspaper', (-6.5, -2.65, top), rot=97)
    static('SM_Mug', (-6.35, -2.05, top), rot=30)
    static('SM_Ashtray', (-6.7, -3.05, top))
    static('SM_Clipboard', (-6.45, -3.25, top), rot=75)
    static('SM_Walkie', (-6.8, -3.4, top), rot=100)
    static('SM_Chair_Wood', (-5.95, -2.45, 0), rot=-75)
    static('SM_Calendar', (-6.99, -0.75, 1.75), rot=90, shadow=False)
    static('SM_Box_B', (-6.7, -3.6, 0), rot=95)
    static('SM_Binder', (-6.85, -1.55, top), rot=90)


def workbench_area():
    static('SM_Pegboard', (-3.0, -5.0, 1.15), rot=180)
    face = -5.0 + 0.03
    for name, x, z in (('SM_Tool_Hammer', -3.65, 1.8), ('SM_Tool_Wrench', -3.35, 1.85), ('SM_Tool_Screwdriver', -3.12, 1.85),
                       ('SM_Tool_Screwdriver', -3.0, 1.83), ('SM_Tool_Pliers', -2.8, 1.85), ('SM_Tool_Saw', -2.55, 1.75),
                       ('SM_Tool_Wrench', -2.1, 1.82)):
        static(name, (x, face + 0.012, z), rot=180, collection='Workshop', shadow=True)
        static('SM_PegHook', (x, face, z + 0.05), rot=180, collection='Workshop')
    static('SM_BenchVise', (-2.2, -4.55, 0.92), rot=180)
    # Tonight's gear, laid out on the bench.
    for name, loc, rot in (('SM_EQ_Torch_Pocket', (-2.62, -4.42, 0.92), 25), ('SM_EQ_Crowbar', (-3.08, -4.3, 0.92), 3),
                           ('SM_EQ_Lockpicks', (-2.9, -4.68, 0.92), 8), ('SM_EQ_Walkie', (-3.88, -4.42, 0.92), 100),
                           ('SM_EQ_Headlamp', (-2.45, -4.75, 0.92), -20), ('SM_EQ_GlassCutter', (-3.35, -4.62, 0.92), 60)):
        static(name, loc, rot=rot, collection='Workshop')
    static('SM_Toolbox', (-3.55, -4.72, 0.92), rot=172)
    static('SM_PaintCan', (-3.6, -4.6, 0.21))
    static('SM_PaintCan', (-3.4, -4.7, 0.21), rot=40)
    static('SM_Box_A', (-2.5, -4.6, 0.21), rot=180)
    static('SM_Locker_Triple', (-0.6, -4.73, 0), rot=180)
    static('SM_Stool', (-2.9, -3.8, 0))
    practical('rect', 'SM_Lamp_Fluorescent', (-3.0, -4.3, CEIL), light_offset=(0, 0, -0.62), light_dir=(0, 0, -1),
              intensity=420, temperature=4300, radius=1100, rect=(120, 12), flicker='Buzz', strength=0.6,
              hum='A_Hum_Fluorescent_Loop', tags=['HH_MainLights', 'HH_Flickerable'], emissive=20,
              name='Light_Fluoro_Bench')
    cam = camera('CAM_Loadout', (-2.0, -2.7, 1.7), (-3.05, -4.85, 1.12), fov=58)
    actor('HHStationActor', mat((-3.0, -4.575, 0), rot=180), 'Stations', mesh='SM_Workbench',
          properties={'Screen': enum('EHHLobbyScreen', 'Loadout'), 'CameraDrift': 0.3,
                      'Prompt': text('Gear up')},
          components={'ViewCamera': cam}, name='Station_Loadout')
    # Breaker box next to the boiler-room door (the main lights' switch).
    actor('HHLightSwitch', mat((-5.2, -5.0, 1.45), rot=90), 'Gameplay', mesh='SM_BreakerBox',
          properties={'LightTag': 'HH_MainLights', 'InteractSound': asset('S_Switch_Breaker'),
                      'Prompt': text('Main lights')},
          components={'Lever': {'StaticMesh': asset('SM_BreakerLever'), 'RelativeLocation': H.ue_vector((0.13, 0, 0))}},
          name='BreakerSwitch')
    static('SM_FireExtinguisher', (-4.75, -4.88, 0), rot=180)
    decal('MI_Decal_Grime', (-3.0, -4.99, 2.6), (2.4, 1.6), rot=(-90, 0, 0), name='Grime')
    decal('MI_Decal_Grime', (4.0, 4.99, 2.4), (1.6, 1.4), rot=(90, 0, 0), name='Grime')
    emitter((0.0, -4.7, 3.0), oneshots=['A_PipeCreak_01', 'A_PipeCreak_02', 'A_PipeCreak_03', 'A_PipeCreak_04'],
            interval=(20, 55), scatter=400, caption='[Pipes groan]', name='PipeCreaks')


def mirror_corner():
    static('SM_ClothingRack', (3.1, -4.35, 0), rot=175)
    for i, (garment, dx) in enumerate((('SM_Garment_Coat', -0.5), ('SM_Garment_Hoodie', -0.15), ('SM_Garment_Jacket', 0.2),
                                       ('SM_Garment_Coat', 0.5))):
        rack = mat((3.1, -4.35, 0), rot=175)
        static('SM_Hanger', (dx, 0, 1.6), rot=(0, 0, 90 + (i % 2) * 8), parent=rack, collection='Wardrobe')
        static(garment, (dx, 0, 1.55), rot=(0, 0, 90 + (i % 2) * 8), parent=rack, collection='Wardrobe')
    static('SM_Bench_Wood', (0.78, -4.55, 0), rot=180)
    static('SM_Shoebox', (0.5, -4.55, 0.46), rot=170)
    static('SM_Shoebox', (1.08, -4.6, 0.46), rot=185)
    practical('point', 'SM_Lamp_Bulb', (2.0, -3.4, CEIL), light_offset=(0, 0, -0.99), intensity=140, temperature=2500,
              radius=650, source=3, flicker='Flicker', strength=0.25, emissive=30, tags=['HH_Flickerable'],
              name='Light_Bulb_Mirror')
    # Preview station: root at the mirror, forward (+X in Unreal) pointing north into the room.
    station = mat((2.0, -4.75, 0), rot=90)
    mirror_rel = mat((0.15, 0, 0), rot=90)    # mirror 15 cm ahead of the root, facing north
    obj_from_mesh('SM_Mirror_Standing', station @ mirror_rel, 'Stations')
    rel = H.ue_transform(empty('mirror_rel', mirror_rel, 'Stations'))
    actor('HHPreviewStation', station, 'Stations',
          properties={'Screen': enum('EHHLobbyScreen', 'Customization'), 'Prompt': text('Change clothes')},
          components={'Mesh': {'StaticMesh': asset('SM_Mirror_Standing'), 'RelativeLocation': rel['location'],
                               'RelativeQuat': rel['quat']}},
          name='Station_Customization')
    # Emulate the preview boom camera for Blender renders (PreviewSpot 120 cm ahead, arm 260, pitch -6, offset 45).
    spot = Vector((2.0, -4.75 + 1.2, 1.0))
    pitch = math.radians(6)
    cam_pos = spot + Vector((-0.45, 2.6 * math.cos(pitch), 2.6 * math.sin(pitch)))
    camera('CAM_Customization_Preview', tuple(cam_pos), tuple(spot + Vector((-0.45, 0, -0.25))), fov=45)
    # Tripod work light as the key light on whoever stands in front of the mirror.
    practical('spot', 'SM_WorkLight', (3.45, -2.35, 0), rot=-50.6, light_offset=(0, -0.1, 1.7),
              light_dir=(0, -0.94, -0.34), intensity=380, temperature=3300, radius=900, cone=(18, 42), source=8,
              emissive=20, name='Light_WorkLight', fixture_shadow=False)
    decal('MI_Decal_Cracks', (0.8, -2.6, 0), (2.2, 2.2), rot=(0, 0, 20), name='Cracks')


def cage_area():
    static('SM_Cage_Panel', (4.6, -2.6, 0))
    static('SM_Cage_Panel', (6.9, -2.6, 0), scale=(0.17, 1.0, 1.0))
    for y in (-3.2, -4.4):
        static('SM_Cage_Panel', (4.0, y, 0), rot=90)
    static('SM_Cage_Panel', (6.0, -2.6, 2.0), scale=(1.33, 1.0, 0.167))
    static('SM_CashBox', (5.6, -2.85, 1.02), rot=170)
    static('SM_Phone_Rotary', (6.45, -2.95, 1.02), rot=200)
    static('SM_Sign_Fence', (6.0, -2.55, 2.35), rot=180)
    static('SM_Shelf_Metal', (5.0, -4.72, 0), rot=180)
    static('SM_Shelf_Metal', (6.3, -4.72, 0), rot=180)
    for x0 in (5.0, 6.3):
        static('SM_Duffel', (x0, -4.72, 0.135), rot=180)
        static('SM_Box_A', (x0 - 0.3, -4.75, 0.615), rot=185)
        static('SM_Box_B', (x0 + 0.3, -4.7, 0.615), rot=175)
        static('SM_Duffel', (x0, -4.72, 1.115), rot=170, scale=0.8)
        static('SM_Toolbox', (x0, -4.75, 1.615), rot=180)
    static('SM_Safe', (6.65, -3.7, 0), rot=-90)
    static('SM_Stool', (6.1, -3.45, 0))
    practical('point', 'SM_Lamp_Bulb', (6.0, -2.15, CEIL), light_offset=(0, 0, -0.99), intensity=120, temperature=2600,
              radius=600, flicker='Steady', emissive=30, tags=['HH_Flickerable'], name='Light_Counter')
    practical('point', 'SM_Lamp_Caged', (5.6, -5.0, 2.4), rot=180, light_offset=(0, -0.12, 0), intensity=160,
              temperature=3000, radius=700, flicker='Flicker', strength=0.35, emissive=25, tags=['HH_Flickerable'],
              name='Light_Cage')
    cam = camera('CAM_Store', (4.75, -0.35, 1.9), (6.05, -3.3, 1.45), fov=58)
    actor('HHStationActor', mat((6.0, -2.9, 0), rot=180), 'Stations', mesh='SM_Counter',
          properties={'Screen': enum('EHHLobbyScreen', 'Store'), 'CameraDrift': 0.3,
                      'Prompt': text('Talk to the fence')},
          components={'ViewCamera': cam}, name='Station_Store')


def lounge():
    static('SM_Rug', (1.5, 3.3, 0), rot=2)
    static('SM_TVStand', (1.5, 4.75, 0))
    practical('rect', 'SM_TV_CRT', (1.5, 4.7, 0.53), rot=-6, light_offset=(-0.06, -0.3, 0.28), light_dir=(0, -1, 0),
              intensity=30, color=(0.7, 0.8, 1.0), radius=600, rect=(38, 30), flicker='Television', strength=0.8,
              hum='A_TV_Static_Loop', emissive=2.5, name='Light_TV')
    static('SM_Couch', (1.5, 2.25, 0), rot=180)
    static('SM_CoffeeTable', (1.5, 3.3, 0), rot=1)
    static('SM_Armchair', (3.75, 3.2, 0), rot=-122)
    static('SM_Crate_Wood', (3.9, 4.55, 0), rot=4)
    actor('HHRadioActor', mat((3.85, 4.55, 0.51), rot=-8), 'Gameplay', mesh='SM_Radio_Vintage',
          properties={'Stations': [asset('M_Radio_Station_A'), asset('M_Radio_Station_B')],
                      'StaticLoop': asset('A_Radio_Static_Loop'), 'TuneSound': asset('S_Radio_Tune'),
                      'Prompt': text('Tune the radio')},
          name='Radio')
    practical('point', 'SM_FloorLamp', (-0.25, 4.35, 0), light_offset=(0, 0, 1.45), intensity=70, temperature=2300,
              radius=600, source=8, emissive=3, name='Light_FloorLamp')
    practical('point', 'SM_SpaceHeater', (-0.3, 2.5, 0), rot=-55, light_offset=(0, -0.12, 0.3), intensity=12,
              color=(1.0, 0.35, 0.08), radius=300, flicker='Ember', strength=0.6, emissive=12, shadows=False,
              name='Light_Heater')
    for item, loc, rot in (('SM_BeerBottle', (1.2, 3.2, 0.42), 0), ('SM_BeerBottle', (1.32, 3.12, 0.42), 40),
                           ('SM_Can', (1.85, 3.45, 0.42), 0), ('SM_PizzaBox', (1.7, 3.2, 0.42), 14),
                           ('SM_Ashtray', (1.05, 3.45, 0.42), 0), ('SM_Magazine', (1.4, 3.5, 0.42), -20),
                           ('SM_Mug', (2.05, 3.15, 0.42), 120), ('SM_Magazine', (1.3, 3.3, 0.135), 35),
                           ('SM_BeerBottle', (0.6, 2.95, 0), 0), ('SM_Can', (2.6, 2.2, 0), 0)):
        static(item, loc, rot=rot, collection='Clutter', shadow=True)
    static('SM_Clock_Wall', (-0.6, 4.99, 2.35), collection='Clutter')
    emitter((-0.6, 4.95, 2.35), loop='A_Clock_Tick_Loop', name='ClockTick')
    emitter((1.5, 5.1, 2.8), loop='A_Rain_Window_Loop', name='Rain_NorthWindow')
    emitter((-7.1, 3.0, 2.8), loop='A_Rain_Window_Loop', name='Rain_WestWindow')
    emitter((0.0, 0.0, 3.6), oneshots=['A_WoodCreak_01', 'A_WoodCreak_02', 'A_WoodCreak_03'], interval=(30, 75),
            scatter=500, caption='[Floorboards creak overhead]', name='Creaks')


def security_desk():
    top = 0.762
    static('SM_CRT_Monitor', (6.72, -1.55, top), rot=-75)
    static('SM_CRT_Monitor', (6.72, -0.82, top), rot=-102)
    static('SM_Keyboard', (6.35, -1.15, top), rot=-90)
    static('SM_Binder', (6.85, -0.35, top), rot=0)
    static('SM_Mug', (6.3, -0.6, top), rot=20)
    static('SM_Stool', (5.85, -1.2, 0))
    static('SM_Calendar', (6.99, -2.15, 1.55), rot=-90, shadow=False)
    practical('point', None, (6.3, -1.15, 1.15), intensity=10, color=(0.55, 0.9, 0.65), radius=350,
              flicker='Television', strength=0.4, shadows=False, name='Light_Monitors')
    cam = camera('CAM_Settings', (4.85, -0.45, 1.58), (6.85, -1.2, 1.0), fov=58)
    actor('HHStationActor', mat((6.6, -1.15, 0), rot=-90), 'Stations', mesh='SM_Desk_Metal',
          properties={'Screen': enum('EHHLobbyScreen', 'Settings'), 'CameraDrift': 0.25,
                      'Prompt': text('Adjust settings')},
          components={'ViewCamera': cam}, name='Station_Settings')
    practical('point', 'SM_Lamp_Bulkhead', (6.4, 5.0, 4.45), light_offset=(0, -0.08, 0), intensity=45, temperature=3200,
              radius=600, flicker='Dying', strength=0.7, emissive=25, tags=['HH_Flickerable'], name='Light_StairTop')


def boiler_room():
    static('SM_Boiler', (-4.6, -7.4, 0))
    practical('point', None, (-4.6, -7.9, 0.45), intensity=20, color=(1.0, 0.35, 0.08), radius=300, flicker='Ember',
              strength=0.7, shadows=False, name='Light_BoilerFlame')
    static('SM_WaterHeater', (-6.5, -7.75, 0))
    static('SM_MopBucket', (-6.4, -5.9, 0), rot=30)
    static('SM_Shelf_Metal', (-6.73, -6.9, 0), rot=90)
    for z in (0.135, 0.615, 1.115):
        static('SM_PaintCan', (-6.75, -7.2, z), rot=z * 100)
        static('SM_Box_B', (-6.73, -6.7, z), rot=90 + z * 20)
    static('SM_Barrel', (-3.95, -5.75, 0), rot=80)
    practical('point', 'SM_Lamp_Bulb', (-5.3, -6.6, 3.0), light_offset=(0, 0, -0.99), intensity=55, temperature=2400,
              radius=550, flicker='Dying', strength=0.8, emissive=30, tags=['HH_Flickerable'], name='Light_Boiler')
    emitter((-4.6, -7.4, 1.0), loop='A_Hum_Boiler_Loop', name='BoilerHum')
    emitter((-5.6, -7.0, 2.6), oneshots=[f'A_Drip_0{i}' for i in range(1, 7)], interval=(2.5, 8.0), scatter=120,
            caption='[Water drips]', name='Drips')
    tag_point('HH_Whisper', (-5.6, -7.2, 1.6))
    decal('MI_Decal_Water', (-5.5, -7.0, 0), (1.6, 1.6), name='WaterStain')


def main_lights():
    for name, loc in (('Light_Pendant_Van', (-3.6, 1.0, CEIL)), ('Light_Pendant_Lounge', (1.5, 3.05, CEIL)),
                      ('Light_Pendant_Centre', (0.4, -1.4, CEIL)), ('Light_Pendant_Board', (-5.6, -2.5, CEIL))):
        practical('spot', 'SM_Lamp_Industrial', loc, light_offset=(0, 0, -0.8), light_dir=(0, 0, -1), intensity=260,
                  temperature=2700, radius=1000, cone=(28, 62), source=4, flicker='Steady',
                  tags=['HH_MainLights', 'HH_Flickerable'], emissive=30, name=name, fixture_shadow=True)
    # Outside: a sodium street lamp at the top of the ramp, seen through the garage-door windows.
    practical('spot', 'SM_StreetLamp', (-0.9, 12.6, 2.45), rot=180, light_offset=(0.9, 0, 4.3), light_dir=(0.19, 0.81, -0.57),
              intensity=1800, color=(1.0, 0.58, 0.25), radius=2600, cone=(35, 65), source=10, emissive=25,
              name='Light_Street')


def gameplay():
    # Main-menu establishing shot (not usable, the HUD just parks the camera here).
    cam = camera('CAM_MainMenu', (5.35, 3.6, 2.15), (-1.8, -2.4, 0.95), fov=62)
    actor('HHStationActor', mat((5.0, 3.6, 0)), 'Stations',
          properties={'Screen': enum('EHHLobbyScreen', 'MainMenu'), 'bUsable': False, 'CameraDrift': 0.6},
          components={'ViewCamera': cam}, name='Station_MainMenu')
    for i, (x, y) in enumerate(((-0.6, -1.3), (0.5, -1.9), (1.6, -1.3), (0.5, -0.6))):
        yaw = math.degrees(math.atan2(-2.5 - y, -6.0 - x))
        actor('PlayerStart', mat((x, y, 0.92), rot=yaw), 'Gameplay', name=f'PlayerStart_{i}',
              properties={'PlayerStartTag': f'Crew{i}'})
    tag_point('HH_OverheadStart', (-5.0, -3.0, 3.85))
    tag_point('HH_OverheadEnd', (5.0, 3.5, 3.85))
    tag_point('HH_Whisper', (6.4, 4.6, 3.0))
    tag_point('HH_Whisper', (-5.8, 4.4, 1.2))
    actor('HHAmbienceDirector', mat((0, 0, 2.0)), 'Audio', name='AmbienceDirector', properties={
        'OverheadSteps': [asset(f'A_Overhead_Step_0{i}') for i in range(1, 7)],
        'DistantDoorSound': asset('A_DoorSlam_Distant_01'), 'KnockSound': asset('A_Knock_Three'),
        'WhisperSounds': [asset(f'A_Whisper_0{i}') for i in range(1, 4)],
        'ElectricalStutterSound': asset('A_Electrical_Stutter'), 'RadioBleedSound': asset('A_Radio_Bleed')})
    actor('HHWeatherController', mat((1.5, 8.5, 5.0)), 'Audio', name='Weather', properties={
        'ThunderSounds': [asset(f'A_Thunder_0{i}') for i in range(1, 6)], 'ThunderCaption': text('[Thunder rolls]')})
    emitter((0, 0, 1.6), loop='A_RoomTone_Basement_Loop', name='RoomTone')
    emitter((0, 6.5, 3.0), loop='A_Rain_Outside_Loop', name='RainOutside')
    ACTORS.append(dict(name='LevelEnvironment', cls='HHEnvironment', folder='Environment',
                       location=[0, 0, 0], quat=[0, 0, 0, 1], scale=[1, 1, 1],
                       properties=dict(fog_density=0.025, fog_height_falloff=0.35, fog_color=[0.06, 0.07, 0.085],
                                       volumetric_albedo=[0.85, 0.85, 0.85], volumetric_extinction=1.2,
                                       exposure_min=-1.5, exposure_max=2.0, exposure_bias=0.6,
                                       bloom=0.55, vignette=0.45, grain=0.12, contrast=1.05,
                                       saturation=0.92, shadows_tint=[0.92, 0.97, 1.04], highlights_tint=[1.04, 1.0, 0.95],
                                       lens_dirt='T_LensDirt', sky_light_intensity=0.0)))


# =======================================================================================
# Export & preview

def export_all():
    manifest = H.MeshManifest(MANIFEST_PATH)
    first_obj = {}
    for obj in bpy.context.scene.objects:
        if obj.type == 'MESH' and obj.data.name in MESHES and obj.data.name not in first_obj:
            first_obj[obj.data.name] = obj
    for name, (me, folder, collision, nanite) in sorted(MESHES.items()):
        obj = first_obj.get(name)
        if obj is None:
            continue
        rel = os.path.join('SourceArt', 'Meshes', folder, name + '.fbx')
        H.export_mesh_object(obj, os.path.join(H.PROJECT, rel))
        manifest.add(name, rel.replace(os.sep, '/'), [m.name for m in me.materials], folder, collision, nanite)
        print('exported', name, flush=True)
    manifest.save()
    os.makedirs(os.path.dirname(LAYOUT_PATH), exist_ok=True)
    with open(LAYOUT_PATH, 'w', encoding='utf-8') as fh:
        json.dump(dict(level='L_Hideout', units='cm', actors=ACTORS), fh, indent=1)
    print('layout', LAYOUT_PATH, len(ACTORS), 'actors')


def render_previews(fast=False):
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'CPU'
    scene.cycles.samples = 24 if fast else 96
    scene.cycles.use_denoising = True
    scene.cycles.max_bounces = 6
    scene.render.resolution_x = 960 if not fast else 640
    scene.render.resolution_y = 540 if not fast else 360
    world = bpy.data.worlds.new('World')
    world.color = (0.002, 0.0025, 0.003)
    scene.world = world
    try:
        scene.view_settings.view_transform = 'AgX'
        scene.view_settings.look = 'AgX - Medium High Contrast'
    except TypeError:
        pass
    scene.view_settings.exposure = 1.0
    for col in ('PreviewDecals',):
        if col in bpy.data.collections:
            bpy.data.collections[col].hide_render = False
    out = os.path.join(H.PROJECT, 'SourceArt', 'Previews')
    os.makedirs(out, exist_ok=True)
    for name, cam in CAMERAS.items():
        scene.camera = cam
        scene.render.filepath = os.path.join(out, name.replace('CAM_', 'Hideout_') + '.jpg')
        scene.render.image_settings.file_format = 'JPEG'
        bpy.ops.render.render(write_still=True)
        print('rendered', name, flush=True)


def main():
    H.reset()
    build_architecture()
    van_area()
    job_board()
    workbench_area()
    mirror_corner()
    cage_area()
    lounge()
    security_desk()
    boiler_room()
    main_lights()
    gameplay()
    if '--no-export' not in sys.argv:
        export_all()
    if '--render' in sys.argv:
        render_previews(fast='--fast' in sys.argv)
    if '--save' in sys.argv:
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(H.PROJECT, 'SourceArt', 'Layout', 'Hideout.blend'))


if __name__ == '__main__':
    main()
