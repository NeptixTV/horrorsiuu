"""
The Vance residence, 14 Ashgrove Lane - the first job (fully playable).

Two storeys + basement, 11 x 9 m, front facing west onto Ashgrove Lane. House-local frame
(houses.py): front facade y = 0, x -5.5..5.5, ground floor z = 0.6, upper floor 3.5, basement -2.0.

Ground floor                         Upper floor                      Basement
  living room (W front)                master bedroom (W front)         furnace, laundry, shelves,
  dining room (W back)                 guest room (W back)              workbench, the old trunk
  hall + stairs (centre)               hall, landing (centre)
  study (E front)                      kid's room (E front)
  kitchen + back door (E back)         bathroom (E back)

The shell, rooms, furniture, lights and doors live in L_Town (the house is part of the street,
locked outside the job). Loot, containers, the spare key and the haunting are placed in the job
map by job_layout().
"""
import math

from mathutils import Vector

import build_hideout as BH
import hhblend as H
import houses as HS
import placement as PL
import worldkit as K
from hhblend import Builder
from worldkit import T_EXT, T_INT, TRIM
import town as T

HOUSE = T.HOUSE_TAGS['vance']
W, D = 11.0, 9.0
G, U, B = 0.6, 3.5, -2.0            # floor levels
GC, UC, BC = 3.25, 6.38, 0.35        # ceilings
XI0, XI1, YI0, YI1 = -W / 2 + T_EXT, W / 2 - T_EXT, T_EXT, D - T_EXT
PX1, PX2 = -1.1, 1.1                 # long partitions (centre lines)
STAIR_X = (0.16, 1.04)               # main stair / basement stair width
STAIR_Y = (1.9, 5.9)                 # main stair foot -> top (basement stair below, reversed)
UP_HOLE_Y0 = 2.8                     # upper floor opening over the stair starts here

M = K.mat(T.VANCE_ORIGIN, -90)       # local -> world (faces west)
HOUSE_TAGS = [HOUSE]


def L(x, y, z=0.0):
    """Local -> world point (for actors placed with world coordinates)."""
    return tuple(M @ Vector((x, y, z)))


def put(name, x, y, z=0.0, rot=0.0, scale=1.0, collection='Vance', tags=None, shadow=True):
    return PL.place(name, (x, y, z), rot, scale, M=M, collection=collection, tags=tags, shadow=shadow)


def house_light(kind, fixture, x, y, z, on=False, name=None, **kw):
    kw.setdefault('temperature', 2700)
    kw.setdefault('radius', 650)
    kw.setdefault('source', 3)
    kw.setdefault('intensity', 120)
    obj, entry = PL.practical_m(M, kind, fixture, (x, y, z), tags=['HH_HouseLight', HOUSE], name=name, **kw)
    entry.setdefault('properties', {})['bStartOn'] = bool(on)
    return obj, entry


def ceiling_light(x, y, ceiling, on=False, name=None, intensity=140):
    return house_light('point', 'SM_Ceiling_Light_Home', x, y, ceiling, on, name, light_offset=(0, 0, -0.14),
                       intensity=intensity, emissive=6)


# =======================================================================================
# Shell, rooms, stairs

def build_house():
    h = HS.House('Vance_House', M, W, D, floors=2, base=G, fh=2.9, siding='MI_Siding_Grey', foundation='MI_Brick_Red',
                 basement_h=2.6, interior=True, glow=False)
    # --- Exterior openings -------------------------------------------------------------------
    front_door = h.door('front', -0.49)
    for x in (-4.5, -2.1):
        h.window('front', x, floor=0)
    h.window('front', 3.2, w=1.2, floor=0)
    for x in (-4.5, -2.1, 2.2, 4.2):
        h.window('front', x, floor=1)
    h.window('left', 1.2, w=0.8, floor=0)
    h.window('left', 4.6, w=0.8, floor=0)
    h.window('left', 6.6, floor=0)
    h.window('left', 1.6, w=0.8, floor=1)
    h.window('left', 4.4, w=0.8, floor=1)
    h.window('left', 7.2, floor=1)
    h.window('right', 2.0, floor=0)
    h.window('right', 6.4, w=1.0, h=1.0, sill=1.2, floor=0)
    h.window('right', 1.0, w=0.8, floor=1)
    h.window('right', 7.3, w=0.7, h=1.0, sill=1.2, floor=1)
    back_door = h.door('back', 3.9, w=0.86, h=2.04)
    for x in (-3.2, 2.0):
        h.window('back', x, floor=0)
    h.window('back', -3.2, floor=1)
    h.window('back', 0.0, w=0.7, h=1.1, floor=1)
    h.window('back', 2.8, w=0.7, h=1.0, sill=1.2, floor=1)
    h.build_exterior()
    zr = h.build_roof('gable', 38.0, 'MI_Roof_Shingles_Dark', overhang=0.45)
    h.attic_floor()
    # Exterior chimney on the left (north) wall, behind the fireplace.
    h.shell.box_minmax((-W / 2 - 0.55, 2.4, -0.3), (-W / 2, 3.4, h.ze - 0.4), 'MI_Brick_Red')
    K.chimney(h.shell, -W / 2 - 0.28, 2.9, h.ze - 0.4, zr + 0.7, w=0.55, d=1.0)
    porch_mid, porch_front = h.porch(-2.4, 1.6, 2.0, roof=True, steps=(-1.2, 0.2))
    # Back stoop.
    for i in range(3):
        z = G - i * 0.2
        h.shell.box_minmax((3.3, D, -0.3), (4.5, D + 0.9 + i * 0.3, z), 'MI_Concrete_Raw')

    # --- Floors (slabs with stair holes) -------------------------------------------------------
    sx0, sx1 = STAIR_X
    # Ground floor: hole over the basement stair between the stair wall (x 0.04..0.16) and P2 (1.04..1.16).
    h.slab([(XI0, YI0, XI1, STAIR_Y[0]), (XI0, STAIR_Y[1], XI1, YI1), (XI0, STAIR_Y[0], 0.04, STAIR_Y[1]),
            (sx1 + T_INT, STAIR_Y[0], XI1, STAIR_Y[1])], G)
    h.slab([(XI0, YI0, XI1, UP_HOLE_Y0), (XI0, STAIR_Y[1], XI1, YI1), (XI0, UP_HOLE_Y0, sx0, STAIR_Y[1]),
            (sx1, UP_HOLE_Y0, XI1, STAIR_Y[1])], U)
    h.shell.box_minmax((XI0, YI0, B - 0.3), (XI1, YI1, B), 'MI_Concrete_Floor')

    # --- Partitions ----------------------------------------------------------------------------
    ho = []                                                     # interior openings
    def opening(kind, a0, a1, z0, h_):
        return dict(kind=kind, a0=a0, a1=a1, z0=z0, z1=z0 + h_)
    # Ground floor.
    h.partition('Y', YI0, YI1, PX1, G, GC, [opening('arch', 1.0, 2.6, G, 2.25), opening('arch', 6.4, 7.6, G, 2.2)],
                mat_lo='MI_Wallpaper_Floral', mat_hi='MI_Paint_Cream')
    study_door = opening('idoor', 0.8, 1.62, G, 2.02)
    kitchen_door = opening('idoor', 6.6, 7.42, G, 2.02)
    h.partition('Y', YI0, YI1, PX2, G, GC, [study_door, kitchen_door], mat_lo='MI_Paint_Cream', mat_hi='MI_Paint_Sage')
    h.partition('X', PX2 + T_INT / 2, XI1, 4.0, G, GC, [], mat_lo='MI_Paint_Sage', mat_hi='MI_Paint_White')
    h.partition('X', XI0, PX1 - T_INT / 2, 5.6, G, GC, [opening('arch', -2.8, -1.4, G, 2.2)],
                mat_lo='MI_Wallpaper_Floral', mat_hi='MI_Wallpaper_Stripe')
    # Stair enclosure (ground + basement) and the basement door.
    h.partition('Y', STAIR_Y[0], STAIR_Y[1], 0.1, B, GC, [], mat_lo='MI_Paint_Cream', mat_hi='MI_Plaster')
    basement_door = opening('idoor', 0.19, 1.01, G, 2.0)
    h.partition('X', sx0, sx1, STAIR_Y[1], G, GC - 0.05, [basement_door], mat_lo='MI_Plaster', mat_hi='MI_Paint_Cream')
    h.shell.box_minmax((sx1, STAIR_Y[0], B), (sx1 + T_INT, STAIR_Y[1], G), 'MI_Concrete_Raw')
    # Upper floor.
    master_door = opening('idoor', 4.6, 5.42, U, 2.02)
    guest_door = opening('idoor', 6.8, 7.62, U, 2.02)
    h.partition('Y', YI0, YI1, PX1, U, UC, [master_door, guest_door], mat_lo='MI_Wallpaper_Damask',
                mat_hi='MI_Paint_White')
    kids_door = opening('idoor', 1.0, 1.82, U, 2.02)
    bath_door = opening('idoor', 6.4, 7.22, U, 2.02)
    h.partition('Y', YI0, YI1, PX2, U, UC, [kids_door, bath_door], mat_lo='MI_Paint_White', mat_hi='MI_Paint_Blue')
    h.partition('X', PX2 + T_INT / 2, XI1, 4.46, U, UC, [], mat_lo='MI_Paint_Blue', mat_hi='MI_Tiles_White')
    h.partition('X', XI0, PX1 - T_INT / 2, 5.6, U, UC, [], mat_lo='MI_Wallpaper_Damask', mat_hi='MI_Wallpaper_Stripe')

    # --- Rooms ---------------------------------------------------------------------------------
    px1a, px1b = PX1 - T_INT / 2, PX1 + T_INT / 2
    px2a, px2b = PX2 - T_INT / 2, PX2 + T_INT / 2
    h.room(XI0, px1a, YI0, 5.54, G, GC, walls='MI_Wallpaper_Floral', floor='MI_Wood_Floor', crown=True)     # living
    h.room(XI0, px1a, 5.66, YI1, G, GC, walls='MI_Wallpaper_Stripe', floor='MI_Wood_Floor', crown=True,
           wainscot=('MI_Cabinet_Wood', 0.9))                                                             # dining
    h.room(px1b, px2a, YI0, STAIR_Y[0], G, GC, walls='MI_Paint_Cream', floor='MI_Wood_Floor_Dark',
           open_sides=('y1',))                                                                            # front hall
    h.room(px1b, 0.04, STAIR_Y[0], STAIR_Y[1], G, GC, walls='MI_Paint_Cream', floor='MI_Wood_Floor_Dark',
           open_sides=('y0', 'y1', 'x1'))                                                                 # hall by the stair
    h.room(px1b, px2a, STAIR_Y[1], YI1, G, GC, walls='MI_Paint_Cream', floor='MI_Wood_Floor_Dark',
           open_sides=('y0',))                                                                            # back hall
    h.room(px2b, XI1, YI0, 3.94, G, GC, walls='MI_Paint_Sage', floor='MI_Carpet_Burgundy')                # study
    h.room(px2b, XI1, 4.06, YI1, G, GC, walls='MI_Paint_White', floor='MI_Tiles_Checker',
           wainscot=('MI_Tiles_White', 1.2))                                                              # kitchen
    h.room(XI0, px1a, YI0, 5.54, U, UC, walls='MI_Wallpaper_Damask', floor='MI_Carpet_Beige')             # master
    h.room(XI0, px1a, 5.66, YI1, U, UC, walls='MI_Wallpaper_Stripe', floor='MI_Wood_Floor')               # guest
    h.room(px1b, px2a, YI0, UP_HOLE_Y0, U, UC, walls='MI_Paint_White', floor='MI_Wood_Floor_Dark',
           open_sides=('y1',))                                                                            # upper front hall
    h.room(px1b, 0.04, UP_HOLE_Y0, STAIR_Y[1], U, UC, walls='MI_Paint_White', floor='MI_Wood_Floor_Dark',
           open_sides=('y0', 'y1', 'x1'))                                                                 # gallery
    h.room(px1b, px2a, STAIR_Y[1], YI1, U, UC, walls='MI_Paint_White', floor='MI_Wood_Floor_Dark',
           open_sides=('y0',))                                                                            # landing
    h.shell.box_minmax((0.04, UP_HOLE_Y0, UC - 0.012), (px2a, STAIR_Y[1], UC), 'MI_Plaster')              # stairwell ceiling
    h.room(px2b, XI1, YI0, 4.4, U, UC, walls='MI_Paint_Blue', floor='MI_Carpet_Beige')                    # kid's room
    h.room(px2b, XI1, 4.52, YI1, U, UC, walls='MI_Paint_White', floor='MI_Tiles_Checker',
           wainscot=('MI_Tiles_White', 1.2))                                                              # bathroom
    h.room(XI0, XI1, YI0, YI1, B, BC, walls='MI_Concrete_Raw', floor='MI_Concrete_Floor', ceiling='MI_Wood_Planks',
           skirting=None)                                                                                 # basement

    # --- Stairs --------------------------------------------------------------------------------
    main_stair(h.trim, sx0, sx1)
    K.stairs(h.shell, 'Y', STAIR_Y[0], STAIR_Y[1], sx0, sx1, B, G, tread='MI_Wood_Raw', riser='MI_Concrete_Raw')
    # Upper railings around the stair opening.
    K.railing(h.trim, [(0.1, UP_HOLE_Y0, U), (0.1, STAIR_Y[1] - 0.05, U)], 0.95, 1.2, TRIM, 'MI_Wood_Dark', 0.13, TRIM)
    K.railing(h.trim, [(0.1, UP_HOLE_Y0, U), (sx1, UP_HOLE_Y0, U)], 0.95, 1.2, TRIM, 'MI_Wood_Dark', 0.13, TRIM)
    # Basement stair hand rail on the wall.
    h.trim.tube([(0.22, STAIR_Y[0] + 0.2, B + 1.0), (0.22, STAIR_Y[1] - 0.1, G + 0.9)], 0.022, 'MI_Wood_Raw', segs=8)

    # House number by the door.
    h.trim.box_minmax((0.3, -0.03, G + 1.75), (0.62, 0.0, G + 2.05), 'MI_Trim_Dark')
    h.trim.plane(0.28, 0.28, (0.46, -0.032, G + 1.9), 'MI_Img_House_Numbers', uv_rect=(0.0, 0.0, 0.25, 1.0))
    PL.finish(h, 'Vance', tags=HOUSE_TAGS)

    doors(h, front_door, back_door, study_door, kitchen_door, basement_door, master_door, guest_door, kids_door,
          bath_door)
    return h


def main_stair(b, x0, x1):
    """Closed-riser stair with a soffit, rising +Y from STAIR_Y[0] (G) to STAIR_Y[1] (U)."""
    n = 16
    y0, y1 = STAIR_Y
    run = (y1 - y0) / n
    rise = (U - G) / n
    for i in range(n):
        ya = y0 + i * run
        z = G + (i + 1) * rise
        b.box_minmax((x0, ya - 0.025, z - 0.03), (x1, ya + run, z), 'MI_Wood_Floor_Dark', bevel=0.004)
        b.box_minmax((x0, ya - 0.002, z - rise), (x1, ya + 0.018, z - 0.03), TRIM)
    length = math.hypot(y1 - y0, U - G)
    ang = math.degrees(math.atan2(U - G, y1 - y0))
    b.box((x1 - x0, length, 0.04), ((x0 + x1) / 2, (y0 + y1) / 2 + 0.05, (G + U) / 2 - 0.22), 'MI_Plaster', rot=(ang, 0, 0))
    # Wall-side handrail.
    b.tube([(x1 - 0.06, y0 + 0.1, G + 1.05), (x1 - 0.06, y1, U + 0.9)], 0.024, 'MI_Wood_Dark', segs=8)


def doors(h, front, back, study, kitchen, basement, master, guest, kids, bath):
    common = dict(house=HOUSE)
    PL.door_actor(M, front, 'SM_Door_House_Front', locked=True, name='Vance_FrontDoor', tags=['HH_HouseDoor', HOUSE],
                  bCanPick=True, KeyId='VanceSpareKey', PickSeconds=7.0, **common)
    PL.door_actor(M, back, 'SM_Door_House_Back', locked=True, name='Vance_BackDoor', tags=['HH_HouseDoor', HOUSE],
                  bCanPick=False, bCanPry=True, bCanCutGlass=True, PrySeconds=2.5, CutSeconds=4.5, **common)
    # Interior doors: hinge at a0 on partitions (axis Y: leaf along +Y needs yaw 90 in the local frame).
    for o, x, name, mesh in ((study, PX2, 'Vance_StudyDoor', 'SM_Door_Interior'),
                             (kitchen, PX2, 'Vance_KitchenDoor', 'SM_Door_Interior'),
                             (master, PX1, 'Vance_MasterDoor', 'SM_Door_Interior_Dark'),
                             (guest, PX1, 'Vance_GuestDoor', 'SM_Door_Interior_Dark'),
                             (kids, PX2, 'Vance_KidsDoor', 'SM_Door_Interior'),
                             (bath, PX2, 'Vance_BathDoor', 'SM_Door_Interior')):
        od = dict(o, hinge=(x, o['a0'], o['z0']), rot=90)
        PL.door_actor(M, od, mesh, locked=False, name=name, tags=['HH_HouseDoor', HOUSE], **common)
    od = dict(basement, hinge=(basement['a1'], STAIR_Y[1], basement['z0']), rot=180)
    PL.door_actor(M, od, 'SM_Door_Cellar', locked=False, name='Vance_BasementDoor', tags=['HH_HouseDoor', HOUSE],
                  **common)


# =======================================================================================
# Furniture (L_Town)

def furnish():
    g, u, bz = G + 0.012, U + 0.012, B
    # --- Living room -----------------------------------------------------------------------------
    put('SM_Fireplace', XI0, 2.9, g, rot=90)
    put('SM_Rug_Large', -3.2, 2.9, g, rot=90)
    put('SM_Sofa_3Seat', -1.9, 2.9, g, rot=-90)
    put('SM_Armchair_Wing', -4.2, 4.75, g, rot=-15)
    put('SM_TVStand_Low', -3.3, YI0 + 0.24, g, rot=180)
    put('SM_Bookshelf_Tall', -4.55, 5.54 - 0.16, g, rot=0)
    put('SM_SideTable', -5.0, 4.95, g)
    put('SM_SideTable', -1.55, 1.05, g)
    put('SM_Plant_Pot', -3.4, 5.25, g)
    put('SM_Picture_Frame', XI0, 4.2, g + 1.45, rot=90)
    put('SM_Curtains_Window', -4.5, YI0, g + 0.65, rot=180, shadow=False)
    put('SM_Curtains_Window', -2.1, YI0, g + 0.65, rot=180, shadow=False)
    house_light('point', 'SM_Lamp_Table', -5.0, 4.95, g + 0.6, on=True, name='Vance_LivingLamp', light_offset=(0, 0, 0.38),
                intensity=60, temperature=2400, radius=520, emissive=8)
    ceiling_light(-3.2, 2.9, GC, name='Vance_LivingCeiling')
    # --- Dining room -----------------------------------------------------------------------------
    put('SM_DiningTable', -3.2, 7.2, g, rot=90)
    for x, y, r in ((-3.85, 6.85, 90), (-3.85, 7.55, 90), (-2.55, 7.55, -90), (-3.2, 8.15, 0)):
        put('SM_DiningChair', x, y, g, rot=r)
    put('SM_Dresser', XI0 + 0.26, 8.0, g, rot=90)
    put('SM_Painting_Large', -3.2, YI1, g + 1.35, rot=0)
    put('SM_Rug_Large', -3.2, 7.2, g, rot=90, scale=(0.9, 1.0, 1.0))
    ceiling_light(-3.2, 7.2, GC, name='Vance_DiningCeiling')
    # --- Hall ----------------------------------------------------------------------------------
    put('SM_Doormat', -0.49, 0.6, g)
    put('SM_GrandfatherClock', PX1 + T_INT / 2 + 0.16, 3.6, g, rot=90)
    put('SM_Coat_Rack', -0.7, 8.2, g)
    put('SM_Mirror_Wall', PX1 + T_INT / 2, 4.6, g + 1.1, rot=90)
    ceiling_light(-0.5, 1.2, GC, name='Vance_HallCeiling')
    ceiling_light(-0.1, 7.4, GC, name='Vance_BackHallCeiling')
    BH.emitter(L(-0.6, 3.6, g + 1.6), loop='A_Clock_Tick_Loop', name='Vance_ClockTick', scatter=0, volume=(0.7, 0.8))
    # --- Study ---------------------------------------------------------------------------------
    put('SM_Desk_Home', XI1 - 0.31, 2.0, g, rot=-90)
    put('SM_OfficeChair', 4.15, 2.0, g, rot=90)
    put('SM_Bookshelf_Tall', 2.5, 3.94 - 0.16, g, rot=0)
    put('SM_Bookshelf_Tall', 3.45, 3.94 - 0.16, g, rot=0)
    put('SM_Rug_Large', 3.2, 2.0, g, scale=(0.7, 0.7, 1.0))
    put('SM_Painting_Large', XI1, 0.9, g + 1.3, rot=-90, scale=0.7)
    house_light('spot', 'SM_DeskLamp', XI1 - 0.12, 2.48, g + 0.754, rot=180, name='Vance_DeskLamp',
                light_offset=(0.19, 0, 0.415), light_dir=(0.42, 0, -0.9), intensity=90, radius=420, cone=(30, 55),
                emissive=20)
    ceiling_light(3.2, 2.0, GC, name='Vance_StudyCeiling')
    # --- Kitchen -------------------------------------------------------------------------------
    for y, name in ((4.5, 'SM_Kitchen_Stove'), (5.4, 'SM_Kitchen_Base'), (6.4, 'SM_Kitchen_Base_Sink'),
                    (7.4, 'SM_Kitchen_Base')):
        put(name, XI1 - 0.32, y, g, rot=-90)
    put('SM_Fridge', XI1 - 0.39, 8.3, g, rot=-90)
    for y in (5.4, 7.4):
        put('SM_Kitchen_Wall', XI1, y, g + 1.5, rot=-90)
    put('SM_Microwave', XI1 - 0.3, 7.55, g + 0.96, rot=-90)
    put('SM_Pot_Cooking', XI1 - 0.35, 4.4, g + 0.92)
    put('SM_Plate_Stack', XI1 - 0.3, 5.1, g + 0.96)
    put('SM_DiningTable', 2.5, 5.4, g, rot=90, scale=(0.62, 0.62, 1.0))
    put('SM_DiningChair', 1.85, 5.4, g, rot=90)
    put('SM_DiningChair', 3.15, 5.4, g, rot=-90)
    BH.emitter(L(XI1 - 0.4, 8.3, g + 1.0), loop='A_Fridge_Hum_Loop', name='Vance_Fridge', scatter=0, volume=(0.8, 0.9))
    ceiling_light(3.2, 6.3, GC, name='Vance_KitchenCeiling')
    # --- Upper hall ------------------------------------------------------------------------------
    put('SM_Picture_Frame', PX1 + T_INT / 2, 3.4, u + 1.5, rot=90)
    put('SM_Picture_Frame', PX1 + T_INT / 2, 2.2, u + 1.55, rot=90)
    put('SM_Rug_Large', -0.5, 7.3, u, rot=90, scale=(0.4, 0.8, 1.0))
    ceiling_light(-0.5, 4.2, UC, on=True, name='Vance_UpperHallLight', intensity=90)
    ceiling_light(-0.1, 7.6, UC, name='Vance_LandingLight')
    # --- Master bedroom --------------------------------------------------------------------------
    put('SM_Bed_Double', XI0 + 1.06, 2.9, u, rot=90)
    put('SM_Nightstand', XI0 + 0.21, 1.65, u, rot=90)
    put('SM_Nightstand', XI0 + 0.21, 4.15, u, rot=90)
    put('SM_Dresser', -3.3, YI0 + 0.26, u, rot=180)
    put('SM_Mirror_Wall', -3.3, YI0, u + 1.05, rot=180)
    put('SM_Wardrobe', PX1 - T_INT / 2 - 0.3, 2.0, u, rot=-90)
    put('SM_Rug_Large', -3.0, 2.9, u, rot=90, scale=0.8)
    house_light('point', 'SM_Lamp_Table', XI0 + 0.21, 1.65, u + 0.55, name='Vance_MasterLamp', light_offset=(0, 0, 0.38),
                intensity=50, radius=450, emissive=8)
    ceiling_light(-3.2, 2.9, UC, name='Vance_MasterCeiling')
    # --- Guest room ------------------------------------------------------------------------------
    put('SM_Bed_Single', -4.4, YI1 - 1.01, u)
    put('SM_Desk_Home', -1.9, 6.0, u, rot=-90)
    put('SM_Shelf_Wood', XI0 + 0.2, 6.3, u, rot=90)
    put('SM_Painting_Large', PX1 - T_INT / 2, 7.9, u + 1.3, rot=-90, scale=0.8)
    ceiling_light(-3.2, 7.2, UC, name='Vance_GuestCeiling')
    # --- Kid's room ------------------------------------------------------------------------------
    put('SM_Bed_Single', XI1 - 1.01, 2.6, u, rot=-90)
    put('SM_Nightstand', XI1 - 0.21, 3.75, u, rot=-90)
    put('SM_Bookshelf_Tall', PX2 + T_INT / 2 + 0.16, 3.2, u, rot=90)
    put('SM_Rug_Large', 3.2, 2.3, u, scale=(0.6, 0.6, 1.0))
    ceiling_light(3.2, 2.2, UC, name='Vance_KidsCeiling')
    # --- Bathroom ------------------------------------------------------------------------------
    put('SM_Bathtub', 4.3, YI1 - 0.375, u)
    put('SM_ShowerCurtain', 4.3, YI1 - 0.78, u)
    put('SM_Toilet', XI1 - 0.36, 5.3, u, rot=-90)
    put('SM_Sink_Pedestal', XI1 - 0.23, 6.35, u, rot=-90)
    put('SM_Medicine_Cabinet', XI1, 6.35, u + 1.25, rot=-90)
    put('SM_Towel_Rack', PX2 + T_INT / 2, 5.2, u + 1.0, rot=90, shadow=False)
    ceiling_light(3.2, 6.6, UC, name='Vance_BathCeiling')
    # --- Basement --------------------------------------------------------------------------------
    put('SM_Furnace', -3.4, 7.2, bz)
    put('SM_WaterHeater', -4.6, 8.15, bz)
    put('SM_Washer', 2.7, YI1 - 0.33, bz)
    put('SM_Dryer', 3.4, YI1 - 0.33, bz)
    put('SM_Shelf_Wood', XI1 - 0.2, 2.5, bz, rot=-90)
    put('SM_Shelf_Wood', XI1 - 0.2, 4.4, bz, rot=-90)
    put('SM_Workbench', XI0 + 0.38, 2.6, bz, rot=90)
    for x, y, n, r in ((-2.4, 1.0, 'SM_Box_A', 12), (-2.0, 1.3, 'SM_Box_B', -20), (-2.2, 1.15, 'SM_Box_A', 40),
                       (4.4, 7.0, 'SM_Box_Open', 8), (-4.8, 5.0, 'SM_Barrel', 0), (1.6, 0.8, 'SM_Bucket', 0)):
        put(n, x, y, bz, rot=r)
    for x, y in ((-2.5, 3.0), (2.8, 5.5)):
        house_light('point', 'SM_Lamp_Bulb', x, y, BC, name=BH._unique_name('Vance_BasementBulb'),
                    light_offset=(0, 0, -0.99), intensity=70, temperature=2500, radius=600, emissive=20)
    BH.emitter(L(-3.4, 7.2, bz + 1.2), loop='A_Hum_Boiler_Loop', name='Vance_Furnace', scatter=0, volume=(0.5, 0.6))
    BH.emitter(L(0.0, 4.0, bz + 1.5), oneshots=[f'A_Drip_0{i}' for i in range(1, 7)], interval=(4, 12), scatter=300,
               name='Vance_Drips')


def yard():
    """Front yard, back yard, porch light and the gnome the spare key hides under."""
    T.yard(M, W, D, 21.0, 6.5, 314, fence='SM_Fence_Picket', back_fence='SM_Fence_Privacy', drive_side=1,
           car=None, extras=False)
    put('SM_Tree_Oak', -7.5, -3.8, 0.0, rot=40)
    put('SM_Bush_Round', -3.3, -0.55, 0.0)
    put('SM_Bush_Round', 2.6, -0.55, 0.0, rot=70)
    put('SM_Bush_Small', -4.6, -0.5, 0.0)
    put('SM_Garden_Gnome', 2.15, -1.1, 0.0, rot=200)
    put('SM_Planter_Box', -3.0, -2.6, 0.0)
    put('SM_Doghouse', -3.5, D + 4.0, 0.0, rot=160)
    put('SM_Swing_Rusty', 2.5, D + 6.5, 0.0, rot=10)
    put('SM_Grill_BBQ', 5.0, D + 1.6, 0.0, rot=-30)
    put('SM_Bin_Wheelie', 7.1, 6.0, 0.0, rot=-90)
    put('SM_Bin_Wheelie', 7.1, 6.7, 0.0, rot=-95)
    put('SM_Fence_Picket_Gate', 6.2, D + 0.2, 0.0, rot=0)
    house_light('point', 'SM_Wall_Sconce', 0.35, 0.0, G + 1.9, on=True, name='Vance_PorchLight', light_offset=(0, -0.12, 0.12),
                intensity=20, radius=650, source=2, emissive=10)


def build():
    h = build_house()
    furnish()
    yard()
    return h


# =======================================================================================
# The job (L_Job_Vance): loot, containers, the spare key, the haunting, the van

LOOT = [
    # item id, mesh, local position (x, y, z), yaw, container
    ('LT_TV', 'SM_Loot_TV', (-3.3, 0.51, G + 0.512), 180, None),
    ('LT_Console', 'SM_Loot_Console', (-3.3, 1.05, G + 0.012), 170, None),
    ('LT_MantelClock', 'SM_Loot_MantelClock', (XI0 + 0.24, 2.9, G + 1.262), 90, None),
    ('LT_Candlestick', 'SM_Loot_Candlestick', (XI0 + 0.24, 2.3, G + 1.262), 0, None),
    ('LT_Vase', 'SM_Loot_Vase', (XI0 + 0.3, 1.75, G + 0.012), 0, None),
    ('LT_Whiskey', 'SM_Loot_Whiskey', (-1.55, 1.05, G + 0.612), 20, None),
    ('LT_Silverware', 'SM_Loot_Silverware', (XI0 + 0.28, 8.0, G + 0.862), 90, None),
    ('LT_Candlestick', 'SM_Loot_Candlestick', (-3.2, 7.0, G + 0.772), 0, None),
    ('LT_Painting', 'SM_Loot_Painting', (-1.9, YI1 - 0.12, G + 0.012), 0, None),
    ('LT_Laptop', 'SM_Loot_Laptop', (XI1 - 0.42, 1.62, G + 0.766), -80, None),
    ('LT_Cash', 'SM_Loot_Cash', (XI1 - 0.5, 2.3, G + 0.766), -90, 'StudyDesk'),
    ('LT_Camera', 'SM_Loot_Camera', (1.55, 0.6, G + 0.012), 30, None),
    ('LT_CoinJar', 'SM_Loot_CoinJar', (XI1 - 0.3, 5.75, G + 0.972), 0, None),
    ('LT_Phone', 'SM_Loot_Phone', (2.55, 5.3, G + 0.772), 25, None),
    ('LT_CoinCollection', 'SM_Loot_CoinCollection', (-3.3, 0.53, U + 0.862), 180, 'MasterDresser'),
    ('LT_JewelryBox', 'SM_Loot_JewelryBox', (-3.75, 0.5, U + 0.862), 180, None),
    ('LT_Perfume', 'SM_Loot_Perfume', (-2.85, 0.48, U + 0.862), 170, None),
    ('LT_Necklace', 'SM_Loot_Necklace', (XI0 + 0.21, 4.0, U + 0.562), 80, 'MasterNightstand'),
    ('LT_Watch', 'SM_Loot_Watch', (XI0 + 0.21, 4.3, U + 0.562), 100, None),
    ('LT_Handbag', 'SM_Loot_Handbag', (-1.75, 3.6, U + 0.012), -60, None),
    ('LT_Ring', 'SM_Loot_Ring', (XI1 - 0.33, 6.35, U + 0.932), -90, 'BathCabinet'),
    ('LT_MusicBox', 'SM_Loot_MusicBox', (2.9, 1.9, U + 0.022), 150, None),
    ('LT_Tablet', 'SM_Loot_Tablet', (XI1 - 1.2, 2.5, U + 0.56), -70, None),
    ('LT_Figurine', 'SM_Loot_Figurine', (XI1 - 0.21, 3.75, U + 0.562), -100, None),
    ('LT_Locket', 'SM_Loot_Locket', (-2.2, 1.3, B + 0.522), 0, 'BasementTrunk'),
]

CONTAINERS = [
    # container id, name, local position of the volume centre, half extents (m), drawer mesh + slide
    ('StudyDesk', 'desk drawers', (XI1 - 0.55, 2.35, G + 0.5), (0.25, 0.3, 0.25), None),
    ('MasterDresser', 'dresser', (-3.3, YI0 + 0.55, U + 0.45), (0.6, 0.3, 0.42), None),
    ('MasterNightstand', 'nightstand', (XI0 + 0.45, 4.15, U + 0.35), (0.25, 0.25, 0.25), None),
    ('BathCabinet', 'medicine cabinet', (XI1 - 0.1, 6.35, U + 1.55), (0.12, 0.28, 0.32), None),
    ('BasementTrunk', 'old trunk', (-2.2, 1.3, B + 0.3), (0.5, 0.35, 0.32), 'SM_Crate_Wood'),
    ('KitchenDrawers', 'kitchen drawers', (XI1 - 0.6, 7.4, G + 0.75), (0.3, 0.5, 0.25), None),
    ('GuestDesk', 'sewing desk', (-2.2, 6.0, U + 0.5), (0.3, 0.5, 0.25), None),
]


def _world_rot(yaw_local):
    return yaw_local - 90.0


def job_layout():
    """Everything only the job needs. Returns the van world matrix (for the player starts)."""
    for item, mesh, (x, y, z), yaw, container in LOOT:
        props = {'ItemId': item}
        if container:
            props['ContainerId'] = container
        BH.actor('HHLootActor', M @ K.mat((x, y, z), yaw), 'Loot', mesh=mesh, properties=props,
                 name=BH._unique_name('Loot_' + item[3:]))
    for cid, label, (x, y, z), ext, mesh in CONTAINERS:
        props = {'ContainerId': cid, 'ContainerName': BH.text(label), 'SearchSeconds': 1.8,
                 'SearchVolumeExtent': [ext[0] * 100.0, ext[1] * 100.0, ext[2] * 100.0]}
        if mesh:
            props['OpenOffset'] = [0.0, 0.0, 0.0]
        BH.actor('HHSearchableActor', M @ K.mat((x, y, z - (0.3 if mesh else 0.0))), 'Loot', mesh=mesh,
                 properties=props, name='Search_' + cid)
    # The spare key: under the garden gnome by the porch (search it).
    BH.actor('HHSearchableActor', M @ K.mat((2.15, -1.1, 0.15)), 'Loot',
             properties={'ContainerId': 'Gnome', 'ContainerName': BH.text('garden gnome'), 'SearchSeconds': 1.2,
                         'SearchVolumeExtent': [18.0, 18.0, 20.0]}, name='Search_Gnome')
    BH.actor('HHLootActor', M @ K.mat((2.45, -1.0, 0.02), 30), 'Loot', mesh='SM_Key_Spare',
             properties={'ItemId': 'Key', 'ContainerId': 'Gnome',
                         'KeyId': 'VanceSpareKey', 'KeyName': BH.text('spare key to 14 Ashgrove Lane')},
             name='Loot_SpareKey')

    # The house volume (inside tracking), haunt director, haunt elements.
    c = L(0.0, D / 2, (B + UC) / 2)
    BH.actor('HHHouseVolume', K.mat(c), 'Gameplay', name='Vance_HouseVolume',
             properties={'HouseTag': HOUSE},
             components={'Box': {'BoxExtent': [D / 2 * 100.0 + 10, W / 2 * 100.0 + 10, (UC - B) / 2 * 100.0 + 30]}})
    haunt_director()
    haunt_elements()

    # The van, parked at the curb in front of the house, nose north.
    van = K.mat((T.ASH[1] - 1.25, 57.0, T.TZ), 0)
    BH.static('SM_Van_Body', parent=van, collection='Van')
    for y in (-1.55, 1.45):
        BH.static('SM_Van_Wheel', (0.88, y, 0.375), parent=van, collection='Van')
        BH.static('SM_Van_Wheel', (-0.88, y, 0.375), rot=180, parent=van, collection='Van')
    BH.static('SM_Van_Livery', (1.012, -1.38, 1.45), rot=90, parent=van, collection='Van', shadow=False)
    BH.static('SM_Van_Livery', (-1.012, -0.2, 1.45), rot=-90, parent=van, collection='Van', shadow=False)
    BH.static('SM_Van_SlidingDoor', (0.99, 0.2, 1.2), parent=van, collection='Van')
    BH.actor('HHVanPoint', van @ K.mat((0.0, -2.75, 1.0)), 'Gameplay', name='Van_Stash',
             properties={'PointType': BH.enum('EHHVanPointType', 'Stash'), 'InteractSound': BH.asset('S_Bag_Zip_01'),
                         'Prompt': BH.text('Stash the loot in the van')})
    BH.actor('HHVanPoint', van @ K.mat((-1.15, 1.0, 1.0)), 'Gameplay', name='Van_Drive',
             properties={'PointType': BH.enum('EHHVanPointType', 'Drive'), 'InteractSound': BH.asset('S_Van_Engine_Start_01'),
                         'Prompt': BH.text('Drive away')})
    # Crew starts beside the van on the sidewalk side, facing the house.
    for i, (dx, dy) in enumerate(((2.3, -1.5), (2.3, 0.0), (2.3, 1.5), (3.4, 0.7))):
        BH.actor('PlayerStart', van @ K.mat((dx, dy, 0.15 + 0.92), 0.0), 'Gameplay', name=f'PlayerStart_{i}',
                 properties={'PlayerStartTag': f'Crew{i}'})
    BH.tag_point('HH_WakePoint', (T.ASH[1] + 1.8, 52.0, T.SW + 0.1))
    BH.tag_point('HH_WakePoint', (T.ASH[1] + 1.8, 61.0, T.SW + 0.1))
    return van


def haunt_director():
    a = BH.asset
    BH.actor('HHHauntDirector', K.mat(L(0.0, D / 2, G + 1.0)), 'Haunt', name='Vance_Haunt', properties={
        'HouseTag': HOUSE,
        'SettleSounds': [a(f'A_House_Settle_0{i}') for i in range(1, 5)] + [a('A_WoodCreak_01'), a('A_WoodCreak_02'),
                                                                              a('A_PipeCreak_01')],
        'DistantSounds': [a('A_Haunt_Knock_Wall_01'), a('A_Haunt_Knock_Wall_02'), a('A_DoorSlam_Distant_01'),
                          a('A_Knock_Three')],
        'FootstepSounds': [a(f'A_Overhead_Step_0{i}') for i in range(1, 7)] + [a('A_Haunt_Footsteps_Run_01')],
        'DragSounds': [a('A_Haunt_Drag_01'), a('A_Haunt_Drag_02')],
        'MusicBoxSound': a('A_Haunt_MusicBox_01'),
        'SlamSounds': [a('A_Haunt_Door_Slam_01'), a('A_Haunt_Door_Slam_02')],
        'WhisperSounds': [a('A_Whisper_01'), a('A_Whisper_02'), a('A_Whisper_03'), a('A_Haunt_Breath_01'),
                          a('A_Haunt_Breath_02')],
        'GlassSounds': [a('A_Haunt_Glass_Fall_01'), a('A_Haunt_Glass_Fall_02')],
        'BulbPopSound': a('S_Light_Bulb_Pop'),
        'DoorCreakSound': a('S_Door_Creak_Slow'),
        'StingerSounds': [a('M_Haunt_Stinger_01'), a('M_Haunt_Stinger_02')],
    })


def haunt_elements():
    tags = [HOUSE]
    # Sound points in every room (and between the floors), apparition spots in dark corners.
    for x, y, z in ((-3.2, 2.9, G + 1.2), (-3.2, 7.2, G + 1.2), (3.2, 2.0, G + 1.2), (3.2, 6.3, G + 1.2),
                    (-0.5, 3.5, G + 1.2), (-3.2, 2.9, U + 1.2), (-3.2, 7.2, U + 1.2), (3.2, 2.2, U + 1.2),
                    (3.2, 6.6, U + 1.2), (-0.5, 7.5, U + 1.2), (0.0, 4.0, B + 1.2), (-3.5, 7.0, B + 1.0),
                    (0.0, 4.5, UC + 0.6), (3.0, 4.0, UC + 0.5)):
        BH.actor('TargetPoint', K.mat(L(x, y, z)), 'Haunt', tags=['HH_HauntPoint'] + tags,
                 name=BH._unique_name('Vance_HauntPoint'))
    for x, y, z, yaw in ((-0.5, 8.4, U, 90), (-4.6, 7.9, G, 30), (4.6, 0.8, U, -120), (-4.6, 0.9, B, 60),
                         (0.6, 8.3, B, 180), (-0.5, 0.6, U, 0)):
        BH.actor('TargetPoint', K.mat(L(x, y, z), yaw - 90), 'Haunt', tags=['HH_ApparitionPoint'] + tags,
                 name=BH._unique_name('Vance_ApparitionPoint'))
    # Props that move when nobody looks (alternate transform in Unreal world space).
    def haunt_prop(name, mesh, loc, yaw, alt_loc, alt_yaw, alt_roll=0.0):
        alt = Vector(L(*alt_loc))
        ue = H.to_ue_location(alt)
        BH.actor('HHHauntProp', M @ K.mat(loc, yaw), 'Haunt', mesh=mesh, name=name, tags=tags,
                 properties={'AltLocation': ue, 'AltRotation': [alt_roll, 0.0, -(alt_yaw - 90.0)]})
    haunt_prop('Vance_Haunt_Chair', 'SM_DiningChair', (-3.2, 6.25, G + 0.012), 180, (-3.0, 5.95, G + 0.012), 215)
    haunt_prop('Vance_Haunt_Rocker', 'SM_RockingChair', (2.4, 2.9, U + 0.012), 200, (2.2, 2.4, U + 0.012), 90)
    haunt_prop('Vance_Haunt_Teddy', 'SM_Teddy', (XI1 - 0.6, 2.95, U + 0.55), -100, (0.6, 1.2, U + 0.012), 180)
    haunt_prop('Vance_Haunt_Picture', 'SM_Picture_Frame', (0.04 + 0.0, 4.4, G + 1.5), -90, (0.04, 4.4, G + 1.48), -90,
               alt_roll=14.0)
    # Devices that switch themselves on.
    BH.actor('HHHauntDevice', M @ K.mat((XI1 - 0.22, 1.98, G + 0.766), -90), 'Haunt', mesh='SM_Monitor_Flat',
             name='Vance_Haunt_Monitor', tags=tags,
             properties={'DeviceName': BH.text('monitor'), 'Caption': BH.text('[A screen hisses with static]'),
                         'SwitchOnSound': BH.asset('S_TV_Power_On'), 'Prompt': BH.text('Switch off the monitor')},
             components={'Screen': {'StaticMesh': BH.asset('SM_Screen_Quad'),
                                    'RelativeLocation': H.ue_vector((0.0, -0.013, 0.25)),
                                    'RelativeScale3D': [0.52, 1.0, 0.31]},
                         'Audio': {'Sound': BH.asset('A_TV_Static_Loop')},
                         'Glow': {'RelativeLocation': H.ue_vector((0.0, -0.3, 0.27))}})
    BH.actor('HHHauntDevice', M @ K.mat((XI1 - 0.3, 7.75, G + 0.972), -90), 'Haunt', mesh='SM_Radio_Vintage',
             name='Vance_Haunt_Radio', tags=tags,
             properties={'DeviceName': BH.text('radio'), 'Caption': BH.text('[The radio crackles to life]'),
                         'SwitchOnSound': BH.asset('S_Radio_Tune'), 'Prompt': BH.text('Switch off the radio')},
             components={'Audio': {'Sound': BH.asset('A_Radio_Static_Loop')},
                         'Glow': {'RelativeLocation': H.ue_vector((0.0, -0.2, 0.15))}})
    # The thing in the house.
    BH.actor('HHApparition', K.mat(L(-0.5, 8.4, U), 0.0), 'Haunt', name='Vance_Apparition', tags=tags,
             properties={'IdleAnim': BH.asset('A_HH_Idle'), 'GhostMaterial': BH.asset('MI_Apparition'),
                         'AppearSound': BH.asset('A_Haunt_Breath_02'), 'HuntSpeed': 135.0, 'CatchDistance': 110.0},
             components={'Body': {'SkeletalMeshAsset': BH.asset('SK_HH_Body')},
                         'Presence': {'Sound': BH.asset('A_Electric_Buzz_Loop')}})
