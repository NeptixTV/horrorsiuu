"""The abandoned manor and everything below it.

Above ground: the mansion shell from "Modular Kit Horror House 1" by Pieter Ferreira (CC-BY-4.0), scaled to
real size (0.45 m per source unit: 24 x 30 m, 10.9 m high). The shell is hollow, so this script cuts a real
front door into it and builds the ground floor rooms, the cellar, a mine drift, natural caves and a large
cavern itself (own geometry and own procedural textures).

Site coordinates (meters, +Y up): the front door is at x = 0, z = 0 and faces +Z; the ground is y = 0.
"""
import os
import numpy as np
from PIL import Image
from gltfio import Gltf
from meshutil import flatten, merge, Part, Builder
from glbout import GlbFile, MaterialSpec, stats
from arch import Arch
from sdfmesh import Field
import proctex

CREDIT = ('Mansion shell and shed: "Modular Kit Horror House 1" by Pieter Ferreira (https://sketchfab.com/Badboy17Aiden), CC-BY-4.0, modified. '
          'Interior, cellar, tunnels and caves: generated for SIGNAL UNKNOWN.')
S = 0.45
F0, F0C = 1.28, 4.28          # ground floor: floor / ceiling
K0, K0C = -3.1, -0.6          # cellar: floor / ceiling
DOOR_W, DOOR_H = 2.5, 2.55

# ---------------------------------------------------------------- underground route (floor points)
MINE = [(-6.2, -3.1, -4.6), (-13.0, -3.9, -4.6), (-21.0, -5.0, -8.0), (-27.0, -6.4, -16.0), (-29.0, -8.4, -27.0)]
CAVE = [(-29.0, -8.4, -27.0), (-23.0, -10.6, -37.0), (-12.0, -13.0, -42.0), (0.0, -15.5, -40.0), (10.0, -18.0, -33.0), (16.0, -20.6, -26.0)]
STORE = [(-27.0, -6.4, -16.0), (-32.0, -6.7, -13.0), (-37.0, -6.8, -11.5)]
CAVERN_C, CAVERN_R, CAVERN_FLOOR = (26.0, -20.5, -20.0), (13.0, 6.5, 12.0), -22.8
EXIT = [(22.0, -22.8, -29.0), (26.0, -24.0, -39.0), (23.0, -25.6, -46.5)]
HIDES = [((-21.0, -5.0, -8.0), (-21.5, -4.7, -2.0)), ((-12.0, -13.0, -42.0), (-10.5, -12.8, -35.5)), ((37.0, -22.8, -15.0), (43.5, -22.6, -12.5)),
         ((2.0, -16.0, -39.5), (3.5, -15.8, -46.0))]
TOMB_ENTRY = (22.0, -25.6, -50.0)      # start of the plank walkway of the tomb model (see build_tomb.py)
# dig chamber around the tomb entrance pit (site coordinates)
DIG_LO, DIG_HI = (8.2, -25.65, -55.15), (25.5, -21.8, -44.65)
PIT = (8.3, 15.8, -53.05, -47.05)      # x0, x1, z0, z1: stair pit of the tomb (kept free of cave floor)


def _fit_uv(xy, uv):
    """Affine map (x, y) -> (u, v)."""
    a = np.concatenate([xy, np.ones((len(xy), 1))], axis=1)
    m, *_ = np.linalg.lstsq(a, uv, rcond=None)
    return lambda p: np.concatenate([np.asarray(p, dtype=np.float64), np.ones((len(p), 1))], axis=1) @ m


def _materials(src_dir):
    t = os.path.join(src_dir, 'textures')
    kit = Image.open(os.path.join(t, 'Material__218_baseColor.png')).convert('RGB')
    kit_n = Image.open(os.path.join(t, 'Material__218_normal.png')).convert('RGB')
    m = {'kit': MaterialSpec('M_Manor_Kit', base=kit, normal=kit_n, roughness=0.88, double_sided=True, max_size=2048)}

    def add(key, name, pair, rough=0.9, size=512, metallic=0.0):
        m[key] = MaterialSpec(name, base=pair[0], normal=pair[1], roughness=rough, metallic=metallic, max_size=size)
    add('wallpaper', 'M_Manor_Wallpaper', proctex.wallpaper())
    add('plaster', 'M_Manor_Plaster', proctex.plaster())
    add('ceiling', 'M_Manor_Ceiling', proctex.plaster(seed=17, tint=(120, 116, 104)))
    add('floor', 'M_Manor_Floor', proctex.planks(), 0.75)
    add('wood', 'M_Manor_Wood', proctex.planks(seed=35, count=4, tint=(70, 50, 32)), 0.8)
    add('brick', 'M_Manor_Brick', proctex.brick(), 0.92)
    add('flag', 'M_Manor_Flagstone', proctex.flagstone(), 0.85)
    add('rock', 'M_UG_Rock', proctex.rock(), 0.9)
    add('dirt', 'M_UG_Dirt', proctex.dirt(), 0.96)
    add('timber', 'M_UG_Timber', proctex.timber(), 0.86)
    add('metal', 'M_Manor_Metal', proctex.metal(), 0.55, 256, 0.7)
    add('fabric', 'M_Manor_Fabric', proctex.fabric(), 0.95, 256)
    return m


UV_SCALE = {'wallpaper': 0.5, 'plaster': 0.4, 'ceiling': 0.35, 'floor': 0.45, 'wood': 0.9, 'brick': 0.55, 'flag': 0.4, 'rock': 0.3, 'dirt': 0.4,
            'timber': 1.2, 'metal': 1.0, 'fabric': 1.2}


# =============================================================================================== shell
def build_shell(src_dir):
    g = Gltf(os.path.join(src_dir, 'scene.gltf'))
    allp = merge(flatten(g), material='kit')
    c = allp.pos[allp.idx.reshape(-1, 3)].mean(axis=1)
    in_manor = (c[:, 0] > -12) & (c[:, 0] < 46) & (c[:, 2] > -101) & (c[:, 2] < -31)
    manor = allp.select_tris(in_manor)
    manor.pos = manor.pos * S + np.array([-7.745, 1.77, 20.66])
    shed_sel = (c[:, 0] > -52) & (c[:, 0] < -35) & (c[:, 2] > -61) & (c[:, 2] < -44)
    shed = allp.select_tris(shed_sel)
    lo, hi = shed.pos.min(axis=0), shed.pos.max(axis=0)
    shed.pos = (shed.pos - [(lo[0] + hi[0]) / 2, lo[1], (lo[2] + hi[2]) / 2]) * S

    # ---- cut the front door: remove the two wall panels behind the painted double door, the leaves and the frame
    tri = manor.idx.reshape(-1, 3)
    P = manor.pos[tri]
    front = (np.abs(P[:, :, 2]).max(axis=1) < 0.13) & (P[:, :, 0].min(axis=1) > -3.03) & (P[:, :, 0].max(axis=1) < 3.03) & \
            (P[:, :, 1].min(axis=1) > 1.2) & (P[:, :, 1].max(axis=1) < 4.32)
    removed = manor.select_tris(front)
    manor = manor.select_tris(~front)
    # wall texture mapping from the removed panels (3 x 3 m plank panel of the atlas)
    panel = (removed.uv[:, 0] > 0.78) & (removed.uv[:, 1] > 0.67)
    left = panel & (removed.pos[:, 0] < 0.01)
    uv_of = _fit_uv(removed.pos[left][:, :2], removed.uv[left])
    b = Builder()

    def panel_quad(x0, x1, y0, y1):
        for px0 in (-3.0, 0.0):                     # two panels; the mapping repeats every 3 m
            a0, a1 = max(x0, px0), min(x1, px0 + 3.0)
            if a1 - a0 < 1e-6:
                continue
            corners = np.array([(a0, y0), (a1, y0), (a1, y1), (a0, y1)])
            uv = uv_of(corners - [px0 + 3.0, 0.0])   # fitted on the left panel (x -3..0)
            b.quad('kit', (a0, y0, 0.0), (a1, y0, 0.0), (a1, y1, 0.0), (a0, y1, 0.0), uvs=uv)
    panel_quad(-3.0, -DOOR_W / 2, F0, F0C)
    panel_quad(DOOR_W / 2, 3.0, F0, F0C)
    panel_quad(-DOOR_W / 2, DOOR_W / 2, F0 + DOOR_H, F0C)
    shell = merge([manor] + b.to_parts(), material='kit')

    # ---- door leaves as separate prefabs (pivot = hinge, bottom)
    du0, dv0, du1, dv1 = 0.269, 0.408, 0.361, 0.554
    leaves = {}
    for name, sign in (('SM_Manor_DoorL', 1.0), ('SM_Manor_DoorR', -1.0)):
        lb = Builder()
        w, h, t = DOOR_W / 2 - 0.02, DOOR_H - 0.02, 0.06
        # leaf spans from the hinge (x = 0) towards the door centre; front (+Z) and back face
        x0, x1 = (0.0, -sign * w) if sign > 0 else (0.0, w)
        xa, xb = min(x0, x1), max(x0, x1)
        uvf = [(du0, dv1), (du1, dv1), (du1, dv0), (du0, dv0)]
        lb.quad('kit', (xa, 0, t / 2), (xb, 0, t / 2), (xb, h, t / 2), (xa, h, t / 2), uvs=uvf)
        lb.quad('kit', (xb, 0, -t / 2), (xa, 0, -t / 2), (xa, h, -t / 2), (xb, h, -t / 2), uvs=uvf)
        edge = [(du0, dv1), (du0 + 0.01, dv1), (du0 + 0.01, dv0), (du0, dv0)]
        lb.quad('kit', (xb, 0, t / 2), (xb, 0, -t / 2), (xb, h, -t / 2), (xb, h, t / 2), uvs=edge)
        lb.quad('kit', (xa, 0, -t / 2), (xa, 0, t / 2), (xa, h, t / 2), (xa, h, -t / 2), uvs=edge)
        lb.quad('kit', (xa, h, t / 2), (xb, h, t / 2), (xb, h, -t / 2), (xa, h, -t / 2), uvs=edge)
        leaves[name] = lb.to_parts()
    return shell, shed, leaves


# =============================================================================================== ground floor
def build_ground_floor(pts):
    a = Arch(UV_SCALE)
    y0, y1 = F0 + 0.012, F0C
    dh = y0 + 2.1                                    # interior door height
    sk = ('wood', 0.14)

    def room(lo, hi, wall='wallpaper', openings=None, floor='floor'):
        a.room((lo[0], y0, lo[1]), (hi[0], y1, hi[1]), wall, floor, 'ceiling', openings, sk)

    # A2 hall
    room((-2.4, -7.9), (2.4, -0.15), openings={'z+': [(-DOOR_W / 2, DOOR_W / 2, y0, F0 + DOOR_H)], 'x-': [(-4.5, -3.5, y0, dh)],
                                                 'x+': [(-4.5, -3.5, y0, dh)], 'z-': [(-0.6, 0.6, y0, dh)]})
    a.doorway('z', -0.15, 0.0, -DOOR_W / 2, DOOR_W / 2, F0, F0 + DOOR_H, 'wood', 'floor')
    # A1 parlour (-x), A3 kitchen (+x)
    room((-5.85, -7.9), (-2.6, -0.15), openings={'x+': [(-4.5, -3.5, y0, dh)]})
    room((2.6, -7.9), (5.85, -0.15), 'plaster', openings={'x-': [(-4.5, -3.5, y0, dh)]})
    a.doorway('x', -2.6, -2.4, -4.5, -3.5, y0, dh, 'wood', 'floor')
    a.doorway('x', 2.4, 2.6, -4.5, -3.5, y0, dh, 'wood', 'floor')
    # B corridor
    room((-5.85, -10.1), (5.85, -8.1), openings={'z+': [(-0.6, 0.6, y0, dh)], 'z-': [(-4.0, -3.0, y0, dh), (1.5, 2.7, y0, dh)],
                                                   'x-': [(-9.6, -8.6, y0, dh)], 'x+': [(-9.6, -8.6, y0, dh)]})
    a.doorway('z', -8.1, -7.9, -0.6, 0.6, y0, dh, 'wood', 'floor')
    # wings
    room((-8.85, -14.85), (-6.05, -6.15), 'plaster', openings={'x+': [(-9.6, -8.6, y0, dh)]})
    room((6.05, -14.85), (8.85, -6.15), 'plaster', openings={'x-': [(-9.6, -8.6, y0, dh)]})
    a.doorway('x', -6.05, -5.85, -9.6, -8.6, y0, dh, 'wood', 'floor')
    a.doorway('x', 5.85, 6.05, -9.6, -8.6, y0, dh, 'wood', 'floor')
    # C1 study, C2 stair hall (floor opening for the cellar stairs)
    room((-5.85, -17.85), (-1.2, -10.3), openings={'z+': [(-4.0, -3.0, y0, dh)]})
    room((-1.0, -17.85), (5.85, -10.3), 'plaster', openings={'z+': [(1.5, 2.7, y0, dh)], 'y-': [(1.2, 4.8, -17.85, -16.75)]})
    a.doorway('z', -10.3, -10.1, -4.0, -3.0, y0, dh, 'wood', 'floor')
    a.doorway('z', -10.3, -10.1, 1.5, 2.7, y0, dh, 'wood', 'floor')

    # ---- railing around the stair opening, collapsed upper stairs
    for x in np.arange(1.2, 4.21, 0.6):
        a.box('wood', (x - 0.03, y0, -16.75), (x + 0.03, y0 + 0.95, -16.69))
    a.box('wood', (1.17, y0 + 0.9, -16.76), (4.25, y0 + 0.98, -16.68))
    a.box('wood', (1.14, y0, -17.85), (1.2, y0 + 0.95, -16.69))
    for k in range(7):                                # broken staircase to the upper floor: debris pile
        a.obox('wood', (-0.2 + 0.25 * k, y0 + 0.12 + 0.19 * k, -11.2), (0.14, 0.02, 0.55), 0, 0)
    a.beam('timber', (-0.6, y0, -12.4), (2.2, y1 - 0.05, -10.9), 0.22)
    a.beam('timber', (0.4, y0 + 0.1, -13.2), (2.8, y0 + 0.5, -11.6), 0.18)
    a.obox('plaster', (1.2, y0 + 0.2, -11.6), (1.0, 0.2, 0.7), 20, 0)

    # ---- furniture
    a.table('wood', -4.2, y0, -4.2, 1.6, 0.9, 0.76, 8)
    a.chair('wood', -4.9, y0, -3.3, 30)
    a.chair('wood', -3.4, y0, -5.2, 200, fallen=True)
    a.shelf('wood', -5.6, y0, -1.6, 1.4, 2.0, 0.35, 90)
    a.box('fabric', (-5.6, y0, -7.4), (-3.6, y0 + 0.45, -6.5))            # sofa base
    a.box('fabric', (-5.6, y0 + 0.45, -7.7), (-3.6, y0 + 0.95, -7.4))
    a.table('wood', 4.2, y0, -2.2, 1.8, 0.8, 0.9, 0)                      # kitchen worktop
    a.shelf('wood', 5.62, y0, -5.6, 1.6, 2.0, 0.4, 270)
    a.box('metal', (2.8, y0, -7.7), (3.8, y0 + 0.9, -6.9))                # stove
    a.cylinder('metal', (3.3, y0 + 0.9, -7.5), 0.09, y1 - y0 - 0.9, 8)
    a.table('wood', -3.6, y0, -15.4, 1.7, 0.85, 0.78, 0)                  # study desk
    a.chair('wood', -3.6, y0, -14.5, 180)
    a.shelf('wood', -5.6, y0, -13.0, 1.8, 2.3, 0.35, 90, 5)
    a.shelf('wood', -5.6, y0, -16.0, 1.8, 2.3, 0.35, 90, 5)
    a.shelf('wood', -8.6, y0, -12.5, 1.6, 2.0, 0.4, 90)
    a.crate('wood', -7.3, y0, -14.0, 0.7, 12)
    a.crate('wood', -7.9, y0, -7.2, 0.6, 40)
    a.barrel('wood', 'metal', 8.2, y0, -13.9)
    a.box('metal', (7.9, y0, -7.4), (8.7, y0 + 0.85, -6.5))               # wash boiler
    a.crate('wood', 0.9, y0, -1.0, 0.55, 20)
    # boarded windows (inside): planks across where the shell has its painted windows
    for x, z0, z1 in ((-5.84, -5.0, -3.4), (5.84, -5.0, -3.4)):
        for k in range(4):
            a.obox('wood', (x + (0.03 if x < 0 else -0.03), y0 + 1.0 + 0.32 * k, (z0 + z1) / 2), (0.015, 0.11, (z1 - z0) / 2), 0, 0)

    pts['lights'] += [
        dict(id='HallLamp', pos=(0.0, y1 - 0.35, -4.0), radius=7.5, intensity=0.55, color=(1.0, 0.82, 0.6), kind='bulb', flicker=1, powered=1),
        dict(id='CorridorLamp', pos=(0.0, y1 - 0.35, -9.1), radius=7.0, intensity=0.4, color=(1.0, 0.8, 0.58), kind='bulb', flicker=2, powered=1),
        dict(id='StairLamp', pos=(3.0, y1 - 0.35, -14.0), radius=7.0, intensity=0.45, color=(1.0, 0.8, 0.58), kind='bulb', flicker=1, powered=1),
    ]
    pts['anchors'].update({
        'FrontDoor': (0.0, F0, 0.0), 'Porch': (0.0, F0, 2.2), 'Hall': (0.0, y0, -4.0), 'Parlour': (-4.2, y0, -4.0), 'Kitchen': (4.2, y0, -4.0),
        'Study': (-3.5, y0, -14.0), 'StudyDesk': (-3.6, y0 + 0.8, -15.4), 'StairHall': (2.5, y0, -13.5), 'CellarStairsTop': (5.3, y0, -17.3),
        'Pantry': (-7.4, y0, -10.5), 'WashRoom': (7.4, y0, -10.5), 'KitchenTable': (4.2, y0 + 0.92, -2.2), 'ParlourTable': (-4.2, y0 + 0.78, -4.2),
        'PantryShelf': (-8.4, y0 + 1.0, -12.5),
    })
    pts['doors'] += [
        dict(id='Front', pos=(0.0, F0, 0.0), dir=(0, 1), width=DOOR_W, height=DOOR_H, kind='double', lock=''),
        dict(id='Study', pos=(-3.5, y0, -10.2), dir=(0, 1), width=1.0, height=2.1, kind='single', lock=''),
        dict(id='CellarStairs', pos=(5.05, y0, -16.75), dir=(0, 1), width=0.0, height=0.0, kind='none', lock=''),
    ]
    return a


# =============================================================================================== cellar
def build_cellar(pts):
    a = Arch(UV_SCALE)
    y0, y1 = K0, K0C
    dh = y0 + 2.0
    # ---- stair shaft (ground floor -> cellar), 22 steps going down towards -x
    steps, rise, run = 22, (F0 + 0.012 - K0) / 22.0, 0.27
    sx0 = 4.8
    sx1 = sx0 - steps * run                                    # -1.14
    land0 = sx1 - 1.5                                          # landing at the bottom
    z0, z1 = -17.85, -16.75
    a.wall('brick', 'z', 1, z0, land0, 4.8, y0, F0)
    a.wall('brick', 'z', -1, z1, land0, 4.8, y0, F0, [(land0 + 0.2, land0 + 1.2, y0, dh)])
    a.wall('brick', 'x', 1, land0, z0, z1, y0, F0)
    a.wall('brick', 'x', -1, 4.8, z0, z1, y0, F0)
    a.wall('plaster', 'y', -1, F0 - 0.2, land0, 1.2, z0, z1)   # underside of the ground floor
    a.wall('flag', 'y', 1, y0, land0, sx1, z0, z1)
    a.stairs('flag', (sx0, F0 + 0.012, (z0 + z1) / 2), 'x-', steps, rise, run, z1 - z0, solid_to=y0, tread='wood')
    # ---- rooms
    vault = {'*': 'brick'}

    def room(lo, hi, openings=None, floor='flag'):
        a.room((lo[0], y0, lo[1]), (hi[0], y1, hi[1]), vault, floor, 'brick', openings)
    # K1 corridor
    room((-2.6, -16.55), (-1.0, -9.0), {'z-': [(land0 + 0.2, land0 + 1.2, y0, dh)], 'x+': [(-14.5, -13.5, y0, dh)], 'x-': [(-13.0, -12.0, y0, dh)],
                                         'z+': [(-2.3, -1.3, y0, dh)]})
    a.doorway('z', -16.75, -16.55, land0 + 0.2, land0 + 1.2, y0, dh, 'brick', 'flag')
    # K2 boiler room (+x), K3 store (-x)
    room((-0.8, -16.35), (5.6, -11.2), {'x-': [(-14.5, -13.5, y0, dh)]})
    a.doorway('x', -1.0, -0.8, -14.5, -13.5, y0, dh, 'brick', 'flag')
    room((-5.6, -16.55), (-2.8, -10.2), {'x+': [(-13.0, -12.0, y0, dh)]}, 'dirt')
    a.doorway('x', -2.8, -2.6, -13.0, -12.0, y0, dh, 'brick', 'flag')
    # K4 vault with the breach in the west wall
    bz0, bz1, bh = -5.6, -3.6, y0 + 2.05
    room((-5.6, -8.8), (5.6, -0.6), {'z-': [(-2.3, -1.3, y0, dh)], 'x-': [(bz0, bz1, y0, bh)]})
    a.doorway('z', -9.0, -8.8, -2.3, -1.3, y0, dh, 'brick', 'flag')
    a.doorway('x', -6.2, -5.6, bz0, bz1, y0, bh, 'brick', 'dirt')
    # back plate: the outside of the cellar wall as seen from the tunnel
    a.wall('brick', 'x', -1, -6.2, -8.6, -0.6, y0 - 1.0, y1 + 0.6, [(bz0, bz1, y0, bh)])
    for px in (-2.0, 2.0):                                     # pillars in the vault
        for pz in (-6.2, -3.2):
            a.box('brick', (px - 0.25, y0, pz - 0.25), (px + 0.25, y1, pz + 0.25), 'xXzZ')
    # ---- props
    for k, (bx, bz) in enumerate(((4.9, -1.3), (4.2, -1.2), (4.9, -2.2), (3.4, -1.3))):
        a.barrel('wood', 'metal', bx, y0, bz, 0.34 + 0.02 * (k % 2))
    a.shelf('wood', 5.38, y0, -5.2, 2.2, 1.9, 0.4, 270, 4)
    a.shelf('wood', 0.0, y0, -0.82, 2.4, 1.9, 0.4, 0, 4)
    a.crate('wood', -4.6, y0, -1.4, 0.8, 15)
    a.crate('wood', -4.5, y0 + 0.8, -1.4, 0.6, 40)
    for k in range(5):                                         # rubble in front of the breach
        a.obox('brick', (-5.0 + 0.25 * (k % 3), y0 + 0.08, -6.3 + 0.5 * k), (0.16, 0.08, 0.11), 30 * k, 0)
    a.box('metal', (2.2, y0, -16.2), (3.8, y0 + 1.7, -15.0))   # boiler
    a.cylinder('metal', (3.0, y0 + 1.7, -15.6), 0.12, y1 - y0 - 1.7, 8)
    a.cylinder('metal', (4.9, y0 + 0.3, -11.6), 0.07, 0.07, 8)
    a.beam('metal', (-0.6, y1 - 0.25, -15.8), (5.4, y1 - 0.25, -15.8), 0.09)
    a.box('metal', (5.3, y0 + 1.1, -13.4), (5.58, y0 + 1.8, -12.8))        # fuse box
    a.shelf('wood', -5.38, y0, -13.0, 2.0, 1.8, 0.4, 90, 3)
    a.crate('wood', -3.4, y0, -15.8, 0.7, 5)
    a.barrel('wood', 'metal', -5.0, y0, -10.9)

    pts['lights'] += [
        dict(id='CellarStair', pos=(0.5, y1 + 0.6, -17.3), radius=6.0, intensity=0.35, color=(1.0, 0.78, 0.5), kind='bulb', flicker=2, powered=1),
        dict(id='CellarCorridor', pos=(-1.8, y1 - 0.3, -12.5), radius=6.0, intensity=0.35, color=(1.0, 0.78, 0.5), kind='bulb', flicker=1, powered=1),
        dict(id='Boiler', pos=(2.4, y1 - 0.3, -13.6), radius=6.5, intensity=0.4, color=(1.0, 0.72, 0.45), kind='bulb', flicker=0, powered=1),
        dict(id='Vault', pos=(0.0, y1 - 0.3, -4.6), radius=8.0, intensity=0.4, color=(1.0, 0.78, 0.5), kind='bulb', flicker=3, powered=1),
    ]
    pts['anchors'].update({
        'CellarStairsBottom': (land0 + 0.7, y0, -17.3), 'CellarCorridor': (-1.8, y0, -12.5), 'BoilerRoom': (2.4, y0, -13.8), 'FuseBox': (5.25, y0 + 1.45, -13.1),
        'CellarStore': (-4.2, y0, -13.4), 'CellarStoreShelf': (-5.2, y0 + 0.9, -13.0), 'Vault': (0.0, y0, -4.6), 'Breach': (-5.0, y0, -4.6),
        'VaultShelf': (0.0, y0 + 0.95, -1.0),
    })
    pts['doors'] += [dict(id='Vault', pos=(-1.8, y0, -8.9), dir=(0, 1), width=1.0, height=2.0, kind='single', lock='CellarKey')]
    pts['shaft'] = dict(x0=land0, x1=4.8, z0=z0, z1=z1)
    return a


# =============================================================================================== tunnels and caves
def _seg_points(path, spacing):
    out = []
    for p, q in zip(path[:-1], path[1:]):
        p, q = np.asarray(p, dtype=np.float64), np.asarray(q, dtype=np.float64)
        n = max(1, int(np.ceil(np.linalg.norm(q - p) / spacing)))
        for k in range(n):
            out.append((p + (q - p) * k / n, q - p))
    return out


def build_underground(pts, with_tomb_chamber=True):
    lo = np.array([-44.0, -29.0, -60.0])
    hi = np.array([48.0, -0.5, 2.0])
    f = Field(lo, hi, cell=0.4, margin=2.0)
    # mine drift (regular), then natural passages (wider, irregular)
    for p, q in zip(MINE[:-1], MINE[1:]):
        f.tunnel(p, q, 1.75, height=2.7)
    f.tunnel((-4.6, K0, -4.6), MINE[0], 1.55, height=2.3)       # stub through the cellar wall (clipped below)
    for k, (p, q) in enumerate(zip(CAVE[:-1], CAVE[1:])):
        f.tunnel(p, q, 2.1 + 0.5 * (k % 2), 2.4 + 0.4 * ((k + 1) % 2), height=3.4 + 0.5 * (k % 3))
    for p, q in zip(STORE[:-1], STORE[1:]):
        f.tunnel(p, q, 1.5, height=2.4)
    f.cavern((-38.0, -5.0, -11.0), (4.2, 2.6, 4.2), floor_y=-6.8)
    # the great cavern with two side lobes
    f.cavern(CAVERN_C, CAVERN_R, floor_y=CAVERN_FLOOR)
    f.cavern((17.0, -20.5, -14.0), (7.0, 4.5, 7.0), floor_y=CAVERN_FLOOR + 0.3)
    f.cavern((34.0, -20.0, -27.0), (7.5, 5.0, 6.5), floor_y=CAVERN_FLOOR - 0.2)
    f.tunnel(CAVE[-1], (20.0, CAVERN_FLOOR, -22.0), 2.6, height=3.8)
    for p, q in zip(EXIT[:-1], EXIT[1:]):
        f.tunnel(p, q, 2.2, height=3.6)
    f.tunnel((24.0, CAVERN_FLOOR, -24.0), EXIT[0], 2.6, height=3.8)
    for p, q in HIDES:                                          # crawl spaces: too small for the big creature
        f.tunnel(p, q, 0.82, height=1.4)
        f.cavern((q[0], q[1] + 0.7, q[2]), (1.5, 0.95, 1.5), floor_y=q[1])
    if with_tomb_chamber:
        f.box(DIG_LO, DIG_HI)
        f.tunnel(EXIT[-1], (20.0, -25.6, -50.0), 2.2, height=3.4)
    f.roughen(0.26, 2.4, 7)
    # features inside the cavern
    rng = np.random.default_rng(12)
    for _ in range(26):
        ang, rad = rng.uniform(0, 2 * np.pi), rng.uniform(3.0, 11.0)
        x, z = CAVERN_C[0] + np.cos(ang) * rad, CAVERN_C[2] + np.sin(ang) * rad * 0.9
        if rng.random() < 0.55:
            f.stalagmite((x, CAVERN_FLOOR - 0.3, z), rng.uniform(1.0, 3.4), rng.uniform(0.35, 0.8))
        else:
            f.stalagmite((x, CAVERN_C[1] + CAVERN_R[1] * 0.9, z), rng.uniform(1.2, 3.5), rng.uniform(0.3, 0.6), hanging=True)
    for bx, bz, br in ((21.0, -16.0, 1.3), (30.0, -24.0, 1.6), (28.0, -13.0, 1.1), (33.5, -19.0, 1.4), (18.5, -26.5, 1.0)):
        f.boulder((bx, CAVERN_FLOOR + br * 0.35, bz), br)

    def clip(centre, quad):
        # inside the cellar wall (the stub) and inside the tomb's stair pit
        m = quad[:, :, 0].max(axis=1) > -6.22
        m &= (centre[:, 2] > -9.0) & (centre[:, 2] < -0.5) & (centre[:, 1] > -5.0)
        if with_tomb_chamber:
            pit = (centre[:, 0] > PIT[0]) & (centre[:, 0] < PIT[1]) & (centre[:, 2] > PIT[2]) & (centre[:, 2] < PIT[3]) & (centre[:, 1] < -24.95)
            m |= pit
        return m
    chunks = f.to_parts('rock', UV_SCALE['rock'], chunk=26.0, clip=clip)
    # floors that are close to level get the dirt material
    out = {}
    for key, part in chunks.items():
        tri = part.idx.reshape(-1, 3)
        up = part.nrm[tri].mean(axis=1)[:, 1] > 0.72
        ps = [part.select_tris(~up)]
        if up.any():
            fl = part.select_tris(up)
            fl.material = 'dirt'
            fl.uv = fl.pos[:, [0, 2]] * UV_SCALE['dirt'] * np.array([1.0, -1.0])
            ps.append(fl)
        out[key] = [p for p in ps if p.tris]

    # ---- timbering, rails and props (one extra mesh)
    a = Arch(UV_SCALE)
    for k, (p, d) in enumerate(_seg_points(MINE, 2.8)):
        if k == 0:
            continue
        d = d / np.linalg.norm(d)
        side = np.array([d[2], 0.0, -d[0]])
        yaw = np.degrees(np.arctan2(d[0], d[2]))
        lean = 4.0 * np.sin(k * 1.7)
        for s in (-1, 1):
            a.obox('timber', p + side * s * 1.45 + [0, 1.2, 0], (0.11, 1.25, 0.11), yaw + lean, 0)
        a.obox('timber', p + [0, 2.45, 0], (1.65, 0.12, 0.12), yaw + 90 - 90, 0) if False else a.obox('timber', p + [0, 2.45, 0], (0.12, 0.12, 1.65), yaw + 90, 0)
        if k % 2 == 0:
            a.obox('timber', p + [0, 2.6, 0] + d * 1.3, (0.9, 0.03, 1.3), yaw, 0)
    for p, d in _seg_points(MINE, 0.9):                         # sleepers of an old track
        d = d / np.linalg.norm(d)
        a.obox('timber', p + [0, 0.05, 0], (0.75, 0.05, 0.09), np.degrees(np.arctan2(d[0], d[2])), 0)
    sx, sy, sz = STORE[-1]
    a.crate('wood', sx - 2.2, sy, sz + 1.2, 0.8, 10)
    a.crate('wood', sx - 2.8, sy, sz - 0.2, 0.7, 50)
    a.crate('wood', sx - 2.3, sy + 0.8, sz + 1.1, 0.6, 30)
    a.barrel('wood', 'metal', sx - 0.5, sy, sz - 2.6)
    a.barrel('wood', 'metal', sx - 1.3, sy, sz - 2.9)
    a.table('timber', sx + 0.6, sy, sz + 2.4, 1.5, 0.7, 0.8, 15)
    a.shelf('timber', sx - 3.1, sy, sz + 2.6, 1.6, 1.7, 0.4, 130, 3)
    # bones and a nest in the cavern (the creature's lair)
    lair = np.array([31.0, CAVERN_FLOOR + 0.05, -17.0])
    for k in range(18):
        ang = k * 2.4
        r = 0.6 + 0.12 * k
        a.obox('plaster', lair + [np.cos(ang) * r, 0.05, np.sin(ang) * r], (0.035, 0.035, 0.22 + 0.02 * (k % 4)), np.degrees(ang) * 3.1, 0)
    for k in range(7):
        ang = k * 0.9
        a.beam('timber', lair + [np.cos(ang) * 2.2, 0.0, np.sin(ang) * 2.2], lair + [np.cos(ang + 0.5) * 1.2, 0.5, np.sin(ang + 0.5) * 1.2], 0.12)

    # ---- gameplay data
    lights = []
    for k, i in enumerate((1, 2, 3)):
        p = MINE[i]
        lights.append(dict(id='Mine%d' % k, pos=(p[0], p[1] + 2.15, p[2]), radius=7.0, intensity=0.3, color=(1.0, 0.7, 0.4), kind='lantern', flicker=1, powered=0))
    lights.append(dict(id='Store', pos=(sx - 0.3, sy + 1.9, sz + 0.5), radius=7.0, intensity=0.35, color=(1.0, 0.7, 0.4), kind='lantern', flicker=1, powered=0))
    lights.append(dict(id='Glow1', pos=(CAVERN_C[0] + 6, CAVERN_FLOOR + 0.5, CAVERN_C[2] - 6), radius=9.0, intensity=0.12, color=(0.3, 0.9, 0.7), kind='none', flicker=0, powered=0))
    lights.append(dict(id='Glow2', pos=(CAVERN_C[0] - 7, CAVERN_FLOOR + 0.5, CAVERN_C[2] + 5), radius=9.0, intensity=0.1, color=(0.3, 0.8, 0.9), kind='none', flicker=0, powered=0))
    pts['lights'] += lights
    pts['anchors'].update({
        'MineStart': MINE[0], 'MineJunction': MINE[3], 'StoreChamber': (sx, sy, sz), 'StoreTable': (sx + 0.6, sy + 0.82, sz + 2.4), 'CaveStart': CAVE[0],
        'CaveMid': CAVE[3], 'CavernEntry': CAVE[-1], 'CavernCentre': (CAVERN_C[0], CAVERN_FLOOR, CAVERN_C[2]), 'Lair': tuple(lair),
        'CavernExit': EXIT[0], 'DigChamber': EXIT[-1],
    })
    pts['hides'] = [dict(pos=q, entry=p) for p, q in HIDES]

    # navigation graph for the big creature (nodes on the floor, edges = walkable connections)
    nodes, edges = [], []

    def chain(points, link_to=None, step=5.0):
        prev = link_to
        for i, p in enumerate(points):
            if i > 0:
                a0 = np.asarray(points[i - 1], dtype=np.float64)
                n = int(np.linalg.norm(np.asarray(p) - a0) // step)
                for k in range(1, n + 1):
                    mid = a0 + (np.asarray(p) - a0) * k / (n + 1)
                    nodes.append(tuple(float(v) for v in mid))
                    edges.append((prev, len(nodes) - 1))
                    prev = len(nodes) - 1
            nodes.append(tuple(float(v) for v in p))
            if prev is not None:
                edges.append((prev, len(nodes) - 1))
            prev = len(nodes) - 1
        return prev
    end_mine = chain(MINE)
    junction = min(range(len(nodes)), key=lambda i: np.linalg.norm(np.array(nodes[i]) - MINE[3]))
    chain(STORE[1:], junction)
    end_cave = chain(CAVE[1:], end_mine)
    cc = (20.0, CAVERN_FLOOR, -22.0)
    c0 = chain([cc], end_cave)
    ring = []
    for k in range(6):
        ang = k * np.pi / 3
        nodes.append((CAVERN_C[0] + np.cos(ang) * 7.5, CAVERN_FLOOR, CAVERN_C[2] + np.sin(ang) * 6.5))
        ring.append(len(nodes) - 1)
    nodes.append((CAVERN_C[0], CAVERN_FLOOR, CAVERN_C[2]))
    mid = len(nodes) - 1
    for k in range(6):
        edges.append((ring[k], ring[(k + 1) % 6]))
        edges.append((ring[k], mid))
    edges.append((c0, min(ring, key=lambda i: np.linalg.norm(np.array(nodes[i]) - cc))))
    edges.append((c0, mid))
    e0 = chain([(24.0, CAVERN_FLOOR, -24.0)] + EXIT, mid)
    pts['nav_nodes'], pts['nav_edges'], pts['nav_exit'] = nodes, edges, e0
    pts['spawns'] = dict(Cyclops=tuple(lair + [-1.5, 0.0, 1.0]))
    return out, a


# =============================================================================================== assembly
def build(src_dir, out_dir, with_tomb_chamber=True):
    mats = _materials(src_dir)
    pts = {'lights': [], 'anchors': {}, 'doors': [], 'zones': {}}
    shell, shed, leaves = build_shell(src_dir)
    ground = build_ground_floor(pts)
    cellar = build_cellar(pts)
    caves, props = build_underground(pts, with_tomb_chamber)
    out = GlbFile(mats, CREDIT)
    manifest = {}

    def add(name, parts, group):
        parts = [p for p in parts if p.tris]
        out.add_mesh(name, parts)
        manifest[name] = dict(stats(parts), kind='static', file='Manor.glb', group=group)
    add('SM_Manor_Shell', [shell], 'manor')
    add('SM_Manor_Ground', ground.parts(), 'manor')
    add('SM_Manor_Cellar', cellar.parts(), 'manor')
    for name, parts in leaves.items():
        add(name, parts, 'door')
    shed.material = 'kit'
    add('SM_Shed', [shed], 'prefab')
    for (ix, iz), parts in sorted(caves.items()):
        add('SM_UG_Cave_%s_%s' % (str(ix).replace('-', 'm'), str(iz).replace('-', 'm')), parts, 'underground')
    add('SM_UG_Props', props.parts(), 'underground')
    out.save(os.path.join(out_dir, 'Manor.glb'))
    pts['zones'] = {
        'House': dict(lo=(-9.0, F0 - 0.2, -18.0), hi=(9.0, F0C + 0.3, 0.2)),
        'Cellar': dict(lo=(-6.3, K0 - 0.3, -18.0), hi=(5.8, K0C + 0.2, -0.4)),
        'Underground': dict(lo=(-44.0, -31.0, -62.0), hi=(48.0, -0.7, 2.0)),
    }
    pts['pad'] = dict(centre=(-5.0, 0.0, -27.0), radius=64.0)
    return manifest, pts


if __name__ == '__main__':
    import sys
    import json
    m, p = build(sys.argv[1], sys.argv[2])
    print(json.dumps(m, indent=1)[:3000])
