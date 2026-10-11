"""
The town of Hollowmere (level L_Town, streamed into the hideout and every job map).

Plan (Blender metres, +X east, +Y north; the hideout basement is at the origin, street level 2.45):

                      Church Road (y 98.5..106.5)      St. Agnes church, cemetery, rectory (north)
   Marrow St          +----------------------------+          Ashgrove Lane
   (x -52..-44)       |        Founders' Park      |          (x 44..52)
   Marrow House (W)   |  houses      museum  houses|          Vance house = 14 Ashgrove Ln (E)
                      +----------------------------+
   ===== Main Street (y 16.5..24.5) ==============================================
     Brannigan & Sons | Marlowe's Laundromat (hideout below) + lot | municipal parking

Buildings that can't be entered say so: shops are closed (painted interiors behind the glass,
locked doors are part of the facade), neighbours' doors are locked HHDoorActors that never open.
"""
import math
import random

import build_hideout as BH
import houses as HS
import placement as PL
import worldkit as K
from hhblend import Builder
from worldkit import T_EXT, TRIM

TZ = 2.45                 # road surface
CURB = 0.15
SW = TZ + CURB            # sidewalks, yards, shop floors

MAIN = (16.5, 24.5)       # Main Street road (y)
ASH = (44.0, 52.0)        # Ashgrove Lane road (x)
MAR = (-52.0, -44.0)      # Marrow Street road (x)
CHURCH = (98.5, 106.5)    # Church Road road (y)
XMIN, XMAX = -88.0, 88.0
YMIN, YMAX = -24.0, 152.0
WALK = 2.5                # sidewalk width

NEIGHBOUR_TAG = 'HH_House_Neighbour'
HOUSE_TAGS = {'vance': 'HH_House_Vance', 'marrow': 'HH_House_Marrow', 'rectory': 'HH_House_Rectory'}

# House lots used by the target-house modules (origin = middle of the front facade at yard level).
VANCE_ORIGIN = (61.0, 57.0, SW)       # faces west (towards Ashgrove Lane)
MARROW_ORIGIN = (-62.0, 83.0, SW)     # faces east (towards Marrow Street)
RECTORY_ORIGIN = (21.0, 116.0, SW)    # faces south (towards Church Road)

# Basements cut out of the yard (inner faces of the foundation walls).
VANCE_BASEMENT = (VANCE_ORIGIN[0] + 0.25, VANCE_ORIGIN[1] - 5.25, VANCE_ORIGIN[0] + 8.75, VANCE_ORIGIN[1] + 5.25)

rng = random.Random(1987)


# =======================================================================================
# Ground: roads, sidewalks with curbs, blocks of land

def _road(b, x0, y0, x1, y1):
    b.box_minmax((x0, y0, TZ - 0.4), (x1, y1, TZ), 'MI_Asphalt')


def _block(b, x0, y0, x1, y1, walks, land='MI_Grass', land_z=SW, holes=()):
    """A block of land between streets. walks: edges ('s', 'n', 'w', 'e') that get a sidewalk with a
    curb towards the road (no overlapping faces: strips and corner curbs tile exactly).
    Returns the interior rectangle."""
    c = 0.15
    hw, he = 'w' in walks, 'e' in walks
    for edge in ('s', 'n'):
        if edge not in walks:
            continue
        if edge == 's':
            yc0, yc1, yw0, yw1 = y0, y0 + c, y0 + c, y0 + WALK
        else:
            yc0, yc1, yw0, yw1 = y1 - c, y1, y1 - WALK, y1 - c
        b.box_minmax((x0, yc0, TZ - 0.3), (x1, yc1, SW), 'MI_Curb', bevel=0.01)
        b.box_minmax((x0 + (c if hw else 0), yw0, TZ - 0.3), (x1 - (c if he else 0), yw1, SW), 'MI_Sidewalk')
        if hw:
            b.box_minmax((x0, yw0, TZ - 0.3), (x0 + c, yw1, SW), 'MI_Curb', bevel=0.01)
        if he:
            b.box_minmax((x1 - c, yw0, TZ - 0.3), (x1, yw1, SW), 'MI_Curb', bevel=0.01)
    ys0 = y0 + WALK if 's' in walks else y0
    ys1 = y1 - WALK if 'n' in walks else y1
    if hw:
        b.box_minmax((x0, ys0, TZ - 0.3), (x0 + c, ys1, SW), 'MI_Curb', bevel=0.01)
        b.box_minmax((x0 + c, ys0, TZ - 0.3), (x0 + WALK, ys1, SW), 'MI_Sidewalk')
    if he:
        b.box_minmax((x1 - c, ys0, TZ - 0.3), (x1, ys1, SW), 'MI_Curb', bevel=0.01)
        b.box_minmax((x1 - WALK, ys0, TZ - 0.3), (x1 - c, ys1, SW), 'MI_Sidewalk')
    ix0 = x0 + WALK if hw else x0
    ix1 = x1 - WALK if he else x1
    if land:
        for r in _subtract((ix0, ys0, ix1, ys1), holes):
            b.box_minmax((r[0], r[1], land_z - 0.6), (r[2], r[3], land_z), land)
    return ix0, ys0, ix1, ys1


def _subtract(rect, holes):
    """Split a rectangle (x0, y0, x1, y1) into pieces that leave out the hole rectangles."""
    pieces = [rect]
    for hx0, hy0, hx1, hy1 in holes:
        out = []
        for x0, y0, x1, y1 in pieces:
            if hx1 <= x0 or hx0 >= x1 or hy1 <= y0 or hy0 >= y1:
                out.append((x0, y0, x1, y1))
                continue
            if hy0 > y0:
                out.append((x0, y0, x1, hy0))
            if hy1 < y1:
                out.append((x0, hy1, x1, y1))
            ya, yb = max(y0, hy0), min(y1, hy1)
            if hx0 > x0:
                out.append((x0, ya, hx0, yb))
            if hx1 < x1:
                out.append((hx1, ya, x1, yb))
        pieces = out
    return pieces


def _paint(b, x0, y0, x1, y1, m='MI_Road_Paint_White'):
    b.box_minmax((x0, y0, TZ), (x1, y1, TZ + 0.004), m)


def _dashes(b, axis, a0, a1, c, length=3.0, gap=6.0, w=0.12, m='MI_Road_Paint_White'):
    a = a0
    while a < a1 - 0.5:
        e = min(a + length, a1)
        if axis == 'X':
            _paint(b, a, c - w / 2, e, c + w / 2, m)
        else:
            _paint(b, c - w / 2, a, c + w / 2, e, m)
        a += length + gap


def _crosswalk(b, axis, a0, a1, c0, c1):
    """Zebra across a road: stripes along 'axis' direction between a0..a1, spanning c0..c1."""
    n = int((a1 - a0) / 1.0)
    for i in range(n):
        a = a0 + 0.25 + i * 1.0
        if axis == 'X':
            _paint(b, a, c0, a + 0.5, c1)
        else:
            _paint(b, c0, a, c1, a + 0.5)


def streets():
    # --- Main Street (with parking lanes) ----------------------------------------------------
    main = Builder('SM_Town_MainStreet')
    _road(main, XMIN, MAIN[0], XMAX, MAIN[1])
    yc = (MAIN[0] + MAIN[1]) / 2
    for dy in (-0.09, 0.09):
        _paint(main, XMIN, yc + dy - 0.05, XMAX, yc + dy + 0.05, 'MI_Road_Paint_Yellow')
    for y in (MAIN[0] + 2.2, MAIN[1] - 2.2):          # parking lane lines
        _dashes(main, 'X', XMIN, XMAX, y, 5.5, 0.6, 0.1)
    for x0, x1 in (MAR, ASH):
        _crosswalk(main, 'X', x0 + 0.5, x1 - 0.5, MAIN[1] - 3.2, MAIN[1] - 0.6)
    PL.static_m(PL.unique(main, nanite=True), K.mat(), 'Ground')

    # --- Side streets ------------------------------------------------------------------------
    ash = Builder('SM_Town_AshgroveLane')
    _road(ash, ASH[0], MAIN[1], ASH[1], CHURCH[1])
    _dashes(ash, 'Y', MAIN[1] + 4, CHURCH[0] - 2, (ASH[0] + ASH[1]) / 2, 3.0, 6.0, 0.12, 'MI_Road_Paint_Yellow')
    _paint(ash, ASH[0] + 0.2, MAIN[1] + 1.0, (ASH[0] + ASH[1]) / 2, MAIN[1] + 1.4)        # stop line
    _paint(ash, (ASH[0] + ASH[1]) / 2, CHURCH[0] - 1.4, ASH[1] - 0.2, CHURCH[0] - 1.0)
    PL.static_m(PL.unique(ash, nanite=True), K.mat(), 'Ground')

    mar = Builder('SM_Town_MarrowStreet')
    _road(mar, MAR[0], MAIN[1], MAR[1], CHURCH[1])
    _paint(mar, (MAR[0] + MAR[1]) / 2, MAIN[1] + 1.0, MAR[1] - 0.2, MAIN[1] + 1.4)
    PL.static_m(PL.unique(mar, nanite=True), K.mat(), 'Ground')

    ch = Builder('SM_Town_ChurchRoad')
    _road(ch, MAR[1], CHURCH[0], ASH[0], CHURCH[1])
    _dashes(ch, 'X', MAR[1] + 3, ASH[0] - 3, (CHURCH[0] + CHURCH[1]) / 2, 3.0, 6.0, 0.12, 'MI_Road_Paint_Yellow')
    _crosswalk(ch, 'Y', CHURCH[0] + 0.5, CHURCH[1] - 0.5, -8.0, -5.4)
    PL.static_m(PL.unique(ch, nanite=True), K.mat(), 'Ground')

    # Dead-end stubs north of Church Road on both side streets are part of block N's sidewalk.

    # --- Blocks --------------------------------------------------------------------------------
    nw = Builder('SM_Town_Block_NW')
    _block(nw, XMIN, MAIN[1], MAR[0], CHURCH[1], 'se')
    PL.static_m(PL.unique(nw, nanite=True), K.mat(), 'Ground')

    nc = Builder('SM_Town_Block_NC')
    _block(nc, MAR[1], MAIN[1], ASH[0], CHURCH[0], 'snwe')
    PL.static_m(PL.unique(nc, nanite=True), K.mat(), 'Ground')

    ne = Builder('SM_Town_Block_NE')
    _block(ne, ASH[1], MAIN[1], XMAX, CHURCH[1], 'sw', holes=[VANCE_BASEMENT])
    PL.static_m(PL.unique(ne, nanite=True), K.mat(), 'Ground')

    n = Builder('SM_Town_Block_N')
    _block(n, XMIN, CHURCH[1], XMAX, YMAX, 's')
    PL.static_m(PL.unique(n, nanite=True), K.mat(), 'Ground')


# =======================================================================================
# South side of Main Street: Brannigan & Sons, the laundromat lot, municipal parking

GARAGE_DOOR = (-5.2, -2.0)
STAIR_DOOR = (5.94, 6.86, 2.2, 4.26)
LAUNDRY = (-7.6, 7.6, -10.0, 5.6)        # building footprint (x0, x1, y0, y1)


def south_side():
    g = Builder('SM_Town_Block_S')
    y_walk0 = MAIN[0] - WALK                 # 14.0
    # Sidewalk with curb, except the two flush lot entrances.
    flush = [(-16.0, 16.0), (19.0, 27.0)]
    cuts = sorted({XMIN, XMAX, *[a for f in flush for a in f]})
    for a, c in zip(cuts, cuts[1:]):
        is_flush = any(f[0] <= a and c <= f[1] for f in flush)
        if is_flush:
            g.box_minmax((a, y_walk0, TZ - 0.3), (c, MAIN[0], TZ + 0.02), 'MI_Sidewalk')
        else:
            g.box_minmax((a, MAIN[0] - 0.15, TZ - 0.3), (c, MAIN[0], SW), 'MI_Curb', bevel=0.01)
            g.box_minmax((a, y_walk0, TZ - 0.3), (c, MAIN[0] - 0.15, SW), 'MI_Sidewalk')
    # Land behind (south of) the sidewalk, outside the two lots.
    g.box_minmax((XMIN, YMIN, SW - 0.6), (-16.15, y_walk0, SW), 'MI_Dirt')
    g.box_minmax((-16.15, YMIN, SW - 0.6), (16.15, -14.15, SW), 'MI_Dirt')
    g.box_minmax((16.15, YMIN, SW - 0.6), (17.85, y_walk0, SW), 'MI_Grass')
    g.box_minmax((17.85, YMIN, SW - 0.6), (40.15, -6.15, SW), 'MI_Grass')
    g.box_minmax((40.15, YMIN, SW - 0.6), (XMAX, y_walk0, SW), 'MI_Grass')
    # Laundromat lot (asphalt at road level) with curbs around it.
    lot = [(-16.0, -14.0, -7.6, y_walk0), (7.6, -14.0, 16.0, y_walk0), (-7.6, -14.0, 7.6, LAUNDRY[2]),
           (-7.6, LAUNDRY[3], -5.65, y_walk0), (-1.55, LAUNDRY[3], 7.6, y_walk0), (-5.65, 12.0, -1.55, y_walk0)]
    for x0, y0, x1, y1 in lot:
        g.box_minmax((x0, y0, TZ - 0.4), (x1, y1, TZ), 'MI_Asphalt')
    g.box_minmax((STAIR_DOOR[0], 5.25, TZ - 0.4), (STAIR_DOOR[1], LAUNDRY[3], TZ), 'MI_Concrete_Raw')
    for x0, y0, x1, y1 in ((-16.15, -14.15, -16.0, y_walk0), (16.0, -14.15, 16.15, y_walk0),
                           (-16.15, -14.15, 16.15, -14.0)):
        g.box_minmax((x0, y0, TZ - 0.3), (x1, y1, SW), 'MI_Curb', bevel=0.01)
    # Parking stripes in the laundromat lot (west side) and the municipal lot.
    for i in range(5):
        y = -8.0 + i * 2.7
        g.box_minmax((-16.0, y, TZ), (-11.5, y + 0.1, TZ + 0.004), 'MI_Road_Paint_White')
    # Municipal parking x 18..40, y -6..14.
    g.box_minmax((18.0, -6.0, TZ - 0.4), (40.0, y_walk0, TZ), 'MI_Asphalt')
    for x0, y0, x1, y1 in ((17.85, -6.15, 18.0, y_walk0), (40.0, -6.15, 40.15, y_walk0), (17.85, -6.15, 40.15, -6.0)):
        g.box_minmax((x0, y0, TZ - 0.3), (x1, y1, SW), 'MI_Curb', bevel=0.01)
    for i in range(8):
        x = 19.5 + i * 2.7
        g.box_minmax((x, -6.0, TZ), (x + 0.1, -1.0, TZ + 0.004), 'MI_Road_Paint_White')
        g.box_minmax((x, 6.0, TZ), (x + 0.1, 11.0, TZ + 0.004), 'MI_Road_Paint_White')
    # The ramp down to the hideout's garage door, between retaining walls.
    ground_z = TZ
    for i in range(12):
        ya = 5.6 + (12.0 - 5.6) * i / 12
        yb = 5.6 + (12.0 - 5.6) * (i + 1) / 12
        za = ground_z * i / 12
        zb = ground_z * (i + 1) / 12
        g.box_minmax((-5.4, ya, -0.3), (-1.8, yb, (za + zb) / 2), 'MI_Asphalt')
    for x0, x1 in ((-5.65, -5.4), (-1.8, -1.55)):
        g.box_minmax((x0, 5.25, -0.3), (x1, 12.0, ground_z + 0.45), 'MI_Concrete_Raw')
    PL.static_m(PL.unique(g, nanite=True), K.mat(), 'Ground')

    rail = Builder('SM_Town_RampRail')
    for x in (-5.525, -1.675):
        pts = [(x, 5.7, ground_z + 0.45), (x, 12.0, ground_z + 0.45)]
        rail.tube([(p[0], p[1], p[2] + 0.95) for p in pts], 0.025, 'MI_Metal_Rust', segs=8)
        rail.tube([(p[0], p[1], p[2] + 0.5) for p in pts], 0.018, 'MI_Metal_Rust', segs=8)
        for i in range(5):
            y = 5.8 + i * 1.5
            rail.cyl(0.022, 0.95, (x, y, ground_z + 0.45 + 0.475), 'MI_Metal_Rust', segs=8)
    PL.static_m(PL.unique(rail, collision='box'), K.mat(), 'Town')

    laundromat()
    brannigan()
    # The hideout's garage door and basement windows belong to the street front (seen from both
    # sides; the job maps stream the town without the hideout).
    PL.place('SM_RollUpDoor', (-3.6, 5.0, 0), collection='Laundromat')
    PL.place('SM_Window_Basement', (1.5, 5.125, 2.8), collection='Laundromat')
    PL.place('SM_Window_Basement', (-7.125, 3.0, 2.8), rot=90, collection='Laundromat')

    # Municipal lot furniture.
    PL.place('SM_Sign_Parking', (18.6, 13.3, SW), rot=0)
    for x in (18.4, 39.6):
        PL.place('SM_Bollard', (x, 13.2, TZ))
    for x, y, car, r in ((22.2, -3.5, 'SM_Car_Sedan_Silver', 180), (30.3, 8.5, 'SM_Car_Hatch_Green', 0),
                         (35.7, -3.6, 'SM_Car_Pickup_Black', 180)):
        PL.place(car, (x, y, TZ), rot=r)
    PL.place('SM_Car_Hatch_Blue', (-13.7, -4.0, TZ), rot=-90)
    PL.place('SM_Bin_Metal', (14.8, -12.5, TZ))
    PL.place('SM_Bin_Metal', (15.3, -11.8, TZ), rot=40)
    PL.place('SM_Bin_Wheelie', (9.0, -12.9, TZ), rot=180)
    PL.place('SM_Bin_Wheelie', (10.0, -12.9, TZ), rot=185)
    for x0 in range(-16, 40, 3):
        PL.place('SM_Fence_Chain', (x0, -14.6, SW), collection='Fences')


def laundromat():
    """Marlowe's Laundromat: the brick building above the hideout. Raised ground floor (shop,
    closed at night), apartment above, flat roof. The stair door from the basement opens onto the lot."""
    x0, x1, y0, y1 = LAUNDRY
    top = 10.2
    bld = K.Building('Town_Laundromat', K.mat())
    b, t = bld.shell, bld.trim
    north = [
        dict(kind='hole', a0=GARAGE_DOOR[0], a1=GARAGE_DOOR[1], z0=0.0, z1=2.9),
        dict(kind='hole', a0=0.9, a1=2.1, z0=2.5, z1=3.1),
        dict(kind='hole', a0=STAIR_DOOR[0], a1=STAIR_DOOR[1], z0=TZ, z1=STAIR_DOOR[3]),
        dict(kind='shop', a0=-7.0, a1=-2.4, z0=3.75, z1=6.0, mullions=2, backdrop='shop'),
        dict(kind='shop', a0=-1.4, a1=2.9, z0=3.75, z1=6.0, mullions=2, backdrop='shop'),
        dict(kind='door', a0=3.45, a1=4.37, z0=3.75, z1=5.81, trim='MI_Trim_Dark'),
        dict(kind='window', a0=5.9, a1=6.9, z0=4.9, z1=6.0, trim='MI_Trim_Dark', backdrop='dark', style='picture'),
    ]
    for i, x in enumerate((-6.0, -3.6, -1.2, 1.2, 3.6, 6.0)):
        north.append(dict(kind='window', a0=x - 0.5, a1=x + 0.5, z0=7.85, z1=9.4, trim='MI_Trim_Dark',
                          backdrop='curtain', glow=True, lites=(1, 1),
                          curtain='MI_Fabric_Curtain' if i % 3 == 0 else 'MI_Fabric_Curtain_Light'))
    west = [dict(kind='hole', a0=2.4, a1=3.6, z0=2.5, z1=3.1)]
    for y in (-7.5, -3.5, 0.5):
        west.append(dict(kind='window', a0=y - 0.6, a1=y + 0.6, z0=4.3, z1=6.0, trim='MI_Trim_Dark', backdrop='dark',
                         style='picture', mullions=1))
    for y in (-8.0, -5.0, -2.0, 1.0, 4.0):
        west.append(dict(kind='window', a0=y - 0.5, a1=y + 0.5, z0=7.85, z1=9.4, trim='MI_Trim_Dark',
                         backdrop='curtain', glow=True, lites=(1, 1)))
    east = []
    for y in (-7.0, -3.0, 1.0):
        east.append(dict(kind='window', a0=y - 0.5, a1=y + 0.5, z0=7.85, z1=9.4, trim='MI_Trim_Dark',
                         backdrop='curtain', glow=True, lites=(1, 1)))
    east.append(dict(kind='window', a0=-5.5, a1=-4.5, z0=4.4, z1=5.9, trim='MI_Trim_Dark', backdrop='dark'))
    south = [dict(kind='door', a0=-0.46, a1=0.46, z0=TZ, z1=TZ + 2.06, trim='MI_Trim_Dark')]
    for x in (-5.0, 4.0):
        south.append(dict(kind='window', a0=x - 0.5, a1=x + 0.5, z0=7.85, z1=9.4, trim='MI_Trim_Dark',
                          backdrop='curtain', glow=True, lites=(1, 1)))

    def run(axis, a0, a1, c_out, c_in, ops, z_lo=TZ - 0.05):
        inward = 1 if c_in > c_out else -1
        K.wall(b, axis, a0, a1, c_out, c_in, z_lo, top, ops, mat_c0='MI_Brick', mat_c1='MI_Plaster',
               split=c_out + inward * 0.1)
        for o in ops:
            if o['kind'] != 'hole':
                bld.add_opening_trim(axis, o, c_out, c_in, inside_finish=False)
        # Stone string course and cornice.
        for z0, z1, p in ((3.55, 3.75, 0.05), (6.95, 7.15, 0.06), (top - 0.35, top - 0.1, 0.12)):
            K.wall(t, axis, a0 - (0.12 if axis == 'X' else 0), a1 + (0.12 if axis == 'X' else 0), c_out,
                   c_out - inward * p, z0, z1, [o for o in ops if o['z0'] < z1 and o['z1'] > z0], mat_c0='MI_Concrete_Raw')

    run('X', x0, x1, y1, y1 - 0.35, north, z_lo=TZ)
    run('X', x0, x1, y0, y0 + 0.35, south)
    run('Y', y0 + 0.35, y1 - 0.35, x0, x0 + 0.35, west, z_lo=TZ)
    run('Y', y0 + 0.35, y1 - 0.35, x1, x1 - 0.35, east)
    # Roof, parapet, interior dims behind the curtains.
    K.flat_roof(b, x0, x1, y0, y1, top, parapet=0.7, mat_wall='MI_Brick')
    b.box_minmax((x0 + 0.35, y0 + 0.35, 7.0), (x1 - 0.35, y1 - 0.35, 7.2), 'MI_Wood_Floor_Dark')
    # Stoop to the shop door.
    for i in range(7):
        z = TZ + (i + 1) * (3.75 - TZ) / 7
        b.box_minmax((3.2, y1, TZ - 0.3), (4.62, y1 + 2.0 - i * 0.27, z), 'MI_Concrete_Raw')
    K.railing(t, [(3.15, y1 + 2.0, TZ), (3.15, y1 + 0.1, 3.75)], height=0.9, m_post='MI_Metal_Black',
              m_rail='MI_Metal_Black', baluster_mat='MI_Metal_Black', balusters=0.5)
    K.railing(t, [(4.67, y1 + 2.0, TZ), (4.67, y1 + 0.1, 3.75)], height=0.9, m_post='MI_Metal_Black',
              m_rail='MI_Metal_Black', baluster_mat='MI_Metal_Black', balusters=0.5)
    # Closed shop door (static) and the steel back door.
    PL.finish(bld, 'Laundromat')
    PL.place('SM_Door_House_Front', (3.45, y1 - 0.17, 3.75), collection='Laundromat')
    PL.place('SM_Door_Steel', (0.46, y0 + 0.17, TZ), rot=180, collection='Laundromat')
    # Sign board over the shop windows.
    sign = Builder('SM_Town_Sign_Laundromat')
    sign.box_minmax((-2.2, -0.08, -0.5), (2.2, 0.0, 0.5), 'MI_Trim_Dark', bevel=0.01)
    sign.plane(4.2, 0.95, (0, -0.082, 0), 'MI_Img_Sign_Laundromat')
    for x in (-1.6, 0.0, 1.6):
        sign.cyl(0.015, 0.4, (x, -0.25, 0.62), 'MI_Metal_Black', rot=(-60, 0, 0), segs=8)
        sign.sphere(0.07, (x, -0.42, 0.72), 'MI_Metal_Black', scale=(1, 1, 0.5), segs=10, rings=5)
    PL.place(PL.unique(sign, collision='none'), (-0.6, y1, 6.5), rot=180, collection='Laundromat')
    for x in (-1.6 - 0.6, -0.6, 1.0):
        BH.practical('spot', None, (x, y1 + 0.42, 7.18), light_dir=(0, -0.55, -0.83), intensity=60, temperature=3000,
                     radius=600, cone=(30, 55), source=4, shadows=False, tags=['HH_StreetLight'],
                     name=BH._unique_name('Light_LaundromatSign'))
    # Fire escape on the east wall (decor).
    fe = Builder('SM_Town_FireEscape')
    for z in (7.2, 10.0):
        fe.box_minmax((7.6, -6.8, z - 0.05), (8.6, -1.2, z), 'MI_Metal_Black')
        K.railing(fe, [(8.58, -6.8, z), (8.58, -1.2, z)], 1.0, 1.4, 'MI_Metal_Black', 'MI_Metal_Black', 0.15,
                  'MI_Metal_Black')
    K.stairs(fe, 'Y', -1.6, -5.6, 7.6, 8.5, 7.2, 10.0, tread='MI_Metal_Black', riser='MI_Metal_Black', solid=False)
    fe.box_minmax((7.6, -4.2, 4.6), (8.6, -2.2, 4.65), 'MI_Metal_Black')
    for y in (-6.8, -1.2):
        fe.cyl(0.03, 3.0, (8.58, y, 8.6), 'MI_Metal_Black', segs=8)
    PL.place(PL.unique(fe, collision='complex'), collection='Laundromat')


def brannigan():
    """Brannigan & Sons Plumbing - the fence's front business. Single storey with a workshop door."""
    w, d = 18.0, 14.0
    M = K.mat((-29.0, MAIN[0] - WALK, SW), 180)     # front on the sidewalk, facing north
    bld = K.Building('Town_Brannigan', M)
    b, t = bld.shell, bld.trim
    top = 5.2
    front = [dict(kind='shop', a0=-8.0, a1=-1.5, z0=0.15, z1=3.6, mullions=2, door=(-2.6, -1.6)),
             dict(kind='garage', a0=0.5, a1=4.3, z0=0.0, z1=3.4),
             dict(kind='window', a0=5.4, a1=7.6, z0=1.0, z1=2.8, style='picture', backdrop='dark', trim='MI_Trim_Dark')]
    K.wall(b, 'X', -w / 2, w / 2, 0.0, 0.3, -0.3, top, front, mat_c0='MI_Brick_Red', mat_c1='MI_Plaster', split=0.1)
    for o in front:
        if o['kind'] != 'garage':
            bld.add_opening_trim('X', o, 0.0, 0.3, inside_finish=False)
        else:
            bld.add_opening_trim('X', o, 0.0, 0.3)
    K.wall(b, 'X', -w / 2, w / 2, d - 0.3, d, -0.3, top, [], mat_c0='MI_Plaster', mat_c1='MI_Brick_Red')
    for x in (-w / 2, w / 2 - 0.3):
        b.box_minmax((x, 0.3, -0.3), (x + 0.3, d - 0.3, top), 'MI_Brick_Red')
    K.flat_roof(b, -w / 2, w / 2, 0, d, top, parapet=0.9, mat_wall='MI_Brick_Red')
    b.box_minmax((-w / 2 + 0.3, 0.3, -0.05), (w / 2 - 0.3, d - 0.3, 0.15), 'MI_Concrete_Floor')
    # Roll-up workshop door in the garage opening.
    PL.finish(bld, 'Brannigan')
    PL.place('SM_RollUpDoor', (2.4, 0.3, 0.0), rot=180, scale=(1.19, 1, 1.17), M=M, collection='Brannigan')
    PL.place('SM_Sign_Brannigan', (-4.75, 0.0, 3.75), M=M, collection='Brannigan')
    PL.practical_m(M, 'spot', 'SM_Lamp_Bulkhead', (2.4, 0.0, 3.75), rot=0, light_offset=(0, -0.08, 0),
                   light_dir=(0, -0.3, -1), intensity=55, temperature=3400, radius=800, cone=(40, 70), source=3,
                   tags=['HH_StreetLight'], emissive=20)


# =======================================================================================
# North side of Main Street: diner, clothing store, museum, hardware store

def commercial(name, cx, y_front, w, d, floors=2, facade='MI_Brick_Red', sign=None, awning=None, rot=0.0,
               shop_a=None, door_x=None, up_windows=None, back_lit=True, open_late=False):
    M = K.mat((cx, y_front, SW), rot)
    bld = K.Building(name, M)
    b, t = bld.shell, bld.trim
    gf = 4.0
    top = gf + (floors - 1) * 3.1 + 0.2
    sa = shop_a or (-w / 2 + 0.8, w / 2 - 0.8)
    dx = door_x if door_x is not None else sa[1] - 1.3
    front = [dict(kind='shop', a0=sa[0], a1=sa[1], z0=0.0, z1=3.5, mullions=2, door=(dx, dx + 1.0),
                  backdrop='shop', image='MI_Img_Shop_Interior')]
    for x in (up_windows or ([-w / 2 + 1.6 + i * (w - 3.2) / 3 for i in range(4)] if floors > 1 else [])):
        front.append(dict(kind='window', a0=x - 0.55, a1=x + 0.55, z0=gf + 0.85, z1=gf + 2.45, backdrop='curtain',
                          glow=back_lit, trim='MI_Trim_White', lites=(1, 2)))
    K.wall(b, 'X', -w / 2, w / 2, 0.0, 0.3, -0.3, top, front, mat_c0=facade, mat_c1='MI_Plaster', split=0.1)
    for o in front:
        bld.add_opening_trim('X', o, 0.0, 0.3, inside_finish=False)
    K.wall(t, 'X', -w / 2 - 0.08, w / 2 + 0.08, 0.0, -0.08, gf - 0.25, gf, [], mat_c0='MI_Trim_Dark')
    K.wall(t, 'X', -w / 2 - 0.12, w / 2 + 0.12, 0.0, -0.15, top - 0.3, top, [], mat_c0='MI_Trim_Dark')
    side_ops = []
    if floors > 1:
        side_ops = [dict(kind='window', a0=d * 0.3 - 0.5, a1=d * 0.3 + 0.5, z0=gf + 0.85, z1=gf + 2.35,
                         backdrop='curtain', glow=back_lit, trim='MI_Trim_White', lites=(1, 2))]
    for x, c_in in ((-w / 2, -w / 2 + 0.3), (w / 2, w / 2 - 0.3)):
        K.wall(b, 'Y', 0.3, d - 0.3, x, c_in, -0.3, top, side_ops, mat_c0=facade, mat_c1='MI_Plaster')
        for o in side_ops:
            bld.add_opening_trim('Y', o, x, c_in, inside_finish=False)
    back_ops = [dict(kind='door', a0=-0.45, a1=0.45, z0=0.0, z1=2.06, trim='MI_Trim_Dark')]
    K.wall(b, 'X', -w / 2, w / 2, d, d - 0.3, -0.3, top, back_ops, mat_c0=facade, mat_c1='MI_Plaster')
    for o in back_ops:
        bld.add_opening_trim('X', o, d, d - 0.3, inside_finish=False)
    K.flat_roof(b, -w / 2, w / 2, 0, d, top, parapet=0.6, mat_wall=facade)
    b.box_minmax((-w / 2 + 0.3, 0.3, -0.3), (w / 2 - 0.3, d - 0.3, 0.0), 'MI_Concrete_Floor')
    b.box_minmax((-w / 2 + 0.3, 0.3, gf - 0.3), (w / 2 - 0.3, d - 0.3, gf), 'MI_Wood_Floor_Dark')
    PL.finish(bld, 'Shops')
    PL.place('SM_Door_Steel', (0.45, d - 0.15, 0.0), M=M, rot=180, collection='Shops')
    if sign:
        PL.place(sign, ((sa[0] + sa[1]) / 2, 0.0, 3.63), M=M, collection='Shops')
    if awning:
        for ax in ((sa[0] + 1.6, sa[1] - 1.6) if w > 12 else ((sa[0] + sa[1]) / 2,)):
            PL.place('SM_Awning', (ax, 0.0, 2.75), M=M, collection='Shops')
    # Shop window light: a warm spot inside lights the painted interior in the evening.
    PL.practical_m(M, 'spot', None, ((sa[0] + sa[1]) / 2, 0.9, 3.3), light_dir=(0, 0.3, -1), intensity=35 if not open_late else 120,
                   temperature=3200, radius=700, cone=(50, 80), source=10, shadows=False,
                   tags=['HH_StreetLight'], name=BH._unique_name(name + '_WindowLight'))
    return M, bld


def main_north():
    y = MAIN[1] + WALK               # 27.0, the back edge of the sidewalk
    # Diner (open late: neon and warm light).
    M, _ = commercial('Town_Diner', -33.0, y, 13.0, 13.0, floors=1, facade='MI_Siding_White', sign='SM_Sign_Diner',
                      awning=False, open_late=True)
    PL.practical_m(M, 'point', None, (0, 4.0, 2.6), intensity=90, color=(1.0, 0.3, 0.25), radius=900, shadows=False,
                   tags=['HH_StreetLight'], name='Light_Diner_Neon')
    PL.place('SM_Bench_Park', (-33.0 + 4.5, y - 0.6, SW), rot=180, collection='Street')
    # Clothing store.
    commercial('Town_Clothing', -17.0, y, 14.0, 12.0, floors=2, facade='MI_Brick_Red', sign='SM_Sign_Clothing',
               awning=True)
    # Hardware store.
    commercial('Town_Hardware', 31.0, y, 18.0, 14.0, floors=2, facade='MI_Brick', sign='SM_Sign_Hardware',
               awning=True)
    museum()


def museum():
    """Hollowmere Historical Museum: stone, a columned portico up a flight of steps, and a lit
    display window with 'The Lantern Bearer' (Josiah Marrow) - the lore painting."""
    cx, y_front = 6.0, MAIN[1] + WALK + 5.0
    w, d = 22.0, 18.0
    M = K.mat((cx, y_front, SW), 0)
    bld = K.Building('Town_Museum', M)
    b, t = bld.shell, bld.trim
    fl = 1.2                       # raised floor
    top = fl + 7.0
    front = [dict(kind='door', a0=-1.0, a1=1.0, z0=fl, z1=fl + 3.2, trim='MI_Stone_Wall'),
             dict(kind='window', a0=-7.0, a1=-4.6, z0=fl + 0.9, z1=fl + 4.2, style='picture', mullions=1,
                  backdrop='dark', trim='MI_Stone_Wall'),
             dict(kind='window', a0=4.6, a1=7.0, z0=fl + 0.9, z1=fl + 4.2, style='picture', mullions=0,
                  backdrop=None, trim='MI_Stone_Wall')]
    K.wall(b, 'X', -w / 2, w / 2, 0.0, 0.5, -0.3, top, front, mat_c0='MI_Stone_Wall', mat_c1='MI_Plaster', split=0.2)
    for o in front:
        bld.add_opening_trim('X', o, 0.0, 0.5, inside_finish=False)
    for x, c_in in ((-w / 2, -w / 2 + 0.5), (w / 2, w / 2 - 0.5)):
        K.wall(b, 'Y', 0.5, d - 0.5, x, c_in, -0.3, top, [], mat_c0='MI_Stone_Wall')
    K.wall(b, 'X', -w / 2, w / 2, d, d - 0.5, -0.3, top, [], mat_c0='MI_Stone_Wall')
    K.flat_roof(b, -w / 2, w / 2, 0, d, top, parapet=0.9, mat_wall='MI_Stone_Wall', coping='MI_Concrete_Raw')
    # Plinth, steps, portico with four columns and a pediment.
    b.box_minmax((-w / 2 - 0.1, -0.1, -0.3), (w / 2 + 0.1, 0.0, fl), 'MI_Stone_Wall')
    for i in range(7):
        z = (i + 1) * fl / 7
        b.box_minmax((-5.0 - (6 - i) * 0.05, -4.2 + i * 0.6, -0.3), (5.0 + (6 - i) * 0.05, 0.0, z), 'MI_Concrete_Raw')
    b.box_minmax((-5.0, -0.0 - 0.01, -0.3), (5.0, 0.0, fl), 'MI_Concrete_Raw')
    for x in (-3.9, -1.4, 1.4, 3.9):
        b.lathe([(0, fl), (0.36, fl), (0.36, fl + 0.25), (0.28, fl + 0.4), (0.26, top - 1.4), (0.34, top - 1.15),
                 (0.36, top - 1.05), (0, top - 1.05)], loc=(x, -0.9, 0), mat='MI_Concrete_Raw', segs=20)
    b.box_minmax((-4.6, -1.6, top - 1.05), (4.6, 0.0, top - 0.55), 'MI_Concrete_Raw', bevel=0.02)
    K.slab(b, [(-4.8, -1.75, top - 0.55), (4.8, -1.75, top - 0.55), (0.0, -1.75, top + 1.15)], 1.75, 'MI_Concrete_Raw',
           u_dir=(1, 0, 0))
    b.box_minmax((-5.0, -1.8, fl - 0.02), (5.0, 0.0, fl), 'MI_Concrete_Raw')
    b.box_minmax((-4.6, -1.6, top - 1.07), (4.6, 0.0, top - 1.05), 'MI_Plaster')
    # The display alcove behind the east window: a real little room with the painting.
    ax0, ax1 = 3.9, 7.7
    b.box_minmax((ax0, 0.5, fl - 0.2), (ax1, 2.6, fl), 'MI_Wood_Floor_Dark')
    b.box_minmax((ax0, 2.6, fl), (ax1, 2.75, fl + 4.6), 'MI_Wallpaper_Damask')
    for x0, x1 in ((ax0 - 0.15, ax0), (ax1, ax1 + 0.15)):
        b.box_minmax((x0, 0.5, fl), (x1, 2.75, fl + 4.6), 'MI_Wallpaper_Damask')
    b.box_minmax((ax0 - 0.15, 0.5, fl + 4.6), (ax1 + 0.15, 2.75, fl + 4.8), 'MI_Plaster')
    t.box_minmax((ax0 + 0.6, 1.6, fl), (ax1 - 0.6, 2.2, fl + 0.12), 'MI_Wood_Dark')
    PL.finish(bld, 'Museum')
    PL.place('SM_Painting_Lore', (5.8, 2.6, fl + 1.1), M=M, collection='Museum')
    PL.practical_m(M, 'spot', None, (5.8, 1.3, fl + 4.4), light_dir=(0, 0.5, -0.85), intensity=45, temperature=2900,
                   radius=500, cone=(18, 32), source=2, shadows=True, name='Light_Museum_Painting')
    PL.place('SM_Sign_Museum', (-7.8, 0.0, fl + 4.6), M=M, collection='Museum')
    # Static double doors.
    for x, r in ((-1.0, 0), (1.0, 180)):
        PL.place('SM_Door_Victorian', (x, 0.25, fl), M=M, rot=r, scale=(1.04, 1.0, 1.39), collection='Museum')
    for x in (-5.4, 5.4):
        PL.practical_m(M, 'point', 'SM_Wall_Sconce', (x, -0.0, fl + 2.6), rot=0, light_offset=(0, -0.12, 0.12),
                       intensity=25, temperature=2600, radius=500, source=2, tags=['HH_StreetLight'], emissive=10)


# =======================================================================================
# Church Road: St. Agnes church and cemetery (exterior), the rectory is in prepared.py

def church():
    cx, y_front = -18.0, CHURCH[1] + WALK + 6.0
    M = K.mat((cx, y_front, SW), 0)
    bld = K.Building('Town_Church', M)
    b, t = bld.shell, bld.trim
    w, d = 12.0, 26.0
    wall_h = 7.0
    front = [dict(kind='door', a0=-1.2, a1=1.2, z0=0.3, z1=3.6, trim='MI_Stone_Wall'),
             dict(kind='window', a0=-0.8, a1=0.8, z0=4.1, z1=6.6, style='picture', mullions=1, backdrop='dark',
                  glass='MI_Glass_Dirty', trim='MI_Stone_Wall')]
    K.wall(b, 'X', -w / 2, w / 2, 0.0, 0.6, -0.3, wall_h, front, mat_c0='MI_Stone_Wall')
    for o in front:
        bld.add_opening_trim('X', o, 0.0, 0.6, inside_finish=False)
    side = [dict(kind='window', a0=y - 0.6, a1=y + 0.6, z0=2.4, z1=5.6, style='picture', backdrop='dark',
                 glass='MI_Glass_Dirty', trim='MI_Stone_Wall', glow=True, glow_tags=['HH_Church'])
            for y in (6.0, 11.0, 16.0, 21.0)]
    for x, c_in in ((-w / 2, -w / 2 + 0.6), (w / 2, w / 2 - 0.6)):
        K.wall(b, 'Y', 0.6, d - 0.6, x, c_in, -0.3, wall_h, side, mat_c0='MI_Stone_Wall')
        for o in side:
            bld.add_opening_trim('Y', o, x, c_in, inside_finish=False)
    K.wall(b, 'X', -w / 2, w / 2, d, d - 0.6, -0.3, wall_h, [], mat_c0='MI_Stone_Wall')
    K.gable_roof(b, -w / 2, w / 2, 0.0, d, wall_h, 50.0, 'MI_Roof_Shingles_Dark', 'MI_Wood_Dark', 'MI_Stone_Wall',
                 overhang=0.4, gable_overhang=0.3, ridge_axis='Y', wall_t=0.6)
    b.box_minmax((-w / 2 + 0.6, 0.6, -0.3), (w / 2 - 0.6, d - 0.6, 0.3), 'MI_Stone_Wall')
    # Bell tower at the front left with a spire; the bell is visible in the open belfry.
    tx0, tx1, ty0, ty1 = -w / 2 - 4.2, -w / 2 + 0.2, -1.0, 3.0
    th = 16.0
    belfry = [dict(kind='hole', a0=-0.7 + (tx0 + tx1) / 2, a1=0.7 + (tx0 + tx1) / 2, z0=th - 3.6, z1=th - 1.0)]
    for axis, a0, a1, c_out, c_in in (('X', tx0, tx1, ty0, ty0 + 0.5), ('X', tx0, tx1, ty1, ty1 - 0.5)):
        K.wall(b, axis, a0, a1, c_out, c_in, -0.3, th, belfry, mat_c0='MI_Stone_Wall')
    belfry_y = [dict(kind='hole', a0=-0.7 + (ty0 + ty1) / 2, a1=0.7 + (ty0 + ty1) / 2, z0=th - 3.6, z1=th - 1.0)]
    for axis, a0, a1, c_out, c_in in (('Y', ty0 + 0.5, ty1 - 0.5, tx0, tx0 + 0.5), ('Y', ty0 + 0.5, ty1 - 0.5, tx1, tx1 - 0.5)):
        K.wall(b, axis, a0, a1, c_out, c_in, -0.3, th, belfry_y, mat_c0='MI_Stone_Wall')
    b.box_minmax((tx0 + 0.5, ty0 + 0.5, th - 3.7), (tx1 - 0.5, ty1 - 0.5, th - 3.6), 'MI_Wood_Planks')
    K.hip_roof(b, tx0, tx1, ty0, ty1, th, 72.0, 'MI_Roof_Shingles_Dark', 'MI_Stone_Wall', overhang=0.2)
    bc = ((tx0 + tx1) / 2, (ty0 + ty1) / 2)
    t.lathe([(0, 0.0), (0.22, -0.02), (0.42, -0.5), (0.5, -0.9), (0.0, -0.9)], loc=(bc[0], bc[1], th - 1.3),
            mat='MI_Brass', segs=20)
    t.box_minmax((bc[0] - 0.6, bc[1] - 0.06, th - 1.3), (bc[0] + 0.6, bc[1] + 0.06, th - 1.2), 'MI_Wood_Dark')
    # A cross on the spire.
    t.box_minmax((bc[0] - 0.05, bc[1] - 0.05, th + 6.6), (bc[0] + 0.05, bc[1] + 0.05, th + 7.8), 'MI_Metal_Black')
    t.box_minmax((bc[0] - 0.35, bc[1] - 0.05, th + 7.3), (bc[0] + 0.35, bc[1] + 0.05, th + 7.4), 'MI_Metal_Black')
    for i in range(4):
        b.box_minmax((-2.4 - (3 - i) * 0.3, -1.6 + i * 0.4, -0.3), (2.4 + (3 - i) * 0.3, -1.2 + i * 0.4, 0.075 * (i + 1)),
                     'MI_Concrete_Raw')
    PL.finish(bld, 'Church')
    for x, r in ((-1.2, 0), (1.2, 180)):
        PL.place('SM_Door_Victorian', (x, 0.3, 0.3), M=M, rot=r, scale=(1.25, 1.0, 1.43), collection='Church')
    PL.practical_m(M, 'point', None, (0, 12.0, 3.0), intensity=40, color=(1.0, 0.7, 0.4), radius=1200, shadows=False,
                   name='Light_Church_Candles', flicker='Ember', strength=0.25)
    PL.practical_m(M, 'spot', 'SM_Lamp_Bulkhead', (0, -0.05, 4.0), rot=180, light_offset=(0, -0.08, 0),
                   light_dir=(0, -0.4, -1), intensity=50, temperature=2900, radius=900, cone=(40, 70), source=3,
                   tags=['HH_StreetLight'], emissive=20)
    BH.emitter(PL.world(M, (bc[0], bc[1], th - 1.6)), oneshots=['A_Church_Bell_Distant_01'], interval=(140, 300),
               scatter=50, caption='[A church bell tolls]', volume=(0.6, 0.9), name='ChurchBell')
    cemetery(cx - w / 2 - 6.0, y_front)


def cemetery(x_east, y_front):
    """Old graves west of the church behind a wrought-iron fence."""
    x0, x1 = -50.0, x_east
    y0, y1 = y_front - 1.0, y_front + 30.0
    stones = Builder('SM_Town_Gravestones')
    r = random.Random(1874)
    for row in range(5):
        for col in range(int((x1 - x0 - 4) / 2.6)):
            x = x0 + 2.5 + col * 2.6 + r.uniform(-0.3, 0.3)
            y = y0 + 4.0 + row * 5.0 + r.uniform(-0.4, 0.4)
            if r.random() < 0.18:
                continue
            kind = r.random()
            lean = r.uniform(-7, 7)
            h = r.uniform(0.6, 1.1)
            if kind < 0.6:
                stones.box((0.6, 0.12, h), (x, y, h / 2 - 0.05), 'MI_Stone_Wall', rot=(lean, r.uniform(-4, 4), r.uniform(-6, 6)),
                           bevel=0.03)
                if kind < 0.35:
                    stones.cyl(0.3, 0.12, (x, y, h - 0.05), 'MI_Stone_Wall', rot=(90 + lean, 0, 0), segs=14)
            elif kind < 0.85:
                stones.box((0.12, 0.12, h + 0.4), (x, y, (h + 0.4) / 2), 'MI_Concrete_Raw', rot=(lean, 0, 0))
                stones.box((0.55, 0.12, 0.12), (x, y, h + 0.1), 'MI_Concrete_Raw', rot=(lean, 0, 0))
            else:
                stones.box((0.8, 1.8, 0.35), (x, y + 0.9, 0.12), 'MI_Stone_Wall', rot=(0, 0, r.uniform(-4, 4)),
                           bevel=0.03)
            stones.box((0.7, 1.7, 0.04), (x, y + 0.95, 0.02), 'MI_Dirt')
    PL.place(PL.unique(stones, collision='box'), (0, 0, SW), collection='Cemetery')
    # Iron fence along the road with a gate.
    xs = x0
    while xs < x1 - 2.0:
        if abs(xs + 1.0 - (x0 + x1) / 2) < 1.2:
            PL.place('SM_Fence_Iron_Gate', (xs, y0, SW), collection='Fences')
            xs += 1.2
            continue
        PL.place('SM_Fence_Iron', (xs, y0, SW), collection='Fences')
        xs += 2.0
    for i in range(3):
        PL.place('SM_Tree_Dead', (x0 + 3 + i * 7.5, y1 - 2 - i * 3, SW), rot=i * 70, collection='Trees')
    BH.emitter((x0 + 10, y0 + 12, SW + 3), oneshots=['A_Owl_01', 'A_Owl_02'], interval=(40, 110), scatter=600,
               caption='[An owl calls]', name='Cemetery_Owl')


# =======================================================================================
# Neighbours: exterior-only houses (closed curtains, lit windows at night, locked doors)

NEIGHBOURS = [
    # name, origin, yaw, siding, roof, kind, number
    ('Ash9', (35.0, 53.0, SW), 90, 'MI_Siding_Grey', 'MI_Roof_Shingles_Dark', 'gable', '9'),
    ('Ash13', (35.0, 71.0, SW), 90, 'MI_Siding_Green', 'MI_Roof_Shingles_Brown', 'hip', '13'),
    ('Ash17', (35.0, 88.0, SW), 90, 'MI_Siding_White', 'MI_Roof_Shingles_Dark', 'gable', '17'),
    ('Ash10', (61.0, 36.5, SW), -90, 'MI_Siding_White', 'MI_Roof_Shingles_Brown', 'gable', '10'),
    ('Ash18', (61.0, 79.0, SW), -90, 'MI_Siding_Blue', 'MI_Roof_Shingles_Dark', 'hip', '18'),
    ('Mar1', (-61.0, 36.0, SW), 90, 'MI_Siding_Green', 'MI_Roof_Shingles_Dark', 'gable', '1'),
    ('Mar5', (-61.0, 55.0, SW), 90, 'MI_Siding_Grey', 'MI_Roof_Shingles_Brown', 'gable', '5'),
    ('Mar2', (-35.0, 53.0, SW), -90, 'MI_Siding_Blue', 'MI_Roof_Shingles_Brown', 'gable', '2'),
    ('Mar6', (-35.0, 71.0, SW), -90, 'MI_Siding_White', 'MI_Roof_Shingles_Dark', 'hip', '6'),
]


def neighbour(name, origin, yaw, siding, roof, kind, number, seed):
    r = random.Random(seed)
    w = r.choice((9.0, 10.0, 11.0))
    d = r.choice((8.0, 9.0))
    floors = 2 if r.random() < 0.75 else 1
    M = K.mat(origin, yaw)
    h = HS.House('Town_House_' + name, M, w, d, floors=floors, siding=siding, foundation=r.choice(
        ('MI_Brick_Red', 'MI_Concrete_Raw', 'MI_Stone_Wall')), glow_tags=[f'HH_House_{name}'])
    door_x = r.choice((-0.8, 0.0, 0.9)) if w > 9 else 0.0
    door = h.door('front', door_x)
    for x in (-w / 2 + 1.4, -w / 2 + 2.9):
        if abs(x - door_x) > 1.3:
            h.window('front', x, floor=0)
    h.window('front', w / 2 - 2.0, w=1.6, h=1.5, floor=0, style='picture', mullions=1)
    if floors > 1:
        for x in (-w / 2 + 1.6, 0.0, w / 2 - 1.6):
            h.window('front', x, floor=1)
    for y in (2.2, d - 2.2):
        h.window('left', y, floor=0)
        h.window('right', y, floor=0)
        if floors > 1:
            h.window('left', y, floor=floors - 1, w=0.8, h=1.2)
            h.window('right', y, floor=floors - 1, w=0.8, h=1.2)
    for x in (-w / 2 + 2.0, w / 2 - 2.5):
        h.window('back', x, floor=0)
    h.door('back', 0.4, w=0.86, h=2.04)
    h.build_exterior()
    zr = h.build_roof(kind, r.choice((32.0, 36.0, 40.0)), roof, overhang=0.4)
    if r.random() < 0.7:
        K.chimney(h.shell, -w / 2 + 1.3, d * 0.6, h.ze - 0.5, zr + 0.6)
    px0 = door_x - 1.6
    px1 = door_x + 1.9
    h.porch(px0, px1, 1.9, roof=True)
    PL.finish(h, 'Neighbours')
    PL.door_actor(M, door, 'SM_Door_House_Front', locked=True, house=NEIGHBOUR_TAG, name=f'Door_{name}',
                  bCanPick=False, bCanPry=False)
    PL.place('SM_Door_House_Back', h.walls['back'][-1]['hinge'], rot=180, M=M, collection='Neighbours')
    # Porch light (on all night) and a number plate.
    PL.practical_m(M, 'point', 'SM_Wall_Sconce', (door_x + 0.75, -0.0, h.base + 1.9), light_offset=(0, -0.12, 0.12),
                   intensity=18, temperature=2700, radius=600, source=2, tags=['HH_StreetLight'], emissive=10)
    return M, h, w, d


def yard(M, w, d, lot_w, setback, seed, fence='SM_Fence_Picket', back_fence='SM_Fence_Privacy', drive_side=1,
         car=None, extras=True, mailbox=True):
    """Front path, driveway, fences, a tree and clutter for a house lot (house local frame:
    front facade at y = 0, the street at y = -setback)."""
    r = random.Random(seed)
    g = Builder(f'SM_Town_Yard_{seed}')
    # Path from the sidewalk to the porch, driveway along one side.
    g.box_minmax((-0.6, -setback, 0.0), (0.6, -2.0, 0.025), 'MI_Sidewalk')
    dx = drive_side * (w / 2 + 1.8)
    g.box_minmax((dx - 1.5, -setback, 0.0), (dx + 1.5, d * 0.6, 0.02), 'MI_Concrete_Raw')
    PL.place(PL.unique(g, collision='complex'), M=M, collection='Yards')
    # Fences: front picket fence with a gap for the path and the drive, privacy fence at the back.
    half = lot_w / 2
    x = -half
    while x < half - 0.5:
        seg = 2.0
        if abs(x + seg / 2) < 1.4 or abs(x + seg / 2 - dx) < 2.0:
            x += seg
            continue
        PL.place(fence, (x, -setback + 0.4, 0.0), M=M, collection='Fences')
        x += seg
    x = -half
    while x < half - 0.5:
        PL.place(back_fence, (x, d + 5.0, 0.0), M=M, collection='Fences')
        x += 2.0
    if mailbox:
        PL.place('SM_Mailbox_Post', (1.2, -setback + 0.6, 0.0), rot=0, M=M, collection='Yards')
    if extras:
        PL.place(r.choice(('SM_Tree_Oak', 'SM_Tree_Maple')), (-drive_side * (w / 2 - 1.0), -setback + 3.2, 0.0),
                 rot=r.uniform(0, 360), M=M, collection='Trees')
        PL.place('SM_Bush_Round', (-1.6, -0.6, 0.0), M=M, collection='Yards')
        PL.place('SM_Bush_Round', (2.4, -0.6, 0.0), rot=50, M=M, collection='Yards')
        PL.place('SM_Bin_Wheelie', (dx + 1.6, d * 0.55, 0.0), rot=-90 * drive_side, M=M, collection='Yards')
        items = ('SM_Grill_BBQ', 'SM_Patio_Table', 'SM_Swing_Rusty', 'SM_Shed_Small', 'SM_Wheelbarrow', 'SM_Garden_Gnome')
        for k, item in enumerate(r.sample(items, 2)):
            PL.place(item, (r.uniform(-w / 2 + 1, w / 2 - 1), d + 2.2 + k * 0.8, 0.0), rot=r.uniform(0, 360), M=M,
                     collection='Yards')
    if car:
        PL.place(car, (dx, 2.5, 0.02), rot=180, M=M, collection='Cars')


def neighbours():
    for i, (name, origin, yaw, siding, roof, kind, number) in enumerate(NEIGHBOURS):
        M, h, w, d = neighbour(name, origin, yaw, siding, roof, kind, number, 100 + i)
        setback = 6.0 if abs(origin[0]) < 50 else 6.5
        car = ('SM_Car_Sedan_Red', None, 'SM_Car_Hatch_Blue', None, 'SM_Car_Sedan_Silver', None, None, 'SM_Car_Pickup_Black',
               None)[i]
        yard(M, w, d, 17.5, setback, 200 + i, car=car, drive_side=1 if i % 2 == 0 else -1)


# =======================================================================================
# Founders' Park (middle block)

def park():
    x0, x1, y0, y1 = -20.0, 18.0, 52.0, 94.0
    g = Builder('SM_Town_Park')
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    for a, b_ in (((cx - 1.2, y0), (cx + 1.2, y1)), ((x0, cy - 1.2), (x1, cy + 1.2))):
        g.box_minmax((a[0], a[1], 0.0), (b_[0], b_[1], 0.03), 'MI_Gravel')
    # Gazebo pad in the middle.
    g.cyl(4.2, 0.35, (cx, cy, 0.175), 'MI_Concrete_Raw', segs=8)
    PL.place(PL.unique(g, collision='complex'), (0, 0, SW), collection='Park')
    gz = Builder('SM_Town_Gazebo')
    r = 3.6
    for i in range(8):
        a = math.radians(22.5 + i * 45)
        gz.box((0.16, 0.16, 2.7), (r * math.cos(a), r * math.sin(a), 0.35 + 1.35), TRIM, rot=(0, 0, math.degrees(a)),
               bevel=0.01)
        if i != 2:
            a2 = math.radians(22.5 + (i + 1) * 45)
            p, q = (r * math.cos(a), r * math.sin(a), 0.35), (r * math.cos(a2), r * math.sin(a2), 0.35)
            K.railing(gz, [p, q], 0.85, 3.0, TRIM, TRIM, 0.14, TRIM)
    gz.lathe([(0, 4.8), (4.3, 3.05), (4.3, 2.95), (0, 3.0)], mat='MI_Roof_Shingles_Dark', segs=8)
    gz.cyl(0.08, 0.6, (0, 0, 5.05), 'MI_Metal_Black', segs=8)
    gz.cyl(4.1, 0.08, (0, 0, 0.39), 'MI_Wood_Planks', segs=8)
    PL.place(PL.unique(gz, collision='complex'), (cx, cy, SW), collection='Park')
    # Memorial stone with a plaque.
    mem = Builder('SM_Town_Memorial')
    mem.box((1.6, 1.0, 0.4), (0, 0, 0.2), 'MI_Stone_Wall', bevel=0.03)
    mem.box((1.1, 0.6, 1.6), (0, 0, 1.2), 'MI_Stone_Wall', bevel=0.05)
    mem.box((0.7, 0.02, 0.45), (0, -0.31, 1.3), 'MI_Brass', bevel=0.005)
    PL.place(PL.unique(mem, collision='box'), (cx, y0 + 6.0, SW), collection='Park')
    rr = random.Random(77)
    for i in range(16):
        x = rr.uniform(x0 + 2, x1 - 2)
        y = rr.uniform(y0 + 2, y1 - 2)
        if abs(x - cx) < 3.5 or abs(y - cy) < 3.5:
            continue
        PL.place(rr.choice(('SM_Tree_Oak', 'SM_Tree_Maple', 'SM_Tree_Oak', 'SM_Tree_Dead')), (x, y, SW),
                 rot=rr.uniform(0, 360), collection='Trees')
    for x, y, rot in ((cx - 2.2, cy + 8, -90), (cx + 2.2, cy - 8, 90), (cx - 8, cy - 2.2, 0), (cx + 8, cy + 2.2, 180)):
        PL.place('SM_Bench_Park', (x, y, SW), rot=rot, collection='Park')
    for x, y in ((cx - 2.0, y0 + 1.5), (cx + 2.0, y1 - 1.5)):
        lamp(x, y, 0, intensity=420)
    for xx in range(int(x0), int(x1) - 1, 2):
        if abs(xx + 1 - cx) > 2.2:
            PL.place('SM_Hedge_Box', (xx, y0 + 0.4, SW), collection='Park')


# =======================================================================================
# Street furniture, lamps, cars, trees

def lamp(x, y, yaw, intensity=900.0, name=None):
    """Sodium street lamp; the arm points along local +X (yaw rotates it over the road)."""
    BH.practical('spot', 'SM_StreetLamp', (x, y, SW), rot=yaw, light_offset=(0.9, 0, 4.3), light_dir=(0.15, 0, -1),
                 intensity=intensity, color=(1.0, 0.6, 0.28), radius=2200, cone=(42, 72), source=10, emissive=25,
                 tags=['HH_StreetLight'], name=name or BH._unique_name('StreetLamp'))


def street_furniture():
    # Lamps along Main Street (both sides, staggered) and the side streets.
    def on_road(x):
        return MAR[0] - 1.5 < x < MAR[1] + 1.5 or ASH[0] - 1.5 < x < ASH[1] + 1.5
    for x in range(-80, 81, 20):
        if x != 0:
            lamp(x, MAIN[0] - 1.0, 90)
        if not on_road(x + 10) and x + 10 < XMAX - 4:
            lamp(x + 10, MAIN[1] + 1.0, -90)
    for y in range(36, 98, 18):
        lamp(ASH[0] - 1.0, y, 0)
        lamp(ASH[1] + 1.0, y + 9, 180)
        lamp(MAR[1] + 1.0, y + 9, 180)
        lamp(MAR[0] - 1.0, y, 0)
    for x in range(-34, 40, 18):
        lamp(x, CHURCH[1] + 1.0, -90)
    # The lamp at the top of the hideout ramp (seen through the garage door windows below).
    BH.practical('spot', 'SM_StreetLamp', (-0.9, 12.6, TZ), rot=180, light_offset=(0.9, 0, 4.3),
                 light_dir=(0.19, 0.81, -0.57), intensity=1800, color=(1.0, 0.58, 0.25), radius=2600, cone=(35, 65),
                 source=10, emissive=25, tags=['HH_StreetLight'], name='Light_Street')
    # Street name signs at the corners, stop signs on the side streets.
    PL.place('SM_Sign_Street_Ashgrove', (ASH[0] - 1.0, MAIN[1] + 1.0, SW), rot=0, collection='Street')
    PL.place('SM_Sign_Street_Marrow', (MAR[1] + 1.0, MAIN[1] + 1.0, SW), rot=0, collection='Street')
    PL.place('SM_Sign_Street_Church', (-8.5, CHURCH[0] - 1.0, SW), rot=0, collection='Street')
    for x, rot in ((ASH[1] + 0.9, 90), (MAR[1] + 0.9, 90)):
        PL.place('SM_Sign_Stop', (x, MAIN[1] + 2.5, SW), rot=rot, collection='Street')
    PL.place('SM_Sign_Speed', (ASH[1] + 0.9, 40.0, SW), rot=90, collection='Street')
    PL.place('SM_Sign_Speed', (MAR[0] - 0.9, 40.0, SW), rot=-90, collection='Street')
    # Main Street clutter.
    PL.place('SM_BusStop', (-60.0, MAIN[0] - 1.6, SW), rot=180, collection='Street')
    PL.place('SM_Mailbox_Public', (-22.0, MAIN[1] + 0.8, SW), rot=180, collection='Street')
    PL.place('SM_Newspaper_Box', (-26.5, MAIN[1] + 0.7, SW), rot=180, collection='Street')
    PL.place('SM_Newspaper_Box', (-25.8, MAIN[1] + 0.7, SW), rot=185, collection='Street')
    for x in (-45.5, 12.0, 42.5, 70.0):
        PL.place('SM_Hydrant', (x, MAIN[0] - 0.5, SW), rot=90, collection='Street')
    for x in (-41.0, -9.5, 22.5, 53.0):
        PL.place('SM_Bin_Metal', (x, MAIN[1] + 0.6, SW), collection='Street')
    for x in (-36.0, 0.0, 27.0):
        PL.place('SM_Planter_Box', (x, MAIN[1] + 1.2, SW), collection='Street')
    for x in (-70.0, -30.0, 10.0, 33.0, 75.0):
        PL.place('SM_Manhole', (x, (MAIN[0] + MAIN[1]) / 2 + 2.0, TZ), collection='Street')
    for x in range(-84, 85, 24):
        PL.place('SM_Storm_Drain', (x, MAIN[0] + 0.18, TZ), collection='Street')
        PL.place('SM_Storm_Drain', (x + 12, MAIN[1] - 0.18, TZ), rot=180, collection='Street')
    for y in (40.0, 75.0):
        PL.place('SM_Hydrant', (ASH[0] - 0.6, y, SW), collection='Street')
        PL.place('SM_Hydrant', (MAR[1] + 0.6, y + 8, SW), collection='Street')
    # Utility poles along the side streets.
    for y in range(32, 100, 22):
        PL.place('SM_UtilityPole', (ASH[1] + 2.0, y, SW), rot=90, collection='Street')
        PL.place('SM_UtilityPole', (MAR[0] - 2.0, y + 11, SW), rot=90, collection='Street')
    # Parked cars along the curbs.
    cars = ('SM_Car_Sedan_Red', 'SM_Car_Hatch_Green', 'SM_Car_Sedan_Silver', 'SM_Car_Hatch_Blue', 'SM_Car_Pickup_Black')
    rr = random.Random(5)
    for x in (-74.0, -57.0, -21.0, 36.0, 63.0, 74.0):
        PL.place(rr.choice(cars), (x, MAIN[1] - 1.15, TZ), rot=90, collection='Cars')
    for x in (-66.0, -36.0, 31.5, 58.0):
        PL.place(rr.choice(cars), (x, MAIN[0] + 1.15, TZ), rot=-90, collection='Cars')
    for y in (33.0, 86.0):
        PL.place(rr.choice(cars), (ASH[0] + 1.1, y, TZ), rot=180, collection='Cars')
        PL.place(rr.choice(cars), (MAR[1] - 1.1, y - 10, TZ), rot=0, collection='Cars')
    # Street trees.
    for x in (-84.0, -70.0, -56.0, 46.0, 60.0, 74.0):
        PL.place('SM_Tree_Maple', (x, MAIN[0] - 2.0, SW), rot=x * 13, collection='Trees')


def edges():
    """Woods, closed roads and invisible boundary walls around the playable town."""
    rr = random.Random(99)
    trees = ('SM_Tree_Pine', 'SM_Tree_Pine', 'SM_Tree_Pine', 'SM_Tree_Dead', 'SM_Tree_Oak')
    ring = []
    for x in range(int(XMIN), int(XMAX) + 1, 7):
        ring.append((x + rr.uniform(-2, 2), YMIN + rr.uniform(1, 5)))
        ring.append((x + rr.uniform(-2, 2), YMAX - rr.uniform(1, 5)))
    for y in range(int(YMIN), int(YMAX) + 1, 7):
        if MAIN[0] - 3 < y < MAIN[1] + 3:
            continue
        ring.append((XMIN + rr.uniform(1, 5), y + rr.uniform(-2, 2)))
        ring.append((XMAX - rr.uniform(1, 5), y + rr.uniform(-2, 2)))
    # Woods north of the rectory and east of Ashgrove's last lot.
    for _ in range(40):
        ring.append((rr.uniform(40, XMAX - 4), rr.uniform(112, YMAX - 4)))
    for _ in range(12):
        ring.append((rr.uniform(56, XMAX - 4), rr.uniform(91, 105)))
    for x, y in ring:
        PL.place(rr.choice(trees), (x, y, SW), rot=rr.uniform(0, 360), scale=rr.uniform(0.85, 1.25), collection='Woods')
    # Main Street leaves town in both directions: closed for roadworks.
    for x, s in ((XMIN + 6.0, 1), (XMAX - 6.0, -1)):
        for k in range(4):
            PL.place('SM_Sawhorse', (x, MAIN[0] + 1.2 + k * 2.0, TZ), rot=90, collection='Street')
            PL.place('SM_Traffic_Cone', (x + s * 1.2, MAIN[0] + 2.0 + k * 2.0, TZ), collection='Street')
    # Boundary walls (block pawns only, invisible).
    walls = [((XMIN + 3, (YMIN + YMAX) / 2, SW + 3), (0.5, (YMAX - YMIN) / 2, 6)),
             ((XMAX - 3, (YMIN + YMAX) / 2, SW + 3), (0.5, (YMAX - YMIN) / 2, 6)),
             ((0.0, YMIN + 3, SW + 3), ((XMAX - XMIN) / 2, 0.5, 6)),
             ((0.0, YMAX - 3, SW + 3), ((XMAX - XMIN) / 2, 0.5, 6))]
    for (x, y, z), (ex, ey, ez) in walls:
        BH.actor('HHBoundaryWall', K.mat((x, y, z)), 'Gameplay', name=BH._unique_name('Boundary'),
                 components={'Wall': {'BoxExtent': [ex * 100.0, ey * 100.0, ez * 100.0]}})


def ambience():
    """Spatial beds: day birds / night crickets, wind, distant traffic, dogs, cars, sirens."""
    for x, y in ((-50, 60), (0, 75), (60, 60), (0, 125), (-60, 120), (60, 125), (0, 32)):
        BH.emitter((x, y, SW + 4), loop='A_Town_Birds_Day_Loop', name='Birds_Day', scatter=0)
        BH.ACTORS[-1]['tags'] = ['HH_DayOnly']
        BH.emitter((x, y, SW + 1), loop='A_Town_Crickets_Night_Loop', name='Crickets_Night', scatter=0)
        BH.ACTORS[-1]['tags'] = ['HH_NightOnly']
    for x in (-70.0, 70.0):
        BH.emitter((x, 20.5, SW + 2), loop='A_Town_Traffic_Distant_Loop', name='Traffic', scatter=0)
    for x, y in ((-40, 40), (40, 100), (0, 120)):
        BH.emitter((x, y, SW + 6), oneshots=['A_Wind_Gust_01', 'A_Wind_Gust_02', 'A_Wind_Gust_03'], interval=(20, 60),
                   scatter=1500, caption='[Wind through the trees]', name='Wind')
    BH.emitter((80.0, 100.0, SW + 2), oneshots=['A_Dog_Bark_Distant_01', 'A_Dog_Bark_Distant_02', 'A_Dog_Bark_Distant_03'],
               interval=(35, 120), scatter=3000, caption='[A dog barks in the distance]', name='Dogs')
    BH.emitter((0.0, 20.0, SW + 1), oneshots=['A_Car_Pass_Distant_01', 'A_Car_Pass_Distant_02', 'A_Car_Pass_Distant_03'],
               interval=(25, 90), scatter=4000, caption='[A car passes somewhere]', name='Cars')
    BH.emitter((-80.0, 140.0, SW + 2), oneshots=['A_Siren_Distant_01'], interval=(240, 600), scatter=500,
               caption='[A siren, far away]', name='Siren')
    BH.emitter((61.0, 38.0, SW + 3), oneshots=['A_Wind_Chime_01', 'A_Wind_Chime_02'], interval=(12, 40), scatter=80,
               caption='[Wind chimes]', name='WindChime')
    # The laundromat's machines hum above the hideout.
    BH.emitter((0.0, 0.0, 5.0), loop='A_Machine_Hum_Loop', name='Laundromat_Machines', scatter=0, volume=(0.4, 0.6))


def build():
    streets()
    south_side()
    main_north()
    church()
    neighbours()
    park()
    street_furniture()
    edges()
    ambience()
