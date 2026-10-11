"""
The two houses whose jobs are prepared but not released yet (mission bAvailable = false):

  * The Marrow House, Marrow Street - empty since 1974, run-down Victorian, boarded up, overgrown.
    Ground floor furnished under dust sheets, upper floor walls only. A light flickers in the attic.
  * St. Agnes Rectory, Church Road - larger stone house behind an iron fence, better secured
    (no lock to pick on the front door). Ground floor rooms finished, upper floor walls only.

Both stand in L_Town with real (locked) doors; their job maps (L_Job_Marrow, L_Job_Rectory)
already contain the van, crew starts, the house volume and the haunt director so a level
designer can walk them, but no loot yet.
"""
import build_hideout as BH
import houses as HS
import placement as PL
import town as T
import worldkit as K
from hhblend import Builder
from worldkit import T_EXT, T_INT, TRIM

MARROW = T.HOUSE_TAGS['marrow']
RECTORY = T.HOUSE_TAGS['rectory']

M_MARROW = K.mat(T.MARROW_ORIGIN, 90)        # faces east onto Marrow Street
M_RECTORY = K.mat(T.RECTORY_ORIGIN, 0)       # faces south onto Church Road


def _sheeted(M, name, x, y, z, rot=0.0, scale=(1.0, 1.0, 1.0)):
    """Furniture under a dust sheet: the piece plus a draped white sheet box slightly larger."""
    PL.place(name, (x, y, z), rot, scale, M=M, collection='Marrow')


def marrow():
    M = M_MARROW
    w, d = 12.0, 10.0
    h = HS.House('Marrow_House', M, w, d, floors=2, base=0.9, fh=3.1, siding='MI_Siding_Worn', foundation='MI_Stone_Wall',
                 trim='MI_Trim_Dark', interior=True, glow=False)
    front = h.door('front', 0.0, w=0.96, h=2.3)
    for x in (-4.0, -2.2, 2.2, 4.0):
        h.window('front', x, w=0.9, h=1.8, floor=0, sill=0.7, boarded=True)
    for x in (-4.0, -1.4, 1.4, 4.0):
        h.window('front', x, w=0.9, h=1.6, floor=1, glass='MI_Glass_Dirty')
    for y in (2.5, 7.5):
        h.window('left', y, w=0.9, h=1.8, sill=0.7, boarded=True)
        h.window('right', y, w=0.9, h=1.8, sill=0.7, boarded=True)
        h.window('left', y, w=0.9, h=1.6, floor=1, glass='MI_Glass_Dirty')
        h.window('right', y, w=0.9, h=1.6, floor=1, glass='MI_Glass_Dirty')
    back = h.door('back', -3.0, w=0.86, h=2.04)
    h.window('back', 2.5, w=0.9, h=1.8, sill=0.7, boarded=True)
    h.build_exterior()
    zr = h.build_roof('gable', 48.0, 'MI_Roof_Shingles_Brown', ridge_axis='X', overhang=0.5, gable_mat='MI_Siding_Worn')
    h.attic_floor()
    K.chimney(h.shell, 4.5, d * 0.5, h.ze, zr + 0.4, w=0.8, d=0.6, m='MI_Stone_Wall')
    # Attic window in the gable (the light in October).
    attic = Builder('SM_Marrow_AtticWindow')
    attic.box_minmax((-w / 2 - 0.05, d / 2 - 0.45, h.ze + 1.6), (-w / 2 + 0.02, d / 2 + 0.45, h.ze + 2.6), 'MI_Trim_Dark')
    attic.box_minmax((-w / 2 - 0.06, d / 2 - 0.38, h.ze + 1.68), (-w / 2 - 0.05, d / 2 + 0.38, h.ze + 2.52), 'MI_Glass_Dirty')
    PL.place(PL.unique(attic, collision='none'), M=M, collection='Marrow')
    porch_c, porch_f = h.porch(-3.2, 3.2, 2.4, roof=True, steps=(-0.8, 0.8), roof_mat='MI_Roof_Shingles_Brown')

    # Interior: hall, parlour, dining room, kitchen; upper floor partitions only.
    G = h.base
    U = G + h.fh
    GC, UC = U - 0.25, h.ze - 0.02
    xi0, xi1, yi0, yi1 = -w / 2 + T_EXT, w / 2 - T_EXT, T_EXT, d - T_EXT
    h.slab([(xi0, yi0, xi1, yi1)], G)
    h.slab([(xi0, yi0, -1.2, yi1), (1.2, yi0, xi1, yi1), (-1.2, 6.5, 1.2, yi1)], U)
    h.partition('Y', yi0, yi1, -1.2, G, GC, [dict(kind='arch', a0=1.5, a1=3.1, z0=G, z1=G + 2.3),
                                            dict(kind='idoor', a0=6.6, a1=7.42, z0=G, z1=G + 2.02)],
                mat_lo='MI_Wallpaper_Damask', mat_hi='MI_Paint_Cream')
    h.partition('Y', yi0, yi1, 1.2, G, GC, [dict(kind='arch', a0=1.5, a1=3.1, z0=G, z1=G + 2.3),
                                           dict(kind='idoor', a0=6.6, a1=7.42, z0=G, z1=G + 2.02)],
                mat_lo='MI_Paint_Cream', mat_hi='MI_Wallpaper_Stripe')
    h.partition('X', xi0, -1.26, 5.0, G, GC, [dict(kind='idoor', a0=-3.6, a1=-2.78, z0=G, z1=G + 2.02)],
                mat_lo='MI_Wallpaper_Damask', mat_hi='MI_Paint_Sage')
    h.partition('X', 1.26, xi1, 5.0, G, GC, [], mat_lo='MI_Wallpaper_Stripe', mat_hi='MI_Tiles_White')
    for side, x in (('lo', -1.2), ('hi', 1.2)):
        h.partition('Y', yi0, yi1, x, U, UC, [dict(kind='hole', a0=2.0, a1=2.9, z0=U, z1=U + 2.0)])
    h.room(xi0, -1.26, yi0, 4.94, G, GC, walls='MI_Wallpaper_Damask', floor='MI_Wood_Floor_Dark', crown=True)
    h.room(xi0, -1.26, 5.06, yi1, G, GC, walls='MI_Paint_Sage', floor='MI_Wood_Floor_Dark')
    h.room(-1.14, 1.14, yi0, yi1, G, GC, walls='MI_Paint_Cream', floor='MI_Wood_Floor_Dark')
    h.room(1.26, xi1, yi0, 4.94, G, GC, walls='MI_Wallpaper_Stripe', floor='MI_Wood_Floor_Dark', crown=True)
    h.room(1.26, xi1, 5.06, yi1, G, GC, walls='MI_Tiles_White', floor='MI_Tiles_Checker')
    K.stairs(h.trim, 'Y', 1.0, 6.4, -1.0, 0.0, G, U, tread='MI_Wood_Floor_Dark', riser='MI_Wood_Dark')
    h.shell.box_minmax((xi0, yi0, U), (xi1, yi1, U + 0.012), 'MI_Wood_Floor_Dark')
    PL.finish(h, 'Marrow', tags=[MARROW])
    PL.door_actor(M, front, 'SM_Door_Victorian', locked=True, house=MARROW, name='Marrow_FrontDoor',
                  tags=['HH_HouseDoor', MARROW], bCanPick=True, PickSeconds=9.0, bCanPry=True)
    PL.door_actor(M, back, 'SM_Door_Cellar', locked=True, house=MARROW, name='Marrow_BackDoor',
                  tags=['HH_HouseDoor', MARROW], bCanPick=False, bCanPry=True, PrySeconds=3.0)
    # Furniture under dust sheets (ground floor).
    for name, x, y, rot in (('SM_Sofa_3Seat', -3.6, 2.6, 0), ('SM_Armchair_Wing', -4.9, 4.0, 60),
                            ('SM_Piano_Upright', 3.8, yi0 + 0.31, 180), ('SM_GrandfatherClock', -1.45, 4.3, 90),
                            ('SM_DiningTable', -3.6, 7.4, 90), ('SM_RockingChair', 3.2, 3.0, 220),
                            ('SM_Kitchen_Stove', xi1 - 0.36, 7.4, -90), ('SM_Bookshelf_Tall', 5.6, 2.8, -90)):
        _sheeted(M, name, x, y, G + 0.012, rot)
    sheets = Builder('SM_Marrow_DustSheets')
    for x, y, sx, sy, sz in ((-3.6, 2.6, 2.3, 1.0, 0.95), (-4.9, 4.0, 1.0, 1.0, 1.1), (-3.6, 7.4, 1.0, 1.8, 0.8)):
        sheets.box((sx, sy, sz), (x, y, G + sz / 2), 'MI_Fabric_Bedding', bevel=0.12, segs=3)
    PL.place(PL.unique(sheets, collision='none'), M=M, collection='Marrow')
    # Yard: overgrown, broken fence, dead trees; the attic light.
    for x, y, r in ((-6.0, -6.0, 10), (6.5, -4.0, 200), (-8.0, 12.0, 80)):
        PL.place('SM_Tree_Dead', (x, y, 0.0), r, M=M, collection='Marrow')
    xs = -11.0
    while xs < 11.0:
        if abs(xs + 1.0) > 1.5 and int(xs) % 7 != 3:
            PL.place('SM_Fence_Iron', (xs, -7.2, 0.0), 2.0 * ((int(xs) % 3) - 1), M=M, collection='Fences')
        xs += 2.0
    PL.place('SM_Fence_Iron_Gate', (-0.6, -7.2, 0.0), -35, M=M, collection='Fences')
    for x, y in ((-3.0, -2.0), (3.5, -2.5), (-5.0, -4.5), (5.0, -5.5), (-1.5, -4.0)):
        PL.place('SM_Bush_Round', (x, y, 0.0), x * 17, scale=1.3, M=M, collection='Marrow')
    BH.decal('MI_Decal_Leaves', PL.world(M, (0.0, -3.0, 0.02)), (6.0, 6.0), name='Marrow_Leaves')
    PL.practical_m(M, 'point', None, (-w / 2 + 0.6, d / 2, h.ze + 2.0), intensity=8, color=(1.0, 0.55, 0.25), radius=300,
                   shadows=False, flicker='Ember', strength=0.6, tags=['HH_HouseLight', MARROW], name='Marrow_AtticLight')
    BH.emitter(PL.world(M, (0.0, d / 2, G + 2.0)), oneshots=['A_WoodCreak_01', 'A_WoodCreak_02', 'A_WoodCreak_03'],
               interval=(20, 60), scatter=400, name='Marrow_Creaks')


def rectory():
    M = M_RECTORY
    w, d = 14.0, 11.0
    h = HS.House('Rectory_House', M, w, d, floors=2, base=0.75, fh=3.2, siding='MI_Stone_Wall', foundation='MI_Stone_Wall',
                 trim='MI_Trim_White', interior=True, glow=False, wall_in='MI_Plaster')
    front = h.door('front', 0.0, w=0.96, h=2.3, trim='MI_Stone_Wall')
    for x in (-5.2, -3.0, 3.0, 5.2):
        h.window('front', x, w=1.0, h=1.9, floor=0, sill=0.7, lites=(2, 3))
        h.window('front', x, w=1.0, h=1.7, floor=1, sill=0.7, lites=(2, 3))
    h.window('front', 0.0, w=0.9, h=1.4, floor=1, lites=(2, 2))
    for y in (2.4, 4.2, 8.4):
        for side in ('left', 'right'):
            h.window(side, y, w=0.9, h=1.8, floor=0, sill=0.75)
            h.window(side, y, w=0.9, h=1.6, floor=1, sill=0.75)
    back = h.door('back', 3.0, w=0.9, h=2.1)
    h.window('back', -3.0, w=1.0, h=1.8, sill=0.8)
    h.build_exterior(corner_boards=False)
    zr = h.build_roof('hip', 32.0, 'MI_Roof_Shingles_Dark', overhang=0.55)
    h.attic_floor()
    for x in (-4.5, 4.5):
        K.chimney(h.shell, x, d * 0.55, h.ze - 0.3, zr - 0.6, w=0.8, d=0.7, m='MI_Stone_Wall')
    # Stone portico.
    b = h.shell
    for i in range(4):
        z = h.base - i * h.base / 4
        b.box_minmax((-2.0 - i * 0.3, -1.4 - i * 0.35, -0.3), (2.0 + i * 0.3, 0.0, z), 'MI_Stone_Wall')
    for x in (-1.6, 1.6):
        K.column(h.trim, x, -1.2, h.base, h.base + 3.0, w=0.3, m='MI_Concrete_Raw')
    h.trim.box_minmax((-2.0, -1.5, h.base + 3.0), (2.0, 0.0, h.base + 3.35), 'MI_Concrete_Raw')
    # Interior (ground floor finished, upper floor partitions).
    G = h.base
    U = G + h.fh
    GC, UC = U - 0.25, h.ze - 0.02
    xi0, xi1, yi0, yi1 = -w / 2 + T_EXT, w / 2 - T_EXT, T_EXT, d - T_EXT
    h.slab([(xi0, yi0, xi1, yi1)], G)
    h.slab([(xi0, yi0, -1.4, yi1), (1.4, yi0, xi1, yi1), (-1.4, 7.6, 1.4, yi1)], U)
    for x in (-1.4, 1.4):
        h.partition('Y', yi0, yi1, x, G, GC, [dict(kind='idoor', a0=2.0, a1=2.82, z0=G, z1=G + 2.02),
                                             dict(kind='idoor', a0=7.6, a1=8.42, z0=G, z1=G + 2.02)],
                    mat_lo='MI_Wallpaper_Damask' if x < 0 else 'MI_Paint_Cream',
                    mat_hi='MI_Paint_Cream' if x < 0 else 'MI_Wallpaper_Stripe')
        h.partition('Y', yi0, yi1, x, U, UC, [dict(kind='hole', a0=2.0, a1=2.9, z0=U, z1=U + 2.0)])
    h.partition('X', xi0, -1.46, 5.5, G, GC, [], mat_lo='MI_Wallpaper_Damask', mat_hi='MI_Paint_Sage')
    h.partition('X', 1.46, xi1, 5.5, G, GC, [], mat_lo='MI_Wallpaper_Stripe', mat_hi='MI_Paint_White')
    h.room(xi0, -1.46, yi0, 5.44, G, GC, walls='MI_Wallpaper_Damask', floor='MI_Wood_Floor_Dark', crown=True)
    h.room(xi0, -1.46, 5.56, yi1, G, GC, walls='MI_Paint_Sage', floor='MI_Wood_Floor')
    h.room(-1.34, 1.34, yi0, yi1, G, GC, walls='MI_Paint_Cream', floor='MI_Tiles_Checker')
    h.room(1.46, xi1, yi0, 5.44, G, GC, walls='MI_Wallpaper_Stripe', floor='MI_Carpet_Burgundy', crown=True)
    h.room(1.46, xi1, 5.56, yi1, G, GC, walls='MI_Paint_White', floor='MI_Tiles_Checker')
    K.stairs(h.trim, 'Y', 2.2, 7.6, -1.2, -0.1, G, U, tread='MI_Wood_Floor_Dark', riser='MI_Wood_Dark')
    h.shell.box_minmax((xi0, yi0, U), (xi1, yi1, U + 0.012), 'MI_Wood_Floor')
    PL.finish(h, 'Rectory', tags=[RECTORY])
    # Better secured: the front door can't be picked (key or nothing), the back door has a deadbolt.
    PL.door_actor(M, front, 'SM_Door_Victorian', locked=True, house=RECTORY, name='Rectory_FrontDoor',
                  tags=['HH_HouseDoor', RECTORY], bCanPick=False, KeyId='RectoryKey')
    PL.door_actor(M, back, 'SM_Door_House_Back', locked=True, house=RECTORY, name='Rectory_BackDoor',
                  tags=['HH_HouseDoor', RECTORY], bCanPick=True, PickSeconds=12.0, bCanCutGlass=True, CutSeconds=6.0)
    for name, x, y, rot in (('SM_Bookshelf_Tall', -5.5, 5.28, 0), ('SM_Bookshelf_Tall', -4.5, 5.28, 0),
                            ('SM_Desk_Home', -4.0, 2.4, 180), ('SM_Armchair_Wing', -2.6, 3.8, -60),
                            ('SM_DiningTable', 4.2, 2.8, 90),
                            ('SM_DiningChair', 3.55, 2.4, 90), ('SM_DiningChair', 4.85, 3.2, -90),
                            ('SM_Fridge', 6.35, 8.2, -90), ('SM_Kitchen_Base_Sink', 6.43, 6.8, -90),
                            ('SM_Coat_Rack', 0.9, 1.0, 0)):
        PL.place(name, (x, y, G + 0.012), rot, M=M, collection='Rectory')
    PL.place('SM_Painting_Large', (-w / 2 + T_EXT, 3.3, G + 1.3), 90, M=M, collection='Rectory')
    # Iron fence with a gate along the street, hedges inside.
    xs = -12.0
    while xs < 12.0:
        if abs(xs + 1.0) > 1.0:
            PL.place('SM_Fence_Iron', (xs, -5.0, 0.0), M=M, collection='Fences')
        xs += 2.0
    PL.place('SM_Fence_Iron_Gate', (-0.6, -5.0, 0.0), M=M, collection='Fences')
    for xs in range(-12, 12, 2):
        if abs(xs + 1) > 2:
            PL.place('SM_Hedge_Box', (xs, -3.8, 0.0), M=M, collection='Rectory')
    for x in (-1.6, 1.6):
        PL.practical_m(M, 'point', 'SM_Wall_Sconce', (x * 1.6, 0.0, G + 2.4), light_offset=(0, -0.12, 0.12), intensity=22,
                       temperature=2700, radius=600, source=2, tags=['HH_HouseLight', RECTORY], emissive=10)
    for x, y in ((-3.6, 3.0), (3.6, 3.0)):
        PL.practical_m(M, 'point', 'SM_Ceiling_Light_Home', (x, y, GC), light_offset=(0, 0, -0.14), intensity=110,
                       temperature=2700, radius=650, source=3, tags=['HH_HouseLight', RECTORY], emissive=6)


def build():
    marrow()
    rectory()


# =======================================================================================
# Prepared job maps: van, crew starts, house volume, haunt director (no loot yet)

def _job(M, w, d, tag, van_pos, van_yaw, name):
    from mathutils import Vector
    c = tuple(M @ Vector((0.0, d / 2, 3.0)))
    yaw = abs(round(PL.yaw_of(M))) % 180
    ex, ey = (w / 2, d / 2) if yaw < 45 else (d / 2, w / 2)
    BH.actor('HHHouseVolume', K.mat(c), 'Gameplay', name=f'{name}_HouseVolume', properties={'HouseTag': tag},
             components={'Box': {'BoxExtent': [ex * 100.0 + 10, ey * 100.0 + 10, 400.0]}})
    BH.actor('HHHauntDirector', K.mat(c), 'Haunt', name=f'{name}_Haunt', properties={'HouseTag': tag})
    van = K.mat(van_pos, van_yaw)
    BH.static('SM_Van_Body', parent=van, collection='Van')
    for y in (-1.55, 1.45):
        BH.static('SM_Van_Wheel', (0.88, y, 0.375), parent=van, collection='Van')
        BH.static('SM_Van_Wheel', (-0.88, y, 0.375), rot=180, parent=van, collection='Van')
    BH.static('SM_Van_SlidingDoor', (0.99, 0.2, 1.2), parent=van, collection='Van')
    BH.actor('HHVanPoint', van @ K.mat((0.0, -2.75, 1.0)), 'Gameplay', name='Van_Stash',
             properties={'PointType': BH.enum('EHHVanPointType', 'Stash'), 'InteractSound': BH.asset('S_Bag_Zip_01'),
                         'Prompt': BH.text('Stash the loot in the van')})
    BH.actor('HHVanPoint', van @ K.mat((-1.15, 1.0, 1.0)), 'Gameplay', name='Van_Drive',
             properties={'PointType': BH.enum('EHHVanPointType', 'Drive'),
                         'InteractSound': BH.asset('S_Van_Engine_Start_01'), 'Prompt': BH.text('Drive away')})
    for i, (dx, dy) in enumerate(((2.3, -1.5), (2.3, 0.0), (2.3, 1.5), (3.4, 0.7))):
        BH.actor('PlayerStart', van @ K.mat((dx, dy, 0.15 + 0.92), 0.0), 'Gameplay', name=f'PlayerStart_{i}',
                 properties={'PlayerStartTag': f'Crew{i}'})
    BH.tag_point('HH_WakePoint', tuple(van @ Vector((2.4, -4.0, 0.25))))


def marrow_job():
    _job(M_MARROW, 12.0, 10.0, MARROW, (T.MAR[0] + 1.25, 83.0, T.TZ), 180, 'Marrow')


def rectory_job():
    _job(M_RECTORY, 14.0, 11.0, RECTORY, (21.0, T.CHURCH[1] - 1.25, T.TZ), 90, 'Rectory')
