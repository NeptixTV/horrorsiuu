"""
Downtown Hollowmere: the bigger town east of the old centre (part of L_Town).

Plan (Blender metres, +X east, +Y north; street level TZ, sidewalks SW):

            Orchard Lane (y 84..92)
   houses   +------------------------+  Pine
   [bank]   |  Gazette Pharmacy Barber   |  St.
 ===== Main Street (y 16.5..24.5) ==[lights at Elm]=====[stop at Pine]==== roadworks
   Willow   |      Pick & Save         |
   Park     |   (car park in front)    |
            +------------------------+
            Mill Road (y -64..-56)
         Elm Ave (x 140..148)        Pine St (x 232..240)

Everything that can be entered can be used: the bank (teller window and the ATM outside open
the account screen), Pick & Save (checkout: the Market shop), Cobb's Hardware on old Main Street
(counter: the Hardware shop). The traffic, the lights, the zebra crossings and the townspeople are
gameplay actors (AHHTrafficCar, AHHTrafficLight, AHHCrosswalk, AHHTownsperson, AHHSwing).
"""
import math
import random

import build_hideout as BH
import placement as PL
import town as T
import worldkit as K
from hhblend import Builder

TZ, SW, WALK = T.TZ, T.SW, T.WALK
MAIN = T.MAIN
X0 = T.XMAX                  # 88: where downtown starts
EAST = 262.0                 # the east end of the world
ELM = (140.0, 148.0)         # Elm Avenue road (x)
PINE = (232.0, 240.0)        # Pine Street road (x)
ORCHARD = (84.0, 92.0)       # Orchard Lane road (y)
MILL = (-64.0, -56.0)        # Mill Road road (y)
YMIN, YMAX = -72.0, 100.0
ELM_C = (ELM[0] + ELM[1]) / 2
PINE_C = (PINE[0] + PINE[1]) / 2
MAIN_C = (MAIN[0] + MAIN[1]) / 2
STOP = 7.0                   # stop lines this far from a junction's centre (AHHRoadNode::StopLine)

rng = random.Random(2031)


def a(name):
    return BH.asset(name)


# =======================================================================================
# Streets

def streets():
    main = Builder('SM_Town_MainStreet_East')
    T._road(main, X0, MAIN[0], EAST, MAIN[1])
    for x0, x1 in ((X0, ELM[0] - STOP - 0.5), (ELM[1] + STOP + 0.5, PINE[0] - STOP - 0.5), (PINE[1] + STOP + 0.5, EAST - 9)):
        for dy in (-0.09, 0.09):
            T._paint(main, x0, MAIN_C + dy - 0.05, x1, MAIN_C + dy + 0.05, 'MI_Road_Paint_Yellow')
    # Main/Elm: zebra crossings on all four arms, stop lines in front of them.
    T._crosswalk(main, 'Y', MAIN[0] + 0.3, MAIN[1] - 0.3, ELM[0] - 2.1, ELM[0] - 0.2)
    T._crosswalk(main, 'Y', MAIN[0] + 0.3, MAIN[1] - 0.3, ELM[1] + 0.2, ELM[1] + 2.1)
    T._paint(main, ELM_C - STOP - 0.3, MAIN[0] + 0.2, ELM_C - STOP, MAIN_C - 0.15)
    T._paint(main, ELM_C + STOP, MAIN_C + 0.15, ELM_C + STOP + 0.3, MAIN[1] - 0.2)
    PL.static_m(PL.unique(main, nanite=True), K.mat(), 'Ground')

    elm = Builder('SM_Town_ElmAvenue')
    T._road(elm, ELM[0], MILL[1], ELM[1], MAIN[0])
    T._road(elm, ELM[0], MAIN[1], ELM[1], ORCHARD[0])
    T._dashes(elm, 'Y', MILL[1] + 3, MAIN[0] - STOP - 1, ELM_C, 3.0, 6.0, 0.12, 'MI_Road_Paint_Yellow')
    T._dashes(elm, 'Y', MAIN[1] + STOP + 1, ORCHARD[0] - 3, ELM_C, 3.0, 6.0, 0.12, 'MI_Road_Paint_Yellow')
    T._crosswalk(elm, 'X', ELM[0] + 0.3, ELM[1] - 0.3, MAIN[0] - 2.2, MAIN[0] - 0.3)
    T._crosswalk(elm, 'X', ELM[0] + 0.3, ELM[1] - 0.3, MAIN[1] + 0.3, MAIN[1] + 2.2)
    T._paint(elm, ELM_C + 0.15, MAIN_C - STOP - 0.3, ELM[1] - 0.2, MAIN_C - STOP)
    T._paint(elm, ELM[0] + 0.2, MAIN_C + STOP, ELM_C - 0.15, MAIN_C + STOP + 0.3)
    PL.static_m(PL.unique(elm, nanite=True), K.mat(), 'Ground')

    pine = Builder('SM_Town_PineStreet')
    T._road(pine, PINE[0], MILL[1], PINE[1], MAIN[0])
    T._road(pine, PINE[0], MAIN[1], PINE[1], ORCHARD[0])
    T._dashes(pine, 'Y', MILL[1] + 3, MAIN[0] - STOP - 1, PINE_C, 3.0, 6.0, 0.12, 'MI_Road_Paint_Yellow')
    T._dashes(pine, 'Y', MAIN[1] + STOP + 1, ORCHARD[0] - 3, PINE_C, 3.0, 6.0, 0.12, 'MI_Road_Paint_Yellow')
    # Pine traffic stops for Main Street.
    T._paint(pine, PINE_C + 0.15, MAIN_C - STOP - 0.3, PINE[1] - 0.2, MAIN_C - STOP)
    T._paint(pine, PINE[0] + 0.2, MAIN_C + STOP, PINE_C - 0.15, MAIN_C + STOP + 0.3)
    PL.static_m(PL.unique(pine, nanite=True), K.mat(), 'Ground')

    side = Builder('SM_Town_OrchardMill')
    T._road(side, ELM[0], ORCHARD[0], PINE[1], ORCHARD[1])
    T._road(side, ELM[0], MILL[0], PINE[1], MILL[1])
    for y0, y1 in (ORCHARD, MILL):
        T._dashes(side, 'X', ELM[1] + 3, PINE[0] - 3, (y0 + y1) / 2, 3.0, 6.0, 0.12, 'MI_Road_Paint_Yellow')
    PL.static_m(PL.unique(side, nanite=True), K.mat(), 'Ground')


LOT = (160.0, -13.0, 220.0, 12.0)          # Pick & Save car park (x0, y0, x1, y1), at road level


def blocks():
    # North-west: the bank and two houses on Elm (west side). The old town's tree line is its west edge.
    nw = Builder('SM_Town_Block_DT_NW')
    T._block(nw, X0, MAIN[1], ELM[0], YMAX, 'se')
    PL.static_m(PL.unique(nw, nanite=True), K.mat(), 'Ground')
    # North-east: shops on Main, houses on Orchard and Pine.
    ne = Builder('SM_Town_Block_DT_NE')
    T._block(ne, ELM[1], MAIN[1], PINE[0], ORCHARD[0], 'snwe')
    PL.static_m(PL.unique(ne, nanite=True), K.mat(), 'Ground')
    # South-west: Willow Park and its playground.
    sw = Builder('SM_Town_Block_DT_SW')
    T._block(sw, X0, YMIN, ELM[0], MAIN[0], 'ne')
    PL.static_m(PL.unique(sw, nanite=True), K.mat(), 'Ground')
    # South-east: Pick & Save with its car park (a hole in the lawn at road level, curbed).
    se = Builder('SM_Town_Block_DT_SE')
    T._block(se, ELM[1], MILL[1], PINE[0], MAIN[0], 'snwe', holes=[LOT])
    x0, y0, x1, y1 = LOT
    se.box_minmax((x0, y0, TZ - 0.4), (x1, y1, TZ), 'MI_Asphalt')
    for lo, hi in (((x0 - 0.15, y0 - 0.15), (x1 + 0.15, y0)), ((x0 - 0.15, y1), (x1 + 0.15, y1 + 0.15)),
                   ((x0 - 0.15, y0), (x0, y1)), ((x1, y0), (x1 + 0.15, y1))):
        se.box_minmax((lo[0], lo[1], TZ - 0.3), (hi[0], hi[1], SW), 'MI_Curb', bevel=0.01)
    for i in range(21):
        x = x0 + 1.5 + i * 2.8
        if x > x1 - 1:
            break
        for ya, yb in ((y0 + 0.2, y0 + 5.2), (y1 - 5.2, y1 - 0.2)):
            se.box_minmax((x, ya, TZ), (x + 0.1, yb, TZ + 0.004), 'MI_Road_Paint_White')
    PL.static_m(PL.unique(se, nanite=True), K.mat(), 'Ground')
    # The strips beyond Pine Street and around the edges (woods).
    edge = Builder('SM_Town_Block_DT_Edge')
    T._block(edge, PINE[1], YMIN, EAST, MAIN[0], 'nw', land='MI_Grass')
    T._block(edge, PINE[1], MAIN[1], EAST, YMAX, 'sw', land='MI_Grass')
    T._block(edge, ELM[0], ORCHARD[1], PINE[1], YMAX, 's', land='MI_Grass')
    T._block(edge, ELM[0], YMIN, PINE[1], MILL[0], 'n', land='MI_Grass')
    PL.static_m(PL.unique(edge, nanite=True), K.mat(), 'Ground')


# =======================================================================================
# Hollowmere Savings & Loan (enterable: lobby, teller windows, the vault, the ATM outside)

BANK_ORIGIN = (118.0, MAIN[1] + WALK, SW)      # front facade on the sidewalk, facing Main Street


def bank():
    M = K.mat(BANK_ORIGIN, 0)
    w, d, top = 30.0, 22.0, 6.6
    t = 0.45
    bld = K.Building('Town_Bank', M)
    b = bld.shell
    door = dict(kind='door', a0=-0.96, a1=0.96, z0=0.0, z1=2.3, trim='MI_Stone_Wall')
    transom = dict(kind='window', a0=-0.96, a1=0.96, z0=2.42, z1=3.2, style='picture', mullions=1, trim='MI_Stone_Wall')
    wins = [dict(kind='window', a0=x - 1.1, a1=x + 1.1, z0=1.0, z1=4.8, style='picture', mullions=1, trim='MI_Stone_Wall')
            for x in (-11.0, -5.0, 5.0, 11.0)]
    front = [door, transom] + wins
    K.wall(b, 'X', -w / 2, w / 2, 0.0, t, -0.3, top, front, mat_c0='MI_Stone_Wall', mat_c1='MI_Paint_Bank_Green', split=0.2)
    for o in front:
        bld.add_opening_trim('X', o, 0.0, t, inside_finish=True)
    side = [dict(kind='window', a0=y - 0.9, a1=y + 0.9, z0=2.6, z1=5.2, style='picture', trim='MI_Stone_Wall')
            for y in (5.0, 10.0)]
    for x, c_in in ((-w / 2, -w / 2 + t), (w / 2, w / 2 - t)):
        K.wall(b, 'Y', t, d - t, x, c_in, -0.3, top, side, mat_c0='MI_Stone_Wall', mat_c1='MI_Paint_Bank_Green')
        for o in side:
            bld.add_opening_trim('Y', o, x, c_in, inside_finish=True)
    K.wall(b, 'X', -w / 2, w / 2, d, d - t, -0.3, top, [], mat_c0='MI_Stone_Wall', mat_c1='MI_Paint_Bank_Green')
    K.flat_roof(b, -w / 2, w / 2, 0, d, top, parapet=1.0, mat_wall='MI_Stone_Wall', coping='MI_Concrete_Raw')
    b.box_minmax((-w / 2 + t, t, -0.3), (w / 2 - t, d - t, 0.0), 'MI_Floor_Marble')
    b.box_minmax((-w / 2 + t, t, top - 0.6), (w / 2 - t, d - t, top - 0.25), 'MI_Plaster')
    # Wainscot inside and a cornice outside; pilasters between the windows.
    for x0, x1 in ((-w / 2 + t, -0.96), (0.96, w / 2 - t)):
        bld.trim.box_minmax((x0, t, 0.0), (x1, t + 0.03, 1.0), 'MI_Wood_Dark')
    bld.trim.box_minmax((-w / 2 - 0.15, -0.25, top - 0.7), (w / 2 + 0.15, 0.0, top - 0.4), 'MI_Concrete_Raw', bevel=0.02)
    for x in (-14.4, -8.0, -2.4, 2.4, 8.0, 14.4):
        bld.trim.box_minmax((x - 0.35, -0.18, 0.0), (x + 0.35, 0.0, top - 0.7), 'MI_Stone_Wall', bevel=0.02)
    # The teller line: a partition across the hall with the counters in the middle.
    yc = 13.0
    for x0, x1 in ((-w / 2 + t, -4.5), (4.5, w / 2 - t)):
        ops = [dict(kind='hole', a0=9.5, a1=10.5, z0=0.0, z1=2.1)] if x0 > 0 else []
        K.wall(b, 'X', x0, x1, yc - 0.1, yc + 0.1, 0.0, top - 0.6, ops, mat_c0='MI_Paint_Bank_Green',
               mat_c1='MI_Paint_Bank_Green')
        bld.trim.box_minmax((x0, yc - 0.13, 0.0), (x1, yc - 0.1, 1.0), 'MI_Wood_Dark')
    b.box_minmax((-4.5, yc - 0.1, 2.33), (4.5, yc + 0.1, top - 0.6), 'MI_Paint_Bank_Green')
    PL.finish(bld, 'Bank')
    PL.place('SM_Door_Steel', (9.55, yc, 0.0), M=M, collection='Bank')       # staff door (locked)
    # Double doors.
    for x, r in ((-0.96, 0), (0.96, 180)):
        PL.door_actor(M, dict(hinge=(x, t / 2, 0.0), rot=r), 'SM_Door_Victorian', locked=False, name=BH._unique_name('Door_Bank'))
    PL.place('SM_Sign_Bank', (0.0, -0.18, 3.55), scale=(1.5, 1.0, 1.5), M=M, collection='Bank')
    # Counters: the middle one is the teller station (it carries its own mesh).
    for x in (-3.0, 3.0):
        PL.place('SM_Bank_Counter', (x, yc, 0.0), M=M, collection='Bank')
    cam = BH.camera('CAM_Teller', PL.world(M, (-0.35, yc - 1.6, 1.65)), PL.world(M, (-0.75, yc + 0.9, 1.4)), fov=55)
    PL.actor_m('HHStationActor', M, (0.0, yc, 0.0), 0, collection='Stations', mesh='SM_Bank_Counter', name='Station_BankTeller',
               properties={'Screen': BH.enum('EHHLobbyScreen', 'Bank'), 'bOpenByDayOnly': True, 'bHubOnly': True,
                           'CameraDrift': 0.25, 'Prompt': BH.text('Talk to the teller'),
                           'InteractSound': a('UI_Confirm')},
               components={'ViewCamera': cam})
    # The vault door behind the tellers, the clock above them.
    PL.place('SM_Vault_Door', (0.0, d - t, 0.0), M=M, collection='Bank')
    PL.place('SM_Clock_Wall', (0.0, yc - 0.1, 3.4), M=M, collection='Bank')
    # Lobby: queue posts, check desks, chairs, the manager's desk, plants, posters.
    for x in (-1.6, 0.0):
        PL.place('SM_Stanchion_Pair', (x + 0.8, 10.4, 0.0), M=M, collection='Bank')
    for x in (-8.5, 8.5):
        PL.place('SM_Check_Desk', (x, 6.5, 0.0), rot=90 if x < 0 else -90, M=M, collection='Bank')
    for y in (3.0, 4.2, 5.4, 6.6):
        PL.place('SM_Chair_Wood', (-14.0, y, 0.0), rot=90, M=M, collection='Bank')
    PL.place('SM_Desk_Wood', (11.5, 3.5, 0.0), rot=-90, M=M, collection='Bank')
    PL.place('SM_OfficeChair', (12.6, 3.5, 0.0), rot=-90, M=M, collection='Bank')
    PL.place('SM_DeskLamp', (11.4, 3.0, 0.76), rot=-90, M=M, collection='Bank')
    PL.place('SM_Rug_Large', (11.0, 4.0, 0.0), rot=90, M=M, collection='Bank')
    for x, y in ((-14.0, 1.2), (14.0, 1.2), (-14.0, 11.8), (14.0, 11.8), (-1.6, 1.2), (1.6, 1.2)):
        PL.place('SM_Plant_Pot', (x, y, 0.0), M=M, collection='Bank')
    for x in (-8.0, 8.0):
        PL.place('SM_Poster_Bank', (x, yc - 0.1, 1.4), M=M, collection='Bank')
    PL.place('SM_Poster_Bank', (-w / 2 + t, 9.0, 1.4), rot=90, M=M, collection='Bank')
    # Light: chandeliers in the hall, lamps over the tellers, the facade lamps.
    for x, y in ((-7.0, 6.5), (7.0, 6.5), (0.0, 6.5), (0.0, 17.5)):
        PL.practical_m(M, 'point', 'SM_Ceiling_Light_Home', (x, y, top - 0.6), light_offset=(0, 0, -0.25), intensity=180,
                       temperature=3200, radius=1400, source=8, emissive=12, name=BH._unique_name('Light_Bank'),
                       tags=['HH_DayOnly'])
    for x in (-3.0, 3.0):
        PL.practical_m(M, 'rect', None, (x, yc + 1.5, 2.6), light_dir=(0, 0, -1), intensity=60, temperature=4000, radius=600,
                       rect=(120, 40), shadows=False, name=BH._unique_name('Light_Teller'), tags=['HH_DayOnly'])
    for x in (-3.4, 3.4):
        PL.practical_m(M, 'point', 'SM_Wall_Sconce', (x, -0.0, 2.9), light_offset=(0, -0.12, 0.12), intensity=30,
                       temperature=2700, radius=700, source=2, tags=['HH_StreetLight'], emissive=10)
    # The ATM outside, open all hours.
    PL.place('SM_Bench_Park', (-7.5, -1.0, 0.0), M=M, collection='Street')
    atm_x = 6.9
    cam = BH.camera('CAM_ATM', PL.world(M, (atm_x, -1.25, 1.75)), PL.world(M, (atm_x, -0.35, 1.3)), fov=50)
    PL.actor_m('HHStationActor', M, (atm_x, 0.0, 0.0), 0, collection='Stations', mesh='SM_ATM', name='Station_ATM',
               properties={'Screen': BH.enum('EHHLobbyScreen', 'Bank'), 'bHubOnly': True, 'CameraDrift': 0.15,
                           'Prompt': BH.text('Use the ATM'), 'InteractSound': a('UI_Click')},
               components={'ViewCamera': cam})
    # People: two tellers, a guard at the door, a policeman watching the vault, customers, the manager.
    person(M, (-3.75, yc + 0.9), -90, 'Idle', TELLER_A)
    person(M, (-0.75, yc + 0.9), -90, 'Idle', TELLER_C)
    person(M, (2.25, yc + 0.9), -90, 'Talk', TELLER_B)
    person(M, (1.6, 1.6), 135, 'Guard', GUARD, watch=True)
    person(M, (2.6, d - 1.6), -100, 'Guard', POLICE, watch=True)
    person(M, (-2.6, d - 1.4), -80, 'Idle', GUARD)
    person(M, (2.25, yc - 1.1), 90, 'Talk', CUSTOMERS[0])
    person(M, (0.5, 9.6), 90, 'Idle', CUSTOMERS[1])
    person(M, (-8.5, 7.2), -90, 'Idle', CUSTOMERS[2])
    person(M, (12.55, 3.55), 180, 'SitType', MANAGER)
    BH.emitter(PL.world(M, (0, 8, 2.5)), loop='A_Bank_Roomtone_Loop', name='Bank_Room', scatter=0, volume=(0.5, 0.6))
    BH.ACTORS[-1]['tags'] = ['HH_DayOnly']


# =======================================================================================
# Pick & Save supermarket (enterable; the first checkout is the Market shop)

MARKET_ORIGIN = (190.0, LOT[1] - 4.0, SW)       # front facade (north side), facing the car park


def market():
    M = K.mat(MARKET_ORIGIN, 180)
    w, d, top = 46.0, 30.0, 5.6
    t = 0.35
    bld = K.Building('Town_Market', M)
    b = bld.shell
    ent = (6.0, 10.0)
    front = [dict(kind='hole', a0=ent[0], a1=ent[1], z0=0.0, z1=3.2)]
    for a0, a1 in ((-21.5, -12.0), (-11.0, -1.5), (-0.5, 5.0), (11.0, 21.5)):
        front.append(dict(kind='shop', a0=a0, a1=a1, z0=0.0, z1=3.4, mullions=3, backdrop=None, trim='MI_Trim_White'))
    K.wall(b, 'X', -w / 2, w / 2, 0.0, t, -0.3, top, front, mat_c0='MI_Brick_Red', mat_c1='MI_Paint_White', split=0.1)
    for o in front:
        if o['kind'] != 'hole':
            bld.add_opening_trim('X', o, 0.0, t, inside_finish=False)
    for e in ent:
        bld.trim.box_minmax((e - 0.12, -0.05, 0.0), (e + 0.12, t + 0.05, 3.3), 'MI_Trim_White')
    bld.trim.box_minmax((ent[0] - 0.12, -0.05, 3.2), (ent[1] + 0.12, t + 0.05, 3.4), 'MI_Trim_White')
    for x, c_in in ((-w / 2, -w / 2 + t), (w / 2, w / 2 - t)):
        K.wall(b, 'Y', t, d - t, x, c_in, -0.3, top, [], mat_c0='MI_Brick_Red', mat_c1='MI_Paint_White')
    back = [dict(kind='door', a0=-15.5, a1=-14.6, z0=0.0, z1=2.06, trim='MI_Trim_Dark')]
    K.wall(b, 'X', -w / 2, w / 2, d, d - t, -0.3, top, back, mat_c0='MI_Brick_Red', mat_c1='MI_Paint_White')
    for o in back:
        bld.add_opening_trim('X', o, d, d - t, inside_finish=False)
    K.flat_roof(b, -w / 2, w / 2, 0, d, top, parapet=1.2, mat_wall='MI_Brick_Red')
    b.box_minmax((-w / 2 + t, t, -0.3), (w / 2 - t, d - t, 0.0), 'MI_Floor_Vinyl')
    b.box_minmax((-w / 2 + t, t, top - 0.6), (w / 2 - t, d - t, top - 0.25), 'MI_Plaster')
    # Canopy over the front walk.
    b.box_minmax((-w / 2, -3.2, 3.6), (w / 2, 0.0, 3.85), 'MI_Metal_White')
    for x in (-20.0, -10.0, 0.0, 10.0, 20.0):
        b.box_minmax((x - 0.1, -3.1, 0.0), (x + 0.1, -2.9, 3.6), 'MI_Metal_White')
    PL.finish(bld, 'Market')
    PL.place('SM_Door_Steel', (-14.6, d - t / 2, 0.0), rot=180, M=M, collection='Market')
    # Concrete apron under the canopy, between the store front and the car park.
    apron = Builder('SM_Town_Market_Apron')
    apron.box_minmax((-w / 2, -(MARKET_ORIGIN[1] - LOT[1]) + 0.15, 0.0), (w / 2, 0.0, 0.02), 'MI_Sidewalk')
    PL.place(PL.unique(apron, collision='complex'), M=M, collection='Market')
    PL.place('SM_Sign_Market', ((ent[0] + ent[1]) / 2 - 10.0, -3.25, 3.9), scale=(2.6, 1.0, 2.6), M=M, collection='Market')
    for x in (-18.0, -13.0, 13.0, 18.0):
        PL.place('SM_Poster_Market', (x, -0.01, 1.2), M=M, collection='Market')
    # Checkouts (lanes run towards the door), the first one is the shop counter.
    checks = (-12.0, -7.0, -2.0)
    for i, x in enumerate(checks):
        if i == 0:
            cam = BH.camera('CAM_Market', PL.world(M, (x + 1.6, 5.3, 1.65)), PL.world(M, (x - 0.4, 6.6, 1.1)), fov=55)
            PL.actor_m('HHStationActor', M, (x, 6.5, 0.0), 90, collection='Stations', mesh='SM_Checkout',
                       name='Station_Market',
                       properties={'Screen': BH.enum('EHHLobbyScreen', 'Store'), 'ShopId': 'Market', 'bOpenByDayOnly': True,
                                   'bHubOnly': True, 'CameraDrift': 0.25, 'Prompt': BH.text('Pay at the checkout'),
                                   'InteractSound': a('UI_Confirm')},
                       components={'ViewCamera': cam})
        else:
            PL.place('SM_Checkout', (x, 6.5, 0.0), rot=90, M=M, collection='Market')
        person(M, (x - 0.95, 6.9), 0, 'Idle', CASHIERS[i % len(CASHIERS)])
    # Aisles: six gondola rows of three, aisle signs above, coolers along the back, produce at the door.
    rows = (-16.0, -11.0, -6.0, -1.0, 4.0, 9.0)
    for i, x in enumerate(rows):
        for k, y in enumerate((12.0, 16.0, 20.0)):
            PL.place('SM_Shelf_Grocery' if (i + k) % 2 == 0 else 'SM_Shelf_Grocery_B', (x, y, 0.0), rot=90, M=M,
                     collection='Market')
    for i, x in enumerate((-13.5, -8.5, -3.5, 1.5)):
        PL.place(f'SM_Aisle_Sign_{i + 1}', (x, 16.0, top - 0.6), M=M, collection='Market')
    for x in range(-20, 22, 3):
        PL.place('SM_Cooler', (x + 1.5, d - t, 0.0), M=M, collection='Market')
    for x in (14.5, 17.0):
        PL.place('SM_Produce_Stand', (x, 12.0, 0.0), rot=90, M=M, collection='Market')
    for k in range(5):
        PL.place('SM_Shopping_Cart', (13.0 + k * 0.3, 2.0, 0.0), rot=180, M=M, collection='Market')
    for x in (16.0, 18.5):
        PL.place('SM_Shopping_Cart', (x, -1.8, 0.0), rot=170 + x, M=M, collection='Market')
    # Fluorescent light: emissive fixtures everywhere, a few real lights.
    for x in range(-19, 21, 6):
        for y in (4.0, 10.0, 16.0, 22.0, 27.0):
            PL.place('SM_Lamp_Fluorescent', (x + 1.0, y, top - 0.6), rot=90, M=M, collection='Market', shadow=False)
    for x, y in ((-12.0, 8.0), (0.0, 8.0), (12.0, 8.0), (-10.0, 20.0), (6.0, 20.0)):
        PL.practical_m(M, 'rect', None, (x, y, top - 0.7), light_dir=(0, 0, -1), intensity=110, temperature=4800, radius=1500,
                       rect=(300, 150), shadows=False, name=BH._unique_name('Light_Market'), tags=['HH_DayOnly'])
    # Shoppers and a stock clerk.
    loop = [(12.0, 4.0), (12.0, 23.5), (6.5, 23.5), (6.5, 10.0), (-3.5, 10.0), (-3.5, 23.5), (-13.5, 23.5), (-13.5, 9.5),
            (1.0, 9.5), (8.0, 4.0)]
    walk_loop('HH_Walk_Market', [PL.world(M, (x, y, 0.0)) for x, y in loop])
    walker('HH_Walk_Market', 0.0, CUSTOMERS[3], speed=95)
    walker('HH_Walk_Market', 0.5, CUSTOMERS[4], speed=110)
    person(M, (-8.5, 18.0), 0, 'Kneel', CLERK)
    BH.emitter(PL.world(M, (0, 15, 3.0)), loop='A_Market_Muzak_Loop', name='Market_Muzak', scatter=0, volume=(0.45, 0.55))
    BH.ACTORS[-1]['tags'] = ['HH_DayOnly']
    BH.emitter(PL.world(M, (0, d - 1.0, 1.0)), loop='A_Fridge_Hum_Loop', name='Market_Fridges', scatter=0,
               volume=(0.4, 0.5))
    # The car park: parked cars and a cart corral.
    cars = ('SM_Car_Sedan_Red', 'SM_Car_Hatch_Green', 'SM_Car_Sedan_Silver', 'SM_Car_Hatch_Blue', 'SM_Car_Pickup_Black')
    r = random.Random(41)
    x0, y0, x1, y1 = LOT
    for i in range(20):
        x = x0 + 1.5 + i * 2.8 + 1.4
        if x > x1 - 2:
            break
        for yy, rot in ((y0 + 2.7, 0), (y1 - 2.7, 180)):
            if r.random() < 0.35:
                PL.place(r.choice(cars), (x, yy, TZ), rot=rot + r.uniform(-3, 3), collection='Cars')
    for k in range(4):
        PL.place('SM_Shopping_Cart', (205.0 + k * 0.32, 0.0, TZ), rot=90, collection='Market')
    PL.place('SM_Bin_Metal', (176.0, LOT[1] - 2.0, SW))


# =======================================================================================
# Cobb's Hardware (old Main Street, replaces the closed facade; the counter is the Hardware shop)

def cobbs_hardware():
    cx, y_front = 31.0, MAIN[1] + WALK
    w, d = 18.0, 14.0
    M = K.mat((cx, y_front, SW), 0)
    bld = K.Building('Town_Hardware', M)
    b = bld.shell
    gf = 4.0
    top = gf + 3.1 + 0.2
    door = dict(kind='door', a0=4.4, a1=5.32, z0=0.0, z1=2.06, trim='MI_Trim_Dark')
    front = [dict(kind='shop', a0=-8.2, a1=3.6, z0=0.0, z1=3.5, mullions=3, backdrop=None),
             dict(kind='window', a0=6.0, a1=8.2, z0=0.9, z1=3.0, style='picture', trim='MI_Trim_Dark'), door]
    for x in (-6.2, -2.1, 2.1, 6.2):
        front.append(dict(kind='window', a0=x - 0.55, a1=x + 0.55, z0=gf + 0.85, z1=gf + 2.45, backdrop='curtain',
                          glow=True, trim='MI_Trim_White', lites=(1, 2)))
    K.wall(b, 'X', -w / 2, w / 2, 0.0, 0.3, -0.3, top, front, mat_c0='MI_Brick', mat_c1='MI_Plaster', split=0.1)
    for o in front:
        bld.add_opening_trim('X', o, 0.0, 0.3, inside_finish=o['kind'] == 'door')
    K.wall(bld.trim, 'X', -w / 2 - 0.08, w / 2 + 0.08, 0.0, -0.08, gf - 0.25, gf, [], mat_c0='MI_Trim_Dark')
    for x, c_in in ((-w / 2, -w / 2 + 0.3), (w / 2, w / 2 - 0.3)):
        K.wall(b, 'Y', 0.3, d - 0.3, x, c_in, -0.3, top, [], mat_c0='MI_Brick', mat_c1='MI_Plaster')
    K.wall(b, 'X', -w / 2, w / 2, d, d - 0.3, -0.3, top, [], mat_c0='MI_Brick', mat_c1='MI_Plaster')
    K.flat_roof(b, -w / 2, w / 2, 0, d, top, parapet=0.6, mat_wall='MI_Brick')
    b.box_minmax((-w / 2 + 0.3, 0.3, -0.3), (w / 2 - 0.3, d - 0.3, 0.0), 'MI_Wood_Floor')
    b.box_minmax((-w / 2 + 0.3, 0.3, gf - 0.3), (w / 2 - 0.3, d - 0.3, gf), 'MI_Wood_Floor_Dark')
    PL.finish(bld, 'Shops')
    PL.door_actor(M, dict(hinge=(4.4, 0.15, 0.0), rot=0), 'SM_Door_House_Front', locked=False, name='Door_Hardware')
    PL.place('SM_Sign_Hardware', (-2.3, 0.0, 3.63), M=M, collection='Shops')
    for ax in (-5.0, 1.0):
        PL.place('SM_Awning', (ax, 0.0, 2.75), M=M, collection='Shops')
    # Inside: shelving, pegboards with tools, paint, the counter at the back with the register.
    stock = (('SM_PaintCan', 'SM_PaintCan', 'SM_Bucket'), ('SM_Toolbox', 'SM_Box_B', 'SM_Toolbox'),
             ('SM_Box_A', 'SM_Box_B', 'SM_Box_A'), ('SM_Box_Open', 'SM_Box_A', 'SM_Box_Open'))
    r = random.Random(1952)
    for i, x in enumerate((-6.0, -2.0)):
        for j, y in enumerate((4.5, 8.5)):
            PL.place('SM_Shelf_Metal', (x, y, 0.0), rot=90, M=M, collection='Shops')
            for level, z in enumerate((0.135, 0.615, 1.115, 1.615)):
                for k, dy in enumerate((-0.29, 0.29)):
                    if r.random() < 0.15:
                        continue
                    item = stock[level][(k + i + j) % 3]
                    PL.place(item, (x + r.uniform(-0.05, 0.05), y + dy, z), rot=90 + r.uniform(-8, 8), M=M,
                             collection='Shops')
    face = d - 0.3 - 0.03
    for x in (-7.5, -5.5, -3.5, -1.5):
        PL.place('SM_Pegboard', (x, d - 0.3, 1.1), M=M, collection='Shops')
        for k, (tool, z) in enumerate((('SM_Tool_Hammer', 1.75), ('SM_Tool_Wrench', 1.8), ('SM_Tool_Saw', 1.7),
                                        ('SM_Tool_Pliers', 1.8), ('SM_Tool_Screwdriver', 1.8))):
            PL.place(tool, (x - 0.7 + k * 0.35, face - 0.012, z), M=M, collection='Shops')
            PL.place('SM_PegHook', (x - 0.7 + k * 0.35, face, z + 0.05), M=M, collection='Shops')
    for i in range(6):
        PL.place('SM_PaintCan', (6.5 + (i % 3) * 0.3, 1.2 + (i // 3) * 0.3, 0.0), M=M, collection='Shops')
    PL.place('SM_Barrel', (7.6, 2.4, 0.0), M=M, collection='Shops')
    PL.place('SM_Sawhorse', (7.2, 5.0, 0.0), rot=90, M=M, collection='Shops')
    PL.place('SM_Toolbox', (-6.0, 4.5, 0.86), M=M, collection='Shops')
    cam = BH.camera('CAM_Hardware', PL.world(M, (4.0, 8.4, 1.7)), PL.world(M, (5.2, 10.7, 1.15)), fov=55)
    PL.actor_m('HHStationActor', M, (5.2, 10.8, 0.0), 0, collection='Stations', mesh='SM_Counter', name='Station_Hardware',
               properties={'Screen': BH.enum('EHHLobbyScreen', 'Store'), 'ShopId': 'Hardware', 'bOpenByDayOnly': True,
                           'bHubOnly': True, 'CameraDrift': 0.25, 'Prompt': BH.text("Talk to Mr Cobb"),
                           'InteractSound': a('UI_Confirm')},
               components={'ViewCamera': cam})
    PL.place('SM_CashBox', (5.6, 10.8, 1.0), rot=180, M=M, collection='Shops')
    PL.place('SM_Radio_Vintage', (4.4, 11.0, 1.0), rot=200, M=M, collection='Shops')
    person(M, (5.2, 11.7), -90, 'Idle', COBB)
    for x in (-5.0, 1.0, 6.0):
        PL.place('SM_Lamp_Fluorescent', (x, 7.0, gf - 0.3), M=M, collection='Shops', shadow=False)
    PL.practical_m(M, 'rect', None, (0.0, 7.0, gf - 0.4), light_dir=(0, 0, -1), intensity=90, temperature=4200, radius=1000,
                   rect=(400, 150), shadows=False, name='Light_Hardware', tags=['HH_DayOnly'])
    PL.practical_m(M, 'spot', None, (-2.3, 0.9, 3.3), light_dir=(0, 0.3, -1), intensity=35, temperature=3200, radius=700,
                   cone=(50, 80), source=10, shadows=False, tags=['HH_StreetLight'], name='Town_Hardware_WindowLight')
    return M


# =======================================================================================
# Shops with closed fronts on Main Street east (painted interiors behind glass)

def main_east_shops():
    y = MAIN[1] + WALK
    T.commercial('Town_Gazette', 157.5, y, 14.0, 13.0, floors=2, facade='MI_Brick', sign='SM_Sign_Gazette', awning=False)
    T.commercial('Town_Pharmacy', 173.0, y, 14.0, 13.0, floors=2, facade='MI_Brick_Red', sign='SM_Sign_Pharmacy',
                 awning=True)
    T.commercial('Town_Barber', 187.5, y, 11.0, 12.0, floors=1, facade='MI_Siding_White', sign='SM_Sign_Barber', awning=True)
    for x in (155.0, 160.0):
        PL.place('SM_Newspaper_Box', (x, MAIN[1] + 0.7, SW), rot=180, collection='Street')
    # A barber pole.
    PL.place('SM_Bench_Park', (195.0, MAIN[1] + 1.4, SW), rot=180, collection='Street')


# =======================================================================================
# Willow Park and its playground

PARK = (96.0, -60.0, 136.0, 10.0)


def playground():
    x0, y0, x1, y1 = PARK
    g = Builder('SM_Town_Playground')
    # Rubber safety surface under the equipment, gravel paths from the corner and around it.
    g.box_minmax((102.0, -36.0, 0.0), (132.0, 2.0, 0.03), 'MI_Rubber_Mat')
    for (ax, ay), (bx, by), zz in (((132.0, 8.0), (138.0, 13.5), 0.026), ((118.0, 2.0), (132.0, 8.0), 0.024)):
        length = math.hypot(bx - ax, by - ay)
        ang = math.degrees(math.atan2(by - ay, bx - ax))
        g.box((length + 1.6, 1.8, zz), ((ax + bx) / 2, (ay + by) / 2, zz / 2), 'MI_Gravel', rot=(0, 0, ang))
    g.box_minmax((99.0, -40.0, 0.0), (101.0, 6.0, 0.02), 'MI_Gravel')
    g.box_minmax((99.0, -40.0, 0.0), (135.0, -38.0, 0.02), 'MI_Gravel')
    PL.place(PL.unique(g, collision='complex'), (0, 0, SW), collection='Park')
    z = SW + 0.03
    # Equipment.
    PL.place('SM_Play_SwingFrame', (110.0, -6.0, z), collection='Park')
    for k, dx in enumerate((-0.8, 0.8)):
        BH.actor('HHSwing', K.mat((110.0 + dx, -6.0, z + 2.45)), 'Park', name=f'Swing_{k}', tags=[f'HH_Swing_{k}'],
                 components={'Seat': {'StaticMesh': a('SM_Swing_Seat')}, 'Creak': {'Sound': a('A_Swing_Creak_Loop')}})
    PL.place('SM_Slide', (124.0, -8.0, z), rot=90, collection='Park')
    PL.place('SM_Seesaw', (111.0, -20.0, z), rot=20, collection='Park')
    PL.place('SM_Sandbox', (125.0, -24.0, z), collection='Park')
    PL.place('SM_Climber', (115.0, -30.0, z), collection='Park')
    PL.place('SM_Sign_Playground_Post', (135.0, 12.0, SW), rot=-135, collection='Park')
    for x, y, rot in ((104.0, -14.0, 90), (104.0, -24.0, 90), (130.0, -16.0, -90), (110.0, 0.5, 0)):
        PL.place('SM_Bench_Kids', (x, y, SW), rot=rot, collection='Park')
    for x, y in ((103.0, 0.5), (131.0, -33.5)):
        PL.place('SM_Bin_Metal', (x, y, SW), collection='Park')
    # A low picket fence around the playground with an opening at the path.
    xs = 101.0
    while xs < 133.0:
        if not (116.0 < xs + 1.0 < 120.5):
            PL.place('SM_Fence_Picket', (xs, 2.6, SW), collection='Fences')
        xs += 2.0
    # Trees, lamps.
    r = random.Random(77)
    for _ in range(26):
        x, y = r.uniform(x0 + 2, x1 - 2), r.uniform(y0 + 2, y1 - 2)
        if 99 < x < 135 and -40 < y < 4:
            continue
        PL.place(r.choice(('SM_Tree_Oak', 'SM_Tree_Maple', 'SM_Tree_Maple', 'SM_Tree_Pine')), (x, y, SW), rot=r.uniform(0, 360),
                 collection='Trees')
    for x, y in ((100.0, 4.0), (133.0, 4.0), (100.0, -38.0), (133.0, -38.0)):
        T.lamp(x, y, 0, intensity=380)
    # The children of Hollowmere (by day) and their parents.
    for k, dx in enumerate((-0.8, 0.8)):
        kid_x = 110.0 + dx
        person(None, (kid_x, -6.0 - 0.04), -90, 'SitSwing', KIDS[k], scale=0.62, z=z + 0.5 - 0.29, swing=f'HH_Swing_{k}')
    person(None, (124.6, -23.6), 200, 'SitGround', KIDS[2], scale=0.6, z=z + 0.22)
    person(None, (125.6, -24.6), 60, 'SitGround', KIDS[3], scale=0.64, z=z + 0.22)
    person(None, (104.1, -14.0), 0, 'Sit', PARENTS[0], z=SW)
    person(None, (104.7, -23.4), 0, 'Phone', PARENTS[1], z=SW)
    person(None, (129.0, -12.0), 200, 'Talk', PARENTS[2], z=SW)
    walk_loop('HH_Walk_Playkid', [(106.0, -10.0, z), (128.0, -12.0, z), (130.0, -32.0, z), (108.0, -34.0, z)])
    walker('HH_Walk_Playkid', 0.0, KIDS[4], speed=260, anim=400, scale=0.6, pose='Run')
    BH.emitter((116.0, -16.0, SW + 2.0), loop='A_Playground_Loop', name='Playground', scatter=0, volume=(0.35, 0.45))
    BH.ACTORS[-1]['tags'] = ['HH_DayOnly']


# =======================================================================================
# Houses downtown (exterior only) with lived-in yards

DT_HOUSES = [
    # name, origin, yaw, siding, roof, kind, number
    ('Elm21', (131.0, 62.0, SW), 90, 'MI_Siding_Green', 'MI_Roof_Shingles_Dark', 'gable', '21'),
    ('Elm25', (131.0, 79.0, SW), 90, 'MI_Siding_White', 'MI_Roof_Shingles_Brown', 'hip', '25'),
    ('Orch3', (163.0, ORCHARD[0] - WALK - 6.0, SW), 180, 'MI_Siding_Blue', 'MI_Roof_Shingles_Dark', 'gable', '3'),
    ('Orch5', (181.0, ORCHARD[0] - WALK - 6.0, SW), 180, 'MI_Siding_Grey', 'MI_Roof_Shingles_Brown', 'gable', '5'),
    ('Orch7', (199.0, ORCHARD[0] - WALK - 6.0, SW), 180, 'MI_Siding_Worn', 'MI_Roof_Shingles_Dark', 'hip', '7'),
    ('Pine12', (PINE[0] - WALK - 6.0, 48.0, SW), 90, 'MI_Siding_White', 'MI_Roof_Shingles_Dark', 'gable', '12'),
    ('Pine16', (PINE[0] - WALK - 6.0, 65.0, SW), 90, 'MI_Siding_Green', 'MI_Roof_Shingles_Brown', 'gable', '16'),
]


def houses():
    for i, (name, origin, yaw, siding, roof, kind, number) in enumerate(DT_HOUSES):
        M, h, w, d = T.neighbour(name, origin, yaw, siding, roof, kind, number, 400 + i)
        car = ('SM_Car_Hatch_Blue', None, 'SM_Car_Sedan_Silver', None, 'SM_Car_Pickup_Black', 'SM_Car_Sedan_Red', None)[i]
        T.yard(M, w, d, 17.0, 6.0, 500 + i, car=car, drive_side=1 if i % 2 == 0 else -1)
        # People at home by day: a gardener, someone on the porch, a phone call in the yard; at night
        # one old neighbour stands on her porch and watches the street.
        door_x = T.door_x_of(h)
        if name == 'Elm21':
            person(M, (-2.6, -2.8), 0, 'Kneel', RESIDENTS[0], z=0.0)
        elif name == 'Orch5':
            person(M, (door_x + 1.2, -1.32), -90, 'Sit', RESIDENTS[1], z=h.base)
            PL.place('SM_Patio_Chair', (door_x + 1.2, -1.25, h.base), M=M, collection='Yards')
        elif name == 'Pine12':
            person(M, (1.0, -5.5), -90, 'Phone', RESIDENTS[2], z=0.0)
        elif name == 'Orch7':
            person(M, (door_x + 0.7, -1.0), -90, 'Idle', RESIDENTS[3], z=h.base, when='NightOnly', watch=True)


# =======================================================================================
# Traffic: lights at Main/Elm, crossings, signs; the cars and their rounds

def traffic_control():
    z = SW
    # Mast-arm signals on the four corners of Main/Elm (far side of each approach).
    poles = [  # pole (x, y), arm yaw, head yaw, signal axis
        ((ELM[0] - 2.1, MAIN[0] - 2.1), 0, 180, 1),     # SW pole, arm over southbound Elm, faces north
        ((ELM[1] + 2.1, MAIN[1] + 2.1), 180, 0, 1),     # NE pole, arm over northbound Elm, faces south
        ((ELM[1] + 2.1, MAIN[0] - 2.1), 90, -90, 0),    # SE pole, arm over eastbound Main, faces west
        ((ELM[0] - 2.1, MAIN[1] + 2.1), -90, 90, 0),    # NW pole, arm over westbound Main, faces east
    ]
    for (px, py), arm, head, axis in poles:
        PL.place('SM_TrafficLight_Pole', (px, py, z), rot=arm, collection='Street')
        hx = px + 4.2 * math.cos(math.radians(arm))
        hy = py + 4.2 * math.sin(math.radians(arm))
        BH.actor('HHTrafficLight', K.mat((hx, hy, z + 5.73), head), 'Street', mesh='SM_TrafficLight_Head',
                 mesh_component='Housing', name=BH._unique_name('TrafficLight'),
                 properties={'Axis': axis},
                 components={'RedLamp': {'StaticMesh': a('SM_TrafficLight_Lens_Red')},
                             'AmberLamp': {'StaticMesh': a('SM_TrafficLight_Lens_Amber')},
                             'GreenLamp': {'StaticMesh': a('SM_TrafficLight_Lens_Green')}})
    # Crossing areas (cars stop while anyone is on the stripes): over Main west/east of Elm, over Elm north/south.
    for (cx, cy), rot in (((ELM[0] - 1.15, MAIN_C), 90), ((ELM[1] + 1.15, MAIN_C), 90),
                          ((ELM_C, MAIN[0] - 1.25), 0), ((ELM_C, MAIN[1] + 1.25), 0)):
        BH.actor('HHCrosswalk', K.mat((cx, cy, TZ + 1.0), rot), 'Street', name=BH._unique_name('Crosswalk'),
                 components={'Area': {'BoxExtent': [480.0, 110.0, 150.0]}})
    # The old town's crossings over Ashgrove and Marrow at Main Street.
    for x0, x1 in (T.MAR, T.ASH):
        BH.actor('HHCrosswalk', K.mat(((x0 + x1) / 2, MAIN[1] + 1.4, TZ + 1.0), 0), 'Street', name=BH._unique_name('Crosswalk'),
                 components={'Area': {'BoxExtent': [480.0, 110.0, 150.0]}})
    # Stop signs on Pine Street at Main.
    PL.place('SM_Sign_Stop', (PINE[0] - 0.9, MAIN[1] + 3.6, SW), rot=180, collection='Street')
    PL.place('SM_Sign_Stop', (PINE[1] + 0.9, MAIN[0] - 3.6, SW), rot=0, collection='Street')
    # Street names on the corners.
    for name, (x, y) in (('SM_Sign_Street_Main_Elm', (ELM[0] - 1.0, MAIN[1] + 1.0)),
                         ('SM_Sign_Street_Main_Pine', (PINE[1] + 1.0, MAIN[1] + 1.0)),
                         ('SM_Sign_Street_Orchard_Elm', (ELM[1] + 1.0, ORCHARD[0] - 1.0)),
                         ('SM_Sign_Street_Orchard_Pine', (PINE[0] - 1.0, ORCHARD[0] - 1.0)),
                         ('SM_Sign_Street_Mill_Elm', (ELM[1] + 1.0, MILL[1] + 1.0)),
                         ('SM_Sign_Street_Mill_Pine', (PINE[0] - 1.0, MILL[1] + 1.0))):
        PL.place(name, (x, y, SW), collection='Street')
    PL.place('SM_Sign_Speed', (ELM[1] + 0.9, 40.0, SW), rot=90, collection='Street')
    PL.place('SM_Sign_Speed', (PINE[0] - 0.9, -20.0, SW), rot=-90, collection='Street')


# id, (x, y), links, control, stop approaches
ROAD_NODES = [
    ('M_ELM', (ELM_C, MAIN_C), ['M_PINE', 'E_N', 'E_S'], 'Lights', []),
    ('M_PINE', (PINE_C, MAIN_C), ['M_X', 'P_N', 'P_S'], 'StopSign', ['P_N', 'P_S']),
    ('M_X', (EAST - 12.0, MAIN_C), [], 'None', []),
    ('E_N', (ELM_C, (ORCHARD[0] + ORCHARD[1]) / 2), ['P_N'], 'None', []),
    ('E_S', (ELM_C, (MILL[0] + MILL[1]) / 2), ['P_S'], 'None', []),
    ('P_N', (PINE_C, (ORCHARD[0] + ORCHARD[1]) / 2), [], 'None', []),
    ('P_S', (PINE_C, (MILL[0] + MILL[1]) / 2), [], 'None', []),
]

def road_nodes():
    for nid, (x, y), links, control, approaches in ROAD_NODES:
        BH.actor('HHRoadNode', K.mat((x, y, T.ROAD_Z)), 'Roads', name='Road_' + nid,
                 properties={'NodeId': nid, 'Links': links, 'Control': BH.enum('EHHTrafficControl', control),
                             'StopApproaches': approaches, 'StopLine': STOP * 100.0})


OLD_TOWN = ['M_MAR', 'M_LOT', 'M_PARK', 'M_ASH', 'A1', 'A_VANCE', 'A2', 'A_CH', 'C_RECT', 'R_CH', 'R_MARROW', 'R1']
ROUTES = [
    # route, start index, body
    (['M_ELM', 'M_PINE', 'P_N', 'E_N'], 0, 'SM_Car_Sedan_Red'),
    (['M_ELM', 'M_PINE', 'P_N', 'E_N'], 2, 'SM_Car_Hatch_Green'),
    (['M_ELM', 'E_N', 'P_N', 'M_PINE'], 1, 'SM_Car_Sedan_Silver'),
    (['M_PINE', 'M_ELM', 'E_S', 'P_S'], 0, 'SM_Car_Hatch_Blue'),
    (['M_PINE', 'M_ELM', 'E_S', 'P_S'], 2, 'SM_Car_Pickup_Black'),
    (OLD_TOWN, 0, 'SM_Car_Sedan_Silver'),
    (['M_MAR', 'M_LOT', 'M_PARK', 'M_ASH', 'M_E', 'M_ELM', 'E_S', 'P_S', 'M_PINE', 'P_N', 'E_N', 'M_ELM', 'M_E', 'M_ASH', 'A1',
      'A_VANCE', 'A2', 'A_CH', 'C_RECT', 'R_CH', 'R_MARROW', 'R1'], 0, 'SM_Car_Hatch_Blue'),
    (['M_MAR', 'M_LOT', 'M_PARK', 'M_ASH', 'M_E', 'M_ELM', 'E_S', 'P_S', 'M_PINE', 'P_N', 'E_N', 'M_ELM', 'M_E', 'M_ASH', 'A1',
      'A_VANCE', 'A2', 'A_CH', 'C_RECT', 'R_CH', 'R_MARROW', 'R1'], 11, 'SM_Car_Sedan_Red'),
]
CAR_SHAPES = {   # BoxExtent (cm), wheelbase, track (the town's car meshes)
    'SM_Car_Sedan_Red': ([228.0, 88.0, 50.0], 275.0, 150.0),
    'SM_Car_Sedan_Silver': ([228.0, 88.0, 50.0], 275.0, 150.0),
    'SM_Car_Hatch_Blue': ([200.0, 86.0, 52.0], 245.0, 148.0),
    'SM_Car_Hatch_Green': ([200.0, 86.0, 52.0], 245.0, 148.0),
    'SM_Car_Pickup_Black': ([265.0, 95.0, 60.0], 320.0, 160.0),
}


def traffic_cars():
    for i, (route, start, body) in enumerate(ROUTES):
        extent, base, track = CAR_SHAPES[body]
        BH.actor('HHTrafficCar', K.mat((EAST - 6.0, YMIN + 4.0 + i * 6.0, TZ + 0.85)), 'NPC', name=f'Traffic_{i}',
                 properties={'BodyMesh': a(body), 'Route': list(route), 'StartIndex': start,
                             'EngineLoop': a('S_Van_Engine_Loop'), 'BumpSound': a('S_Car_Bump_01'),
                             'HornSound': a('S_Car_Horn_01'), 'BoxExtent': extent, 'Wheelbase': base, 'TrackWidth': track,
                             'CruiseSpeed': 780.0 + 30.0 * (i % 3)})


# =======================================================================================
# The townspeople (AHHTownsperson): outfits and helpers

def outfit(skin, hair, top, pants, shoes, *extra):
    return [skin, hair, top, pants, shoes] + list(extra)


TELLER_A = outfit('CO_Skin_Fair', 'CO_Hair_Tied_Red', 'CO_NPC_Top_Teller', 'CO_Pants_Slacks', 'CO_Shoes_Dress', 'CO_Acc_Glasses')
TELLER_C = outfit('CO_Skin_Olive', 'CO_Hair_Tied_Black', 'CO_NPC_Top_Teller', 'CO_Pants_Slacks', 'CO_Shoes_Dress')
TELLER_B = outfit('CO_Skin_Umber', 'CO_Hair_Crop_Black', 'CO_NPC_Top_Teller', 'CO_Pants_Slacks', 'CO_Shoes_Dress')
GUARD = outfit('CO_Skin_Tan', 'CO_Hair_Crop_Brown', 'CO_NPC_Top_Guard', 'CO_Pants_Cargo_Black', 'CO_Shoes_Boots',
               'CO_NPC_Jacket_Guard', 'CO_NPC_Hat_Guard', 'CO_Acc_Earpiece')
POLICE = outfit('CO_Skin_Fair', 'CO_Hair_Crop_Brown', 'CO_NPC_Top_Police', 'CO_NPC_Pants_Police', 'CO_Shoes_Dress',
                'CO_NPC_Jacket_Police', 'CO_NPC_Hat_Police')
MANAGER = outfit('CO_Skin_Porcelain', 'CO_Hair_Messy_Grey', 'CO_Top_Tee_White', 'CO_Pants_Slacks', 'CO_Shoes_Dress',
                 'CO_Jacket_Leather', 'CO_Acc_Watch')
CUSTOMERS = [
    outfit('CO_Skin_Olive', 'CO_Hair_Curly_Black', 'CO_Top_Sweater_Stripe', 'CO_Pants_Jeans', 'CO_Shoes_Sneakers_White'),
    outfit('CO_Skin_Deep', 'CO_Hair_Crop_Black', 'CO_Top_Hoodie_Grey', 'CO_Pants_Jeans_Black', 'CO_Shoes_Sneakers_Black',
           'CO_Hat_Cap_Red'),
    outfit('CO_Skin_Fair', 'CO_Hair_Messy_Blond', 'CO_Top_Tee_Ash', 'CO_Pants_Cargo_Olive', 'CO_Shoes_Boots', 'CO_Jacket_Work'),
    outfit('CO_Skin_Tan', 'CO_Hair_Tied_Black', 'CO_Top_Turtleneck', 'CO_Pants_Jeans', 'CO_Shoes_Dress', 'CO_Jacket_Rain'),
    outfit('CO_Skin_Porcelain', 'CO_Hair_Messy_Grey', 'CO_Top_Sweater_Stripe', 'CO_Pants_Slacks', 'CO_Shoes_Dress',
           'CO_Hat_FlatCap'),
]
CASHIERS = [
    outfit('CO_Skin_Fair', 'CO_Hair_Tied_Black', 'CO_NPC_Top_Market', 'CO_Pants_Jeans_Black', 'CO_Shoes_Sneakers_Black'),
    outfit('CO_Skin_Umber', 'CO_Hair_Curly_Black', 'CO_NPC_Top_Market', 'CO_Pants_Jeans', 'CO_Shoes_Sneakers_White'),
    outfit('CO_Skin_Olive', 'CO_Hair_Crop_Brown', 'CO_NPC_Top_Market', 'CO_Pants_Slacks', 'CO_Shoes_Sneakers_Black'),
]
CLERK = outfit('CO_Skin_Tan', 'CO_Hair_Messy_Blond', 'CO_NPC_Top_Market', 'CO_Pants_Cargo_Black', 'CO_Shoes_Boots')
COBB = outfit('CO_Skin_Fair', 'CO_Hair_Messy_Grey', 'CO_Top_Tee_Ash', 'CO_Pants_Jeans', 'CO_Shoes_Boots', 'CO_Jacket_Work',
              'CO_Acc_Glasses')
KIDS = [
    outfit('CO_Skin_Fair', 'CO_Hair_Messy_Blond', 'CO_NPC_Top_Kid_Yellow', 'CO_Pants_Jeans', 'CO_Shoes_Sneakers_White'),
    outfit('CO_Skin_Deep', 'CO_Hair_Curly_Black', 'CO_NPC_Top_Kid_Red', 'CO_Pants_Jeans_Black', 'CO_Shoes_Sneakers_White'),
    outfit('CO_Skin_Olive', 'CO_Hair_Tied_Red', 'CO_NPC_Top_Kid_Blue', 'CO_Pants_Jeans', 'CO_Shoes_Sneakers_Black'),
    outfit('CO_Skin_Porcelain', 'CO_Hair_Crop_Brown', 'CO_NPC_Top_Kid_Red', 'CO_Pants_Cargo_Olive', 'CO_Shoes_Sneakers_White'),
    outfit('CO_Skin_Tan', 'CO_Hair_Crop_Black', 'CO_NPC_Top_Kid_Yellow', 'CO_Pants_Jeans', 'CO_Shoes_Sneakers_Black',
           'CO_Hat_Cap_Red'),
]
PARENTS = [
    outfit('CO_Skin_Fair', 'CO_Hair_Tied_Red', 'CO_Top_Sweater_Stripe', 'CO_Pants_Jeans', 'CO_Shoes_Boots', 'CO_Jacket_Rain'),
    outfit('CO_Skin_Deep', 'CO_Hair_Crop_Black', 'CO_Top_Hoodie_Forest', 'CO_Pants_Jeans', 'CO_Shoes_Sneakers_White'),
    outfit('CO_Skin_Olive', 'CO_Hair_Tied_Black', 'CO_Top_Turtleneck', 'CO_Pants_Slacks', 'CO_Shoes_Dress', 'CO_Jacket_Bomber'),
]
RESIDENTS = [
    outfit('CO_Skin_Porcelain', 'CO_Hair_Messy_Grey', 'CO_Top_Sweater_Stripe', 'CO_Pants_Cargo_Olive', 'CO_Shoes_Boots',
           'CO_Hat_FlatCap', 'CO_Gloves_Work'),
    outfit('CO_Skin_Tan', 'CO_Hair_Crop_Brown', 'CO_Top_Tee_White', 'CO_Pants_Jeans', 'CO_Shoes_Sneakers_White'),
    outfit('CO_Skin_Fair', 'CO_Hair_Tied_Black', 'CO_Top_Hoodie_Grey', 'CO_Pants_Jeans_Black', 'CO_Shoes_Sneakers_Black'),
    outfit('CO_Skin_Porcelain', 'CO_Hair_Messy_Grey', 'CO_Top_Turtleneck', 'CO_Pants_Slacks', 'CO_Shoes_Dress', 'CO_Acc_Chain'),
]
WALKERS = [
    outfit('CO_Skin_Umber', 'CO_Hair_Crop_Black', 'CO_Top_Tee_Black', 'CO_Pants_Jeans', 'CO_Shoes_Sneakers_White', 'CO_Jacket_Bomber'),
    outfit('CO_Skin_Fair', 'CO_Hair_Messy_Blond', 'CO_Top_Hoodie_Forest', 'CO_Pants_Cargo_Olive', 'CO_Shoes_Boots',
           'CO_Back_Rucksack'),
    outfit('CO_Skin_Olive', 'CO_Hair_Tied_Red', 'CO_Top_Sweater_Stripe', 'CO_Pants_Jeans_Black', 'CO_Shoes_Dress', 'CO_Jacket_Rain'),
    outfit('CO_Skin_Porcelain', 'CO_Hair_Messy_Grey', 'CO_Top_Turtleneck', 'CO_Pants_Slacks', 'CO_Shoes_Dress', 'CO_Hat_FlatCap'),
    outfit('CO_Skin_Deep', 'CO_Hair_Curly_Black', 'CO_Top_Hoodie_Black', 'CO_Pants_Jeans', 'CO_Shoes_Sneakers_Black',
           'CO_Hat_Beanie_Mustard'),
    outfit('CO_Skin_Tan', 'CO_Hair_Crop_Brown', 'CO_Top_Tee_White', 'CO_Pants_Jeans', 'CO_Shoes_Sneakers_White', 'CO_Jacket_Work'),
]


def person(M, xy, yaw, pose, cosmetics, z=0.0, scale=1.0, swing=None, when='DayOnly', watch=False, scene=None, name=None):
    """A townsperson standing / sitting at xy (in M's frame if given; z relative to M, or world z without M)."""
    loc = (xy[0], xy[1], z)
    m = (M @ K.mat(loc, yaw)) if M is not None else K.mat(loc, yaw)
    props = {'CosmeticIds': list(cosmetics), 'Pose': pose, 'BodyScale': float(scale),
             'VisibleWhen': BH.enum('EHHIntelTime', 'Any' if scene else when), 'bWatchPlayers': bool(watch)}
    if swing:
        props['SwingName'] = swing
    if scene:
        props['SceneTag'] = scene
    return BH.actor('HHTownsperson', m, 'NPC', name=name or BH._unique_name('Townsperson'), properties=props)


def walk_loop(tag, points):
    for i, p in enumerate(points):
        BH.tag_point(f'{tag}_{i}', p if len(p) == 3 else (p[0], p[1], SW))


def walker(tag, offset, cosmetics, speed=125.0, anim=150.0, scale=1.0, pose='Walk', when='DayOnly'):
    return BH.actor('HHTownsperson', K.mat((0, 0, -100)), 'NPC', name=BH._unique_name('Walker'),
                    properties={'CosmeticIds': list(cosmetics), 'Pose': pose, 'BodyScale': float(scale), 'PathTag': tag,
                                'PathOffset': float(offset), 'WalkSpeed': float(speed), 'AnimSpeed': float(anim),
                                'VisibleWhen': BH.enum('EHHIntelTime', when)})


def pedestrians():
    """People walking the sidewalks (closed loops; the ones round Main/Elm use the zebra crossings)."""
    walk_loop('HH_Walk_Junction', [(ELM[0] - 1.15, MAIN[0] - 1.25), (ELM[1] + 1.15, MAIN[0] - 1.25),
                                   (ELM[1] + 1.15, MAIN[1] + 1.25), (ELM[0] - 1.15, MAIN[1] + 1.25)])
    walker('HH_Walk_Junction', 0.0, WALKERS[0], speed=120)
    walk_loop('HH_Walk_DT_NE', [(ELM[1] + 1.25, MAIN[1] + 1.25), (PINE[0] - 1.25, MAIN[1] + 1.25),
                                (PINE[0] - 1.25, ORCHARD[0] - 1.25), (ELM[1] + 1.25, ORCHARD[0] - 1.25)])
    walker('HH_Walk_DT_NE', 0.15, WALKERS[1], speed=130)
    walker('HH_Walk_DT_NE', 0.6, WALKERS[2], speed=115)
    walk_loop('HH_Walk_DT_SE', [(ELM[1] + 1.25, MAIN[0] - 1.25), (ELM[1] + 1.25, MILL[1] + 1.25),
                                (PINE[0] - 1.25, MILL[1] + 1.25), (PINE[0] - 1.25, MAIN[0] - 1.25)])
    walker('HH_Walk_DT_SE', 0.3, WALKERS[3], speed=105)
    # Old town: round Founders' Park block (past the museum) and through the park.
    walk_loop('HH_Walk_Park', [(T.MAR[1] + 1.25, MAIN[1] + 1.25), (T.ASH[0] - 1.25, MAIN[1] + 1.25),
                               (T.ASH[0] - 1.25, T.CHURCH[0] - 1.25), (T.MAR[1] + 1.25, T.CHURCH[0] - 1.25)])
    walker('HH_Walk_Park', 0.0, WALKERS[4], speed=125)
    walker('HH_Walk_Park', 0.45, WALKERS[5], speed=140)
    walk_loop('HH_Walk_Gazebo', [(-1.0, 53.0, SW + 0.03), (-1.0, 68.0, SW + 0.03), (-6.0, 73.0, SW + 0.03),
                                 (-1.0, 78.0, SW + 0.03), (-1.0, 93.0, SW + 0.03), (3.0, 93.0, SW + 0.03), (3.0, 53.0, SW + 0.03)])
    walker('HH_Walk_Gazebo', 0.0, CUSTOMERS[0], speed=100)
    # Mr Pruitt's neighbours on Ashgrove: someone sweeping, someone on the porch (old town houses).
    person(None, (35.0 + 1.27, 71.0 - 0.6), 0, 'Sit', RESIDENTS[1], z=SW + 0.6)
    PL.place('SM_Patio_Chair', (35.0 + 1.2, 71.0 - 0.6, SW + 0.6), rot=90, collection='Yards')


# =======================================================================================
# Street furniture, edges, sound

def furniture():
    for x in range(int(X0) + 6, int(EAST) - 8, 20):
        if not (ELM[0] - 4 < x < ELM[1] + 4 or PINE[0] - 4 < x < PINE[1] + 4):
            T.lamp(x, MAIN[0] - 1.0, 90)
        if not (ELM[0] - 4 < x + 10 < ELM[1] + 4 or PINE[0] - 4 < x + 10 < PINE[1] + 4):
            T.lamp(x + 10, MAIN[1] + 1.0, -90)
    for y in list(range(-50, 10, 20)) + list(range(34, 82, 20)):
        T.lamp(ELM[0] - 1.0, y, 0)
        T.lamp(PINE[1] + 1.0, y + 10, 180)
    for x in range(156, 230, 22):
        T.lamp(x, ORCHARD[0] - 1.0, 90)
        T.lamp(x + 11, MILL[1] + 1.0, -90)
    for x in (100.0, 152.0, 210.0):
        PL.place('SM_Hydrant', (x, MAIN[0] - 0.5, SW), rot=90, collection='Street')
    for x in (125.0, 166.0, 228.0):
        PL.place('SM_Bin_Metal', (x, MAIN[1] + 0.6, SW), collection='Street')
    PL.place('SM_BusStop', (200.0, MAIN[0] - 1.6, SW), rot=180, collection='Street')
    PL.place('SM_Mailbox_Public', (150.0, MAIN[1] + 0.8, SW), rot=180, collection='Street')
    for x in range(100, 132, 12):
        PL.place('SM_Tree_Maple', (x, MAIN[0] - 2.0, SW), rot=x * 7, collection='Trees')
    for x in (154.0, 170.0):
        PL.place('SM_Planter_Box', (x, MAIN[0] - 0.9, SW), collection='Street')
    for x in range(212, 230, 6):
        PL.place('SM_Tree_Oak', (x, MAIN[0] - 2.0, SW), rot=x * 11, collection='Trees')
    for x in (130.0, 200.0):
        PL.place('SM_Manhole', (x, MAIN_C + 2.0, TZ), collection='Street')


def edges():
    r = random.Random(4711)
    trees = ('SM_Tree_Pine', 'SM_Tree_Pine', 'SM_Tree_Oak', 'SM_Tree_Dead', 'SM_Tree_Maple')
    ring = []
    for x in range(int(X0), int(EAST) + 1, 7):
        ring.append((x + r.uniform(-2, 2), YMIN + r.uniform(1, 5)))
        ring.append((x + r.uniform(-2, 2), YMAX - r.uniform(1, 5)))
    for y in range(int(YMIN), int(YMAX) + 1, 7):
        if MAIN[0] - 3 < y < MAIN[1] + 3:
            continue
        ring.append((EAST - r.uniform(1, 5), y + r.uniform(-2, 2)))
        if y < T.YMIN - 2:
            ring.append((X0 + r.uniform(1, 5), y + r.uniform(-2, 2)))
    for _ in range(30):
        ring.append((r.uniform(PINE[1] + 3, EAST - 3), r.uniform(YMIN + 3, YMAX - 3)))
    for x, y in ring:
        if PINE[1] < x < EAST and MAIN[0] - 4 < y < MAIN[1] + 4:
            continue
        PL.place(r.choice(trees), (x, y, SW), rot=r.uniform(0, 360), scale=r.uniform(0.85, 1.25), collection='Woods')
    # Main Street ends at roadworks past Pine Street.
    for k in range(4):
        PL.place('SM_Sawhorse', (EAST - 6.0, MAIN[0] + 1.2 + k * 2.0, TZ), rot=90, collection='Street')
        PL.place('SM_Traffic_Cone', (EAST - 7.2, MAIN[0] + 2.0 + k * 2.0, TZ), collection='Street')
    # Boundary walls (pawns only, invisible).
    walls = [((EAST - 3, (YMIN + YMAX) / 2, SW + 3), (0.5, (YMAX - YMIN) / 2, 6)),
             (((X0 + EAST) / 2, YMIN + 3, SW + 3), ((EAST - X0) / 2, 0.5, 6)),
             (((X0 + EAST) / 2, YMAX - 3, SW + 3), ((EAST - X0) / 2, 0.5, 6)),
             ((X0 - 3, (YMIN + T.YMIN) / 2 + 1.5, SW + 3), (0.5, (T.YMIN + 3 - YMIN) / 2, 6)),
             ((X0 - 3, (YMAX - 3 + T.YMAX) / 2, SW + 3), (0.5, (T.YMAX - YMAX) / 2 + 1.5, 6))]
    for (x, y, z), (ex, ey, ez) in walls:
        BH.actor('HHBoundaryWall', K.mat((x, y, z)), 'Gameplay', name=BH._unique_name('Boundary'),
                 components={'Wall': {'BoxExtent': [ex * 100.0, ey * 100.0, ez * 100.0]}})


def ambience():
    for x, y in ((120.0, -20.0), (190.0, 55.0), (190.0, -30.0), (250.0, 20.0)):
        BH.emitter((x, y, SW + 4), loop='A_Town_Birds_Day_Loop', name='Birds_Day', scatter=0)
        BH.ACTORS[-1]['tags'] = ['HH_DayOnly']
        BH.emitter((x, y, SW + 1), loop='A_Town_Crickets_Night_Loop', name='Crickets_Night', scatter=0)
        BH.ACTORS[-1]['tags'] = ['HH_NightOnly']
    BH.emitter((PINE_C, MAIN_C, SW + 1), oneshots=['A_Car_Pass_Distant_01', 'A_Car_Pass_Distant_02', 'A_Car_Pass_Distant_03'],
               interval=(30, 90), scatter=3000, caption='[A car passes somewhere]', name='Cars')


def decals():
    r = random.Random(808)
    for _ in range(14):
        BH.decal('MI_Decal_Puddle', (r.uniform(X0 + 4, EAST - 10), r.choice((MAIN[0] + 0.9, MAIN[1] - 0.9)), TZ + 0.02),
                 (r.uniform(1.2, 2.8), r.uniform(1.0, 2.0)), rot=(0, 0, r.uniform(0, 360)), depth=0.4, name='Decal_Puddle')
    for _ in range(10):
        BH.decal('MI_Decal_Puddle', (r.uniform(LOT[0] + 2, LOT[2] - 2), r.uniform(LOT[1] + 2, LOT[3] - 2), TZ + 0.02),
                 (r.uniform(1.4, 3.0), r.uniform(1.0, 2.2)), rot=(0, 0, r.uniform(0, 360)), depth=0.4, name='Decal_Puddle')
    for _ in range(12):
        BH.decal('MI_Decal_Leaves', (r.uniform(PARK[0] + 2, PARK[2] - 2), r.uniform(PARK[1] + 2, PARK[3] - 2), SW + 0.05),
                 (r.uniform(2.0, 3.5), r.uniform(1.5, 3.0)), rot=(0, 0, r.uniform(0, 360)), depth=0.4, name='Decal_Leaves')


# =======================================================================================
# The morning after a job at the Vance house (AHHAftermathScene + tagged pieces)

VANCE_SCENE = 'HH_Scene_Vance'


def vance_aftermath():
    vx, vy, _ = T.VANCE_ORIGIN
    lane_x = T.ASH[1] - 1.3                      # patrol car at the curb, east lane, nose north
    car = (lane_x, vy - 2.0)
    police = VANCE_SCENE + '_Police'
    PL.place('SM_Car_Police', (car[0], car[1], TZ), rot=180, collection='Scene', tags=[police])
    scene_z = TZ + 1.55
    flash = (vx - 3.6, vy - 1.4, SW + 1.55)
    BH.actor('HHAftermathScene', K.mat((car[0], car[1], scene_z)), 'Scene', name='Aftermath_Vance',
             properties={'MissionId': 'JOB_Vance', 'SceneTag': VANCE_SCENE, 'ClosedNodes': ['A_VANCE'],
                         'FlashOffset': [(flash[0] - car[0]) * 100.0, -(flash[1] - car[1]) * 100.0, (flash[2] - scene_z) * 100.0],
                         'FlashSound': a('S_Camera_Flash_01')},
             components={'Radio': {'Sound': a('A_Police_Radio_Loop')}})
    # Tape along the front fence and across the drive, evidence markers by the porch.
    fence_x = vx - 6.5 + 0.55
    for y0 in (vy - 9.0, vy - 3.0, vy + 3.0):
        PL.place('SM_Police_Tape', (fence_x, y0, SW), rot=90, collection='Scene', tags=[police])
    PL.place('SM_Evidence_Markers', (vx - 2.2, vy + 1.0, SW), rot=20, collection='Scene', tags=[police])
    PL.place('SM_Evidence_Markers', (vx - 4.0, vy - 3.5, SW), rot=-40, collection='Scene', tags=[police])
    # A crew member who didn't make it, under a sheet on the lawn.
    PL.place('SM_Body_Covered', (vx - 3.0, vy - 2.6, SW + 0.01), rot=75, collection='Scene', tags=[VANCE_SCENE + '_Body'])
    # The road closed both ways.
    for y in (vy - 14.0, vy + 14.0):
        for k in range(3):
            PL.place('SM_Sawhorse', (T.ASH[0] + 1.4 + k * 2.6, y, TZ), collection='Scene', tags=[VANCE_SCENE + '_Road'])
            PL.place('SM_Traffic_Cone', (T.ASH[0] + 2.7 + k * 2.6, y + (1.0 if y > vy else -1.0), TZ), collection='Scene',
                     tags=[VANCE_SCENE + '_Road'])
    # The haunting: someone poured salt across the porch steps.
    PL.place('SM_Salt_Line', (vx - 2.3, vy, SW + 0.02), rot=90, collection='Scene', tags=[VANCE_SCENE + '_Haunt'])
    # People: Margaret Vance telling a reporter, an officer, forensics, a priest when the house showed itself.
    margaret = ['CO_Skin_Porcelain', 'CO_Hair_Messy_Grey', 'CO_Top_Turtleneck', 'CO_Pants_Slacks', 'CO_Shoes_Dress', 'CO_Acc_Chain']
    reporter = outfit('CO_Skin_Tan', 'CO_Hair_Tied_Black', 'CO_Top_Tee_White', 'CO_Pants_Jeans_Black', 'CO_Shoes_Dress',
                      'CO_Jacket_Rain')
    forensic = outfit('CO_Skin_Fair', 'CO_Hair_Crop_Brown', 'CO_NPC_Top_Forensic', 'CO_NPC_Pants_Forensic', 'CO_Shoes_Boots',
                      'CO_Gloves_Latex')
    priest = outfit('CO_Skin_Porcelain', 'CO_Hair_Messy_Grey', 'CO_Top_Turtleneck', 'CO_Pants_Slacks', 'CO_Shoes_Dress',
                    'CO_Acc_Glasses')
    walk_x = T.ASH[1] + 1.2
    person(None, (walk_x, vy + 4.0), -90, 'Talk', margaret, z=SW, scene=VANCE_SCENE, name='Scene_Vance_Owner')
    person(None, (walk_x, vy + 2.9), 90, 'Talk', reporter, z=SW, scene=VANCE_SCENE, name='Scene_Vance_Reporter')
    person(None, (walk_x + 0.2, vy - 4.5), 180, 'Guard', POLICE, z=SW, watch=True, scene=police, name='Scene_Vance_Officer')
    person(None, (flash[0] - 0.3, flash[1]), -50, 'Photo', forensic, z=SW, scene=police, name='Scene_Vance_Photographer')
    person(None, (vx - 3.4, vy - 3.6), 80, 'Kneel', forensic, z=SW, scene=VANCE_SCENE + '_Body', name='Scene_Vance_Forensic')
    person(None, (vx - 1.6, vy + 0.4), 180, 'Idle', priest, z=SW + 0.6, scene=VANCE_SCENE + '_Haunt', name='Scene_Vance_Priest')


# =======================================================================================

def build():
    cobbs_hardware()
    streets()
    blocks()
    bank()
    market()
    main_east_shops()
    playground()
    houses()
    traffic_control()
    road_nodes()
    traffic_cars()
    pedestrians()
    furniture()
    edges()
    ambience()
    decals()
    vance_aftermath()
