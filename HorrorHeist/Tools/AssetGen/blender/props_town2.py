"""
Props for the bigger Hollowmere: traffic signals, the bank (teller counter, ATM, vault door),
the supermarket (shelves, cooler, checkout, cart), the playground, crime-scene pieces and more
yard decoration. Same registry and pivot conventions as props_world.py:

  floor props    bottom centre (z = 0 is the floor), front faces -Y
  wall props     back face on y = 0, the prop extends towards -Y
  ceiling props  mount point at the origin, hanging down -Z
  spans          (tape) run from x = 0 to x = length, so they tile along +X

QA: preview_world_props.py picks these up through props_world.WORLD (group 'town2').
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from mathutils import Vector  # noqa: E402

from hhblend import Builder  # noqa: E402
import props_world as PW  # noqa: E402
from props_world import wprop, _attach, _recentre, _blob, _sheet, _rrect, _chain  # noqa: E402

PW.GROUPS = tuple(PW.GROUPS) + ('town2',) if 'town2' not in PW.GROUPS else PW.GROUPS


# =======================================================================================
# Traffic signals (AHHTrafficLight: housing + three lens meshes in the same frame)

SIGNAL_LENS_Z = (-0.20, -0.525, -0.85)       # red, amber, green centres below the mount point
SIGNAL_FRONT = -0.17


@wprop('SM_TrafficLight_Pole', 'Props/Town', 'complex', 'town2', size=(4.6, 0.5, 6.4))
def traffic_light_pole():
    """Galvanised pole with a mast arm along +X; the signal head hangs at x = 4.2, z = 5.75."""
    b = Builder('SM_TrafficLight_Pole')
    m = 'MI_Aluminium'
    b.box((0.5, 0.5, 0.04), (0, 0, 0.02), 'MI_Steel', bevel=0.006)
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.cyl(0.018, 0.05, (sx * 0.19, sy * 0.19, 0.06), 'MI_Steel', segs=6)
    b.lathe([(0.0, 0.04), (0.11, 0.04), (0.11, 0.3), (0.095, 0.34), (0.085, 6.1), (0.07, 6.2), (0.0, 6.22)], mat=m,
            segs=16)
    b.cyl(0.13, 0.04, (0, 0, 0.32), m, segs=16)
    # Mast arm with a slight rise, a tie rod above it, and a clamp for the head.
    b.tube([(0.05, 0, 5.9), (2.2, 0, 5.98), (4.4, 0, 6.02)], 0.06, m, segs=12, bend=0.4)
    b.tube([(0.05, 0, 6.18), (2.6, 0, 6.06)], 0.018, m, segs=8)
    b.cyl(0.03, 0.3, (4.2, 0, 5.88), 'MI_Metal_Black', segs=8)
    b.box((0.16, 0.12, 0.08), (4.2, 0, 6.0), 'MI_Metal_Black', bevel=0.01)
    b.sphere(0.065, (4.42, 0, 6.02), m, segs=10, rings=6)
    # Push-button box for pedestrians.
    b.box((0.12, 0.08, 0.18), (0, -0.12, 1.1), 'MI_Metal_Yellow', bevel=0.01)
    b.cyl(0.025, 0.02, (0, -0.165, 1.1), 'MI_Steel', rot=(90, 0, 0), segs=12)
    return b


@wprop('SM_TrafficLight_Head', 'Props/Town', 'box', 'town2', kind='ceiling', size=(0.56, 0.4, 1.12))
def traffic_light_head():
    """Three-lamp head hanging from its mount point (origin); lenses face -Y. Dark lenses only -
    the lit discs are SM_TrafficLight_Lens_* (shown by AHHTrafficLight)."""
    b = Builder('SM_TrafficLight_Head')
    body = 'MI_Metal_Black'
    b.box((0.34, 0.26, 1.02), (0, 0, -0.56), body, bevel=0.02)
    b.cyl(0.03, 0.08, (0, 0, -0.03), body, segs=8)
    # Backplate with a reflective border.
    b.box((0.56, 0.02, 1.12), (0, 0.14, -0.56), body, bevel=0.01)
    for x0, z0, x1, z1 in ((-0.28, -1.12, 0.28, -1.08), (-0.28, -0.04, 0.28, 0.0), (-0.28, -1.12, -0.24, 0.0),
                           (0.24, -1.12, 0.28, 0.0)):
        b.box_minmax((x0, 0.126, z0), (x1, 0.13, z1), 'MI_Metal_Yellow')
    for z in SIGNAL_LENS_Z:
        b.cyl(0.115, 0.03, (0, -0.135, z), 'MI_Metal_Black', rot=(90, 0, 0), segs=24)
        b.cyl(0.105, 0.01, (0, SIGNAL_FRONT + 0.012, z), 'MI_Glass_Bottle', rot=(90, 0, 0), segs=24)
        # Visor: a half tube over the top of each lens, reaching 24 cm forward.
        def visor(u, v, z=z):
            a = math.pi * u
            return (0.13 * math.cos(a), -0.15 - 0.24 * v, z + 0.13 * math.sin(a))
        _sheet(b, visor, 10, 2, body, thickness=0.006, size=(0.4, 0.24))
    return b


def _signal_lens(name, mat, index):
    b = Builder(name)
    b.cyl(0.103, 0.012, (0, SIGNAL_FRONT, SIGNAL_LENS_Z[index]), mat, rot=(90, 0, 0), segs=24)
    return b


@wprop('SM_TrafficLight_Lens_Red', 'Props/Town', 'none', 'town2', kind='ceiling', size=(0.21, 0.02, 0.21))
def traffic_lens_red():
    return _signal_lens('SM_TrafficLight_Lens_Red', 'MI_Emit_Signal_Red', 0)


@wprop('SM_TrafficLight_Lens_Amber', 'Props/Town', 'none', 'town2', kind='ceiling', size=(0.21, 0.02, 0.21))
def traffic_lens_amber():
    return _signal_lens('SM_TrafficLight_Lens_Amber', 'MI_Emit_Signal_Amber', 1)


@wprop('SM_TrafficLight_Lens_Green', 'Props/Town', 'none', 'town2', kind='ceiling', size=(0.21, 0.02, 0.21))
def traffic_lens_green():
    return _signal_lens('SM_TrafficLight_Lens_Green', 'MI_Emit_Signal_Green', 2)


# Street-name corner posts for downtown (two blades: along X, along Y).
STREET_ROW = {'MAIN': 0, 'ASHGROVE': 1, 'MARROW': 2, 'CHURCH': 3, 'ELM': 4, 'PINE': 5, 'MILL': 6, 'ORCHARD': 7}


def _street_sign2(name, along_x, along_y):
    b = Builder(name)
    b.lathe([(0, 0), (0.04, 0), (0.035, 0.03), (0.032, 2.62), (0.0, 2.62)], mat='MI_Metal_Green', segs=12)
    b.lathe([(0, 2.6), (0.04, 2.6), (0.04, 2.66), (0.0, 2.66)], mat='MI_Steel', segs=12)
    bl, bh, bt = 0.8, 0.1, 0.006
    rows = len(STREET_ROW)
    for row, z, rot in ((STREET_ROW[along_x], 2.715, 0), (STREET_ROW[along_y], 2.835, 90)):
        v0, v1 = 1 - (row + 1) / rows, 1 - row / rows
        s = Builder('blade')
        s.box((bl, bt, bh), (0, 0, 0), 'MI_Aluminium', bevel=0.0015, segs=1)
        s.plane(bl, bh, (0, -bt / 2 - 0.0005, 0), 'MI_Img_Sign_Street', uv_rect=(0, v0, 1, v1))
        s.plane(bl, bh, (0, bt / 2 + 0.0005, 0), 'MI_Img_Sign_Street', rot=(0, 0, 180), uv_rect=(0, v0, 1, v1))
        for sz in (-1, 1):
            s.box((0.07, 0.03, 0.018), (0, 0, sz * (bh / 2 + 0.008)), 'MI_Steel', bevel=0.003)
        _attach(b, s, (0, 0, z), (0, 0, rot))
    b.cyl(0.018, 0.02, (0, 0, 2.775), 'MI_Steel', segs=10)
    b.sphere(0.02, (0, 0, 2.9 - 0.02), 'MI_Steel', segs=10, rings=6)
    return b


for _a, _b in (('MAIN', 'ELM'), ('MAIN', 'PINE'), ('ORCHARD', 'ELM'), ('ORCHARD', 'PINE'), ('MILL', 'ELM'),
               ('MILL', 'PINE')):
    _n = f'SM_Sign_Street_{_a.title()}_{_b.title()}'

    def _mk(n=_n, a=_a, c=_b):
        return _street_sign2(n, a, c)
    wprop(_n, 'Props/Town', 'box', 'town2', size=(0.8, 0.8, 2.9))(_mk)


# =======================================================================================
# Shop signs for downtown (same board as Main Street's, different images)

for _n, _img, _frame in (('SM_Sign_Bank', 'MI_Img_Sign_Bank', 'MI_Brass'), ('SM_Sign_Market', 'MI_Img_Sign_Market', 'MI_Trim_White'),
                         ('SM_Sign_Gazette', 'MI_Img_Sign_Gazette', 'MI_Trim_Dark'),
                         ('SM_Sign_Pharmacy', 'MI_Img_Sign_Pharmacy', 'MI_Trim_White'),
                         ('SM_Sign_Barber', 'MI_Img_Sign_Barber', 'MI_Trim_Dark'),
                         ('SM_Sign_Playground', 'MI_Img_Sign_Playground', 'MI_Wood_Dark')):
    def _mk(n=_n, img=_img, frame=_frame):
        return PW._shop_sign(n, img, frame=frame)
    wprop(_n, 'Props/Town', 'box', 'town2', kind='wall', size=(3.0, 0.08, 0.75))(_mk)


@wprop('SM_Sign_Playground_Post', 'Props/Town', 'box', 'town2', size=(3.2, 0.2, 2.2))
def sign_playground_post():
    """The Willow Park board on two wooden posts."""
    b = Builder('SM_Sign_Playground_Post')
    sign = PW._shop_sign('_pg', 'MI_Img_Sign_Playground', frame='MI_Wood_Dark')
    _attach(b, sign, (0, 0.04, 1.3))
    for x in (-1.45, 1.45):
        b.box((0.12, 0.12, 2.2), (x, 0.0, 1.1), 'MI_Wood_Dark', bevel=0.01)
        b.box((0.16, 0.16, 0.03), (x, 0.0, 2.21), 'MI_Wood_Dark', bevel=0.005)
    return b


# =======================================================================================
# The bank

@wprop('SM_Bank_Counter', 'Props/Bank', 'complex', 'town2', size=(3.0, 0.8, 2.3))
def bank_counter():
    """Two teller windows: panelled wooden counter (customers at -Y), marble top, brass rails,
    glass up to 2.3 m with a speaking grille and a transaction slot in each window."""
    b = Builder('SM_Bank_Counter')
    W, D, H = 3.0, 0.8, 1.08
    b.box((W, D - 0.1, H - 0.05), (0, 0.05, (H - 0.05) / 2), 'MI_Wood_Dark', bevel=0.004)
    b.box((W + 0.02, D + 0.04, 0.05), (0, 0.0, H - 0.025), 'MI_Stone_Wall', bevel=0.006)
    b.box((W, 0.04, 0.12), (0, -D / 2 + 0.07, 0.06), 'MI_Wood_Dark', bevel=0.004)
    # Raised panels on the customer side.
    for i in range(4):
        x = -W / 2 + W / 8 + i * W / 4
        PW._frame_ring(b, W / 4 - 0.12, H - 0.36, W / 4 - 0.22, H - 0.46, -D / 2 + 0.04, -D / 2 + 0.07, 'MI_Wood_Dark',
                       cx=x, cz=(H - 0.05) / 2 + 0.04, bevel=0.003)
    # Brass foot rail and the glass screen with posts.
    b.tube([(-W / 2 + 0.05, -D / 2 - 0.1, 0.18), (W / 2 - 0.05, -D / 2 - 0.1, 0.18)], 0.02, 'MI_Brass', segs=10)
    for x in (-W / 2 + 0.2, 0.0, W / 2 - 0.2):
        b.box((0.03, 0.03, 0.08), (x, -D / 2 - 0.05, 0.18), 'MI_Brass')
    top = 2.3
    for x in (-W / 2 + 0.03, 0.0, W / 2 - 0.03):
        b.box((0.06, 0.06, top - H), (x, 0.0, H + (top - H) / 2), 'MI_Brass', bevel=0.008)
    b.box((W, 0.08, 0.06), (0, 0.0, top), 'MI_Wood_Dark', bevel=0.006)
    for side in (-1, 1):
        cx = side * W / 4
        # Glass in two panes around the slot and the grille.
        b.box((W / 2 - 0.08, 0.012, top - H - 0.32), (cx, 0.0, H + 0.24 + (top - H - 0.32) / 2 + 0.04), 'MI_Glass_Teller')
        b.box((W / 2 - 0.5, 0.012, 0.2), (cx - 0.28, 0.0, H + 0.14), 'MI_Glass_Teller')
        b.box((0.3, 0.06, 0.06), (cx + 0.25, 0.0, H + 0.03), 'MI_Steel', bevel=0.006)       # slot tray
        b.cyl(0.09, 0.012, (cx, -0.006, H + 0.62), 'MI_Brass', rot=(90, 0, 0), segs=20)
        for k in range(-3, 4):
            b.box((0.12, 0.016, 0.006), (cx, -0.012, H + 0.62 + k * 0.022), 'MI_Metal_Black')
        # The window number plate.
        b.box((0.22, 0.012, 0.1), (cx, -0.01, top - 0.12), 'MI_Brass', bevel=0.003)
    # Teller side: a shelf with a cash drawer and a lamp.
    b.box((W - 0.1, 0.32, 0.03), (0, D / 2 - 0.14, 0.78), 'MI_Wood_Dark')
    for side in (-1, 1):
        b.box((0.42, 0.32, 0.12), (side * W / 4, D / 2 - 0.14, 0.7), 'MI_Metal_Grey', bevel=0.006)
        b.box((0.08, 0.02, 0.02), (side * W / 4, D / 2 + 0.03, 0.7), 'MI_Chrome')
    return b


@wprop('SM_ATM', 'Props/Bank', 'box', 'town2', kind='wall', size=(0.8, 0.5, 1.75))
def atm():
    """Wall ATM (back on y = 0): screen with side keys, keypad, card and cash slots, a lit topper."""
    b = Builder('SM_ATM')
    W, D, H = 0.8, 0.5, 1.75
    b.box((W, D, 1.0), (0, -D / 2, 0.5), 'MI_Metal_Grey', bevel=0.02)
    b.box((W, D - 0.1, 0.75), (0, -(D - 0.1) / 2, 1.0 + 0.375), 'MI_Metal_Grey', bevel=0.02)
    # Fascia: sloped screen panel.
    s = Builder('screen')
    s.box((0.66, 0.06, 0.5), (0, 0, 0), 'MI_Plastic_Black', bevel=0.01)
    s.plane(0.48, 0.36, (0, -0.032, 0.02), 'MI_Screen_ATM')
    for side in (-1, 1):
        for k in range(2):
            s.box((0.05, 0.02, 0.035), (side * 0.29, -0.035, 0.12 - k * 0.15), 'MI_Steel', bevel=0.004)
    _attach(b, s, (0, -D + 0.12, 1.32), (-12, 0, 0))
    # Keypad shelf.
    b.box((0.6, 0.18, 0.04), (0, -D + 0.04, 0.98), 'MI_Metal_Black', rot=(10, 0, 0), bevel=0.006)
    for r in range(4):
        for c in range(3):
            b.box((0.035, 0.03, 0.012), (-0.06 + c * 0.05, -D + 0.0 + r * 0.035, 1.005 - r * 0.006), 'MI_Steel',
                  rot=(10, 0, 0), bevel=0.003)
    for r, mat in enumerate(('MI_Plastic_Red', 'MI_Plastic_Yellow', 'MI_Plastic_Green')):
        b.box((0.05, 0.03, 0.012), (0.11, -D + 0.0 + r * 0.04, 1.005 - r * 0.006), mat, rot=(10, 0, 0), bevel=0.003)
    b.box((0.12, 0.02, 0.025), (0.24, -D - 0.005, 1.12), 'MI_Plastic_Green', bevel=0.004)   # card slot
    b.box((0.3, 0.03, 0.04), (0, -D - 0.005, 0.86), 'MI_Metal_Black', bevel=0.005)          # cash slot
    b.box((0.1, 0.02, 0.04), (-0.24, -D - 0.005, 1.12), 'MI_Metal_Black', bevel=0.004)      # receipt
    # Lit topper.
    b.box((W, 0.2, 0.16), (0, -0.1, H - 0.08), 'MI_Plastic_Blue', bevel=0.01)
    b.box((W - 0.06, 0.01, 0.1), (0, -0.205, H - 0.08), 'MI_Emit_Fluoro')
    return b


@wprop('SM_Vault_Door', 'Props/Bank', 'complex', 'town2', kind='wall', size=(3.2, 0.7, 3.2))
def vault_door():
    """Round vault door in its frame (back at y = 0, the door bulges out to -Y)."""
    b = Builder('SM_Vault_Door')
    m = 'MI_Metal_Vault'
    cz = 1.6
    pts = [(x, z + cz) for x, z in _rrect(3.2, 3.2, 0.12, 4)]
    b.prism(pts, 0.25, (0, -0.125, 0), m, axis='Y', bevel=0.01)
    b.lathe([(0.0, 0.0), (1.2, 0.0), (1.25, 0.06), (1.25, 0.32), (1.12, 0.4), (0.4, 0.44), (0.0, 0.45)],
            (0, -0.25, cz), m, rot=(90, 0, 0), segs=48)
    b.torus(1.27, 0.035, (0, -0.32, cz), 'MI_Chrome', rot=(90, 0, 0), segs=48, rsegs=8)
    for k in range(16):
        a = 2 * math.pi * k / 16
        b.cyl(0.045, 0.06, (1.05 * math.cos(a), -0.6, cz + 1.05 * math.sin(a)), 'MI_Chrome', rot=(90, 0, 0), segs=12)
    # Spoked handle wheel and the dial.
    b.torus(0.42, 0.03, (-0.3, -0.78, cz), 'MI_Chrome', rot=(90, 0, 0), segs=32, rsegs=8)
    for k in range(3):
        a = math.radians(30 + 120 * k)
        b.tube([(-0.3, -0.76, cz), (-0.3 + 0.42 * math.cos(a), -0.76, cz + 0.42 * math.sin(a))], 0.025, 'MI_Chrome',
               segs=8)
        b.sphere(0.05, (-0.3 + 0.48 * math.cos(a), -0.78, cz + 0.48 * math.sin(a)), 'MI_Chrome', segs=10, rings=6)
    b.cyl(0.08, 0.2, (-0.3, -0.72, cz), 'MI_Chrome', rot=(90, 0, 0), segs=16)
    b.cyl(0.16, 0.08, (0.5, -0.74, cz + 0.4), 'MI_Metal_Black', rot=(90, 0, 0), segs=24)
    b.cyl(0.12, 0.03, (0.5, -0.79, cz + 0.4), 'MI_Chrome', rot=(90, 0, 0), segs=24)
    # Hinges on the right.
    for dz in (-0.7, 0.7):
        b.cyl(0.11, 0.35, (1.35, -0.42, cz + dz), m, segs=16)
    return b


@wprop('SM_Stanchion_Pair', 'Props/Bank', 'convex', 'town2', size=(1.7, 0.36, 1.0))
def stanchion_pair():
    """Two brass queue posts with a velvet rope sagging between them (x = 0 and x = 1.6)."""
    b = Builder('SM_Stanchion_Pair')
    for x in (0.0, 1.6):
        b.lathe([(0, 0), (0.17, 0), (0.17, 0.02), (0.05, 0.05), (0.025, 0.07), (0.025, 0.92), (0.04, 0.95), (0.035, 0.98),
                 (0.0, 0.99)], (x, 0, 0), 'MI_Brass', segs=20)
    pts = [(1.6 * i / 10, 0, 0.93 - 0.18 * math.sin(math.pi * i / 10)) for i in range(11)]
    b.tube(pts, 0.018, 'MI_Velvet_Blue', segs=8)
    return _recentre(b)


@wprop('SM_Check_Desk', 'Props/Bank', 'box', 'town2', size=(1.4, 0.7, 1.15))
def check_desk():
    """Stand-up desk for filling in slips: marble top on a pedestal, pens on chains, slip trays."""
    b = Builder('SM_Check_Desk')
    b.box((1.2, 0.5, 0.95), (0, 0, 0.475), 'MI_Wood_Dark', bevel=0.01)
    b.box((1.4, 0.7, 0.05), (0, 0, 0.975), 'MI_Stone_Wall', bevel=0.008)
    for x in (-0.45, 0.45):
        b.box((0.36, 0.22, 0.04), (x, 0.18, 1.02), 'MI_Brass', bevel=0.004)
        b.box((0.3, 0.18, 0.02), (x, 0.18, 1.045), 'MI_Paper')
        b.cyl(0.006, 0.14, (x - 0.1, -0.15, 1.007), 'MI_Plastic_Black', rot=(0, 90, 20), segs=6)
        _chain(b, [(x - 0.17, -0.18, 1.005), (x - 0.2, -0.05, 1.005), (x - 0.25, 0.05, 1.04)], link=0.012, wire=0.0015,
               mat='MI_Brass', segs=5, rsegs=3)
    for k in range(5):
        b.box((0.21, 0.14, 0.001), (0.05 + k * 0.03, -0.05 - k * 0.01, 1.0006 + k * 0.0004), 'MI_Paper',
              rot=(0, 0, k * 7))
    return b


# =======================================================================================
# The supermarket

GROCERY_BANDS = 8


def _band_uv(band):
    """V range of a band of T_Groceries (band 0 at the top of the image)."""
    return 1 - (band + 1) / GROCERY_BANDS, 1 - band / GROCERY_BANDS


def _shelf_side(b, L, y_face, side, bands, levels):
    """One face of a gondola: product facings on 'levels' shelves, a price strip on each lip."""
    rot = 0 if side < 0 else 180
    for (z0, z1), band in zip(levels, bands):
        v0, v1 = _band_uv(band)
        u_len = L / 2.0                  # 2 m of shelf per texture width
        b.plane(L - 0.04, z1 - z0, (0, y_face, (z0 + z1) / 2), 'MI_Img_Groceries', rot=(0, 0, rot),
                uv_rect=(0, v0 + 0.004, u_len, v1 - 0.004))
        b.box((L, 0.02, 0.05), (0, y_face + side * 0.012, z0 - 0.02), 'MI_Metal_White', bevel=0.003)
        b.box((L, 0.004, 0.03), (0, y_face + side * 0.024, z0 - 0.02), 'MI_Plastic_Yellow')


@wprop('SM_Shelf_Grocery', 'Props/Market', 'box', 'town2', size=(4.0, 1.1, 1.9))
def shelf_grocery():
    """Double-sided gondola, 4 m: four stocked shelves each side (cereal on top, jugs at the bottom)."""
    return _gondola('SM_Shelf_Grocery', ((6, 1, 2, 0), (5, 4, 3, 0)))


@wprop('SM_Shelf_Grocery_B', 'Props/Market', 'box', 'town2', size=(4.0, 1.1, 1.9))
def shelf_grocery_b():
    return _gondola('SM_Shelf_Grocery_B', ((4, 1, 5, 2), (6, 3, 1, 5)))


def _gondola(name, bands_per_side):
    b = Builder(name)
    L, D, H = 4.0, 1.0, 1.85
    b.box((L, D, 0.12), (0, 0, 0.06), 'MI_Metal_White', bevel=0.01)
    b.box((L, 0.06, H - 0.12), (0, 0, 0.12 + (H - 0.12) / 2), 'MI_Pegboard')
    for x in (-L / 2 + 0.03, 0.0, L / 2 - 0.03):
        b.box((0.05, 0.1, H), (x, 0, H / 2), 'MI_Metal_White', bevel=0.005)
    levels = [(0.14, 0.48), (0.52, 0.86), (0.9, 1.24), (1.28, 1.7)]
    for side, bands in zip((-1, 1), bands_per_side):
        y_face = side * (D / 2 - 0.05)
        for z0, _ in levels:
            b.box((L - 0.02, D / 2 - 0.05, 0.02), (0, side * (D / 4 + 0.01), z0 - 0.01), 'MI_Metal_White')
        _shelf_side(b, L, y_face, side, bands, levels)
        b.box((L, 0.04, 0.12), (0, side * (D / 2 - 0.02), 0.06), 'MI_Plastic_Red', bevel=0.005)
    b.box((L + 0.02, D * 0.25, 0.03), (0, 0, H + 0.015), 'MI_Metal_White')
    return b


@wprop('SM_Cooler', 'Props/Market', 'box', 'town2', kind='wall', size=(3.0, 0.9, 2.15))
def cooler():
    """Refrigerated wall case: four glass doors, lit shelves of milk and soda (back on y = 0)."""
    b = Builder('SM_Cooler')
    W, D, H = 3.0, 0.9, 2.15
    b.box((W, D, 0.2), (0, -D / 2, 0.1), 'MI_Metal_Black', bevel=0.01)
    b.box((W, D, 0.25), (0, -D / 2, H - 0.125), 'MI_Metal_White', bevel=0.01)
    b.box((W, 0.05, H), (0, -0.025, H / 2), 'MI_Metal_White')
    for x in (-W / 2 + 0.025, W / 2 - 0.025):
        b.box((0.05, D, H), (x, -D / 2, H / 2), 'MI_Metal_White')
    b.box((W - 0.1, 0.01, 0.06), (0, -D + 0.12, H - 0.3), 'MI_Emit_Fluoro')
    levels = [(0.24, 0.64), (0.7, 1.1), (1.16, 1.5), (1.56, 1.85)]
    for (z0, z1), band in zip(levels, (7, 3, 7, 3)):
        v0, v1 = _band_uv(band)
        b.plane(W - 0.1, z1 - z0, (0, -0.3, (z0 + z1) / 2), 'MI_Img_Groceries', uv_rect=(0, v0 + 0.004, W / 2.2, v1 - 0.004))
        b.box((W - 0.1, 0.5, 0.02), (0, -0.4, z0 - 0.01), 'MI_Metal_White')
    for i in range(4):
        x = -W / 2 + W / 8 + i * W / 4
        PW._frame_ring(b, W / 4 - 0.02, H - 0.48, W / 4 - 0.1, H - 0.56, -D - 0.02, -D + 0.02, 'MI_Metal_Black',
                       cx=x, cz=0.2 + (H - 0.45) / 2, bevel=0.003)
        b.box((W / 4 - 0.1, 0.008, H - 0.56), (x, -D, 0.2 + (H - 0.45) / 2), 'MI_Glass_Teller')
        b.box((0.025, 0.04, 0.6), (x + W / 8 - 0.12, -D - 0.04, 1.05), 'MI_Chrome', bevel=0.006)
    return b


@wprop('SM_Checkout', 'Props/Market', 'complex', 'town2', size=(2.8, 1.2, 1.5))
def checkout():
    """Checkout lane counter along X (customers at -Y): belt, scanner, register, candy rack."""
    b = Builder('SM_Checkout')
    L, D, H = 2.6, 0.75, 0.9
    b.box((L, D, H - 0.04), (0, 0, (H - 0.04) / 2), 'MI_Metal_White', bevel=0.01)
    b.box((L, D + 0.02, 0.04), (0, 0, H - 0.02), 'MI_Countertop', bevel=0.004)
    b.box((1.5, 0.45, 0.02), (-0.5, -0.06, H + 0.01), 'MI_Rubber_Black')
    for x in (-1.25, 0.25):
        b.cyl(0.03, 0.47, (x, -0.06, H), 'MI_Steel', rot=(90, 0, 0), segs=10)
    b.box((0.04, 0.5, 0.06), (0.3, -0.06, H + 0.03), 'MI_Steel', bevel=0.004)       # divider bar
    # Scanner window and scale.
    b.box((0.45, 0.4, 0.02), (0.65, -0.06, H + 0.012), 'MI_Glass_Bottle')
    # Register on a post (screen to the cashier at +Y and a customer display).
    b.cyl(0.03, 0.3, (1.05, 0.2, H + 0.15), 'MI_Metal_Black', segs=8)
    b.box((0.42, 0.36, 0.14), (1.05, 0.2, H + 0.3), 'MI_Plastic_Beige', bevel=0.01)
    b.box((0.36, 0.06, 0.22), (1.05, 0.33, H + 0.48), 'MI_Plastic_Beige', rot=(-15, 0, 0), bevel=0.01)
    b.box((0.3, 0.01, 0.16), (1.05, 0.365, H + 0.49), 'MI_Screen_Monitor', rot=(-15, 0, 0))
    b.box((0.38, 0.32, 0.11), (1.05, 0.2, H + 0.18), 'MI_Plastic_Beige', bevel=0.008)   # cash drawer
    for r in range(3):
        for c in range(5):
            b.box((0.045, 0.04, 0.012), (0.95 + c * 0.05, 0.1 + r * 0.05, H + 0.375), 'MI_Plastic_Grey', bevel=0.002)
    # Bagging well and the candy rack at the customer side.
    b.box((0.5, 0.5, 0.04), (1.05, -0.2, H - 0.25), 'MI_Steel')
    rack = Builder('rack')
    rack.box((0.6, 0.3, 1.2), (0, 0, 0.6), 'MI_Metal_Black', bevel=0.01)
    for k in range(3):
        v0, v1 = _band_uv(5)
        rack.plane(0.56, 0.26, (0, -0.152, 0.3 + k * 0.32), 'MI_Img_Groceries', uv_rect=(k * 0.2, v0, k * 0.2 + 0.15, v1))
    _attach(b, rack, (-1.65, -0.1, 0), (0, 0, 0))
    # Lane light pole with a number.
    b.cyl(0.02, 1.4, (1.25, 0.32, H + 0.7), 'MI_Steel', segs=8)
    b.box((0.18, 0.18, 0.18), (1.25, 0.32, H + 1.45), 'MI_Emit_Fluoro', bevel=0.01)
    return b


@wprop('SM_Shopping_Cart', 'Props/Market', 'convex', 'town2', size=(0.6, 0.95, 1.05))
def shopping_cart():
    """Wire cart, handle at +Y (pushed towards -Y)."""
    b = Builder('SM_Shopping_Cart')
    m = 'MI_Chrome'
    z0, z1 = 0.45, 0.95
    f0 = [(-0.2, -0.45), (0.2, -0.45), (0.27, 0.38), (-0.27, 0.38)]
    for i in range(4):
        a, c = f0[i], f0[(i + 1) % 4]
        b.tube([(a[0], a[1], z1), (c[0], c[1], z1)], 0.008, m, segs=6)
        b.tube([(a[0] * 0.85, a[1] + 0.03, z0), (c[0] * 0.85, c[1] + 0.03, z0)], 0.007, m, segs=6)
    for x, y in f0:
        b.tube([(x * 0.85, y + 0.03, z0), (x, y, z1)], 0.008, m, segs=6)
    for k in range(1, 8):
        t = k / 8
        y = -0.45 + 0.83 * t
        xw = 0.2 + 0.07 * t
        b.tube([(-xw * 0.85, y + 0.03, z0), (-xw, y, z1)], 0.004, m, segs=4)
        b.tube([(xw * 0.85, y + 0.03, z0), (xw, y, z1)], 0.004, m, segs=4)
        b.tube([(-xw * 0.85, y + 0.03, z0 + 0.002), (xw * 0.85, y + 0.03, z0 + 0.002)], 0.004, m, segs=4)
    for k in range(1, 5):
        z = z0 + (z1 - z0) * k / 5
        b.tube([(-0.2 - 0.0 * k, -0.45 + 0.0, z), (0.2, -0.45, z)], 0.004, m, segs=4)
        b.tube([(-0.2, -0.45, z), (-0.27 + 0.07 * (1 - k / 5) * 0, 0.38, z)], 0.004, m, segs=4)
        b.tube([(0.2, -0.45, z), (0.27, 0.38, z)], 0.004, m, segs=4)
    # Handle, chassis, wheels.
    b.tube([(-0.27, 0.38, z1), (-0.28, 0.48, z1 + 0.05), (0.28, 0.48, z1 + 0.05), (0.27, 0.38, z1)], 0.012, m, segs=8,
           bend=0.03)
    b.cyl(0.018, 0.52, (0, 0.48, z1 + 0.05), 'MI_Plastic_Red', rot=(0, 90, 0), segs=10)
    for sx in (-1, 1):
        b.tube([(sx * 0.27, 0.38, z1), (sx * 0.25, 0.36, 0.1), (sx * 0.17, -0.4, 0.1)], 0.012, m, segs=8)
    b.tube([(-0.17, -0.4, 0.1), (0.17, -0.4, 0.1)], 0.01, m, segs=6)
    b.tube([(-0.25, 0.36, 0.1), (0.25, 0.36, 0.1)], 0.01, m, segs=6)
    for x, y in ((-0.17, -0.4), (0.17, -0.4), (-0.25, 0.36), (0.25, 0.36)):
        b.cyl(0.05, 0.03, (x, y, 0.05), 'MI_Rubber_Black', rot=(0, 90, 0), segs=12)
        b.box((0.02, 0.04, 0.05), (x, y, 0.085), m)
    b.box((0.36, 0.5, 0.01), (0, -0.05, 0.16), 'MI_Steel')
    return b


@wprop('SM_Produce_Stand', 'Props/Market', 'box', 'town2', size=(2.0, 1.1, 1.0))
def produce_stand():
    """Tilted crates of apples, oranges, lemons and pumpkins on a wooden stand."""
    b = Builder('SM_Produce_Stand')
    b.box((2.0, 1.0, 0.6), (0, 0, 0.3), 'MI_Wood_Raw', bevel=0.01)
    rnd = random.Random(31)
    fruits = (('MI_Plastic_Red', 0.04), ('MI_Plastic_Orange', 0.042), ('MI_Plastic_Yellow', 0.035), ('MI_Plastic_Green', 0.04))
    for i, (mat, r) in enumerate(fruits):
        cx = -0.75 + i * 0.5
        crate = Builder('crate')
        crate.box((0.46, 0.8, 0.02), (0, 0, 0.01), 'MI_Wood_Planks')
        for sx in (-1, 1):
            crate.box((0.02, 0.8, 0.14), (sx * 0.22, 0, 0.07), 'MI_Wood_Planks')
        for sy in (-1, 1):
            crate.box((0.46, 0.02, 0.14), (0, sy * 0.39, 0.07), 'MI_Wood_Planks')
        for _ in range(26):
            crate.sphere(r, (rnd.uniform(-0.19, 0.19), rnd.uniform(-0.35, 0.35), 0.02 + r + rnd.uniform(0, 0.07)), mat,
                         scale=(1, 1, 0.92), segs=7, rings=4)
        _attach(b, crate, (cx, 0.0, 0.66), (-18, 0, 0))
    for x in (-0.95, 0.95):
        b.box((0.06, 0.06, 0.9), (x, 0.45, 0.45), 'MI_Wood_Raw')
    return b


def _aisle_sign(name, row):
    b = Builder(name)
    for x in (-0.6, 0.6):
        b.cyl(0.004, 0.6, (x, 0, -0.3), 'MI_Steel', segs=4)
    b.box((1.6, 0.03, 0.22), (0, 0, -0.71), 'MI_Plastic_White', bevel=0.004)
    v0, v1 = 1 - (row + 1) / 4, 1 - row / 4
    b.plane(1.56, 0.19, (0, -0.0155, -0.71), 'MI_Img_Aisle_Signs', uv_rect=(0, v0, 1, v1))
    b.plane(1.56, 0.19, (0, 0.0155, -0.71), 'MI_Img_Aisle_Signs', rot=(0, 0, 180), uv_rect=(0, v0, 1, v1))
    return b


for _i in range(4):
    def _mk(i=_i):
        return _aisle_sign(f'SM_Aisle_Sign_{i + 1}', i)
    wprop(f'SM_Aisle_Sign_{_i + 1}', 'Props/Market', 'none', 'town2', kind='ceiling', size=(1.6, 0.03, 0.82))(_mk)


@wprop('SM_Poster_Bank', 'Props/Bank', 'none', 'town2', kind='wall', size=(0.62, 0.02, 0.9))
def poster_bank():
    b = Builder('SM_Poster_Bank')
    PW._frame_ring(b, 0.62, 0.9, 0.56, 0.84, -0.02, 0.0, 'MI_Brass', cz=0.45, bevel=0.003)
    b.plane(0.56, 0.84, (0, -0.012, 0.45), 'MI_Img_Poster_Bank')
    return b


@wprop('SM_Poster_Market', 'Props/Market', 'none', 'town2', kind='wall', size=(0.6, 0.01, 0.9))
def poster_market():
    b = Builder('SM_Poster_Market')
    b.plane(0.6, 0.9, (0, -0.004, 0.45), 'MI_Img_Poster_Market')
    for x in (-0.27, 0.27):
        b.box((0.02, 0.006, 0.02), (x, -0.006, 0.87), 'MI_Plastic_Red')
    return b


# =======================================================================================
# Playground (Willow Park)

@wprop('SM_Play_SwingFrame', 'Props/Playground', 'complex', 'town2', size=(4.0, 2.2, 2.6))
def play_swing_frame():
    """A-frame for two swings: the top bar along X at z = 2.45 (the AHHSwing pivots hang from it
    at x = -0.8 and x = +0.8)."""
    b = Builder('SM_Play_SwingFrame')
    m = 'MI_Metal_Red'
    top = 2.45
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.tube([(sx * 1.85, 0.0, top), (sx * 1.95, sy * 1.0, 0.0)], 0.04, m, segs=10)
        b.tube([(sx * 1.9, -0.55, 1.1), (sx * 1.9, 0.55, 1.1)], 0.025, m, segs=8)
        b.sphere(0.06, (sx * 1.88, 0, top), m, segs=10, rings=6)
    b.cyl(0.05, 3.8, (0, 0, top), m, rot=(0, 90, 0), segs=12)
    for x in (-1.0, -0.6, 0.6, 1.0):
        b.box((0.06, 0.08, 0.06), (x, 0, top - 0.04), 'MI_Steel', bevel=0.005)
    return b


@wprop('SM_Swing_Seat', 'Props/Playground', 'none', 'town2', kind='ceiling', size=(0.5, 0.2, 2.0))
def swing_seat():
    """Rubber strap seat on two chains, hanging from the origin (the pivot on the top bar)."""
    b = Builder('SM_Swing_Seat')
    drop = 1.95
    for dx in (-0.2, 0.2):
        b.torus(0.022, 0.005, (dx, 0, -0.03), 'MI_Steel', rot=(0, 90, 0), segs=8, rsegs=3)
        _chain(b, [(dx, 0, -0.05), (dx, 0, -drop + 0.06)], link=0.06, wire=0.0035, mat='MI_Steel', segs=5, rsegs=3)
    pts = []
    for i in range(9):
        t = i / 8
        pts.append((-0.22 + 0.44 * t, 0, -drop + 0.06 * (1 - (2 * t - 1) ** 2) * -1 + 0.02))
    seat = Builder('seat')
    seat.box((0.46, 0.16, 0.015), (0, 0, 0), 'MI_Rubber_Black', bevel=0.005)
    _attach(b, seat, (0, 0, -drop))
    return b


@wprop('SM_Slide', 'Props/Playground', 'complex', 'town2', size=(1.0, 4.4, 2.4))
def slide():
    """Ladder at +Y up to a railed platform at 1.6 m, the chute curves down towards -Y."""
    b = Builder('SM_Slide')
    m = 'MI_Metal_Yellow'
    p = 1.6
    for sx in (-1, 1):
        for y in (0.9, 1.6):
            b.tube([(sx * 0.4, y, 0), (sx * 0.4, y, p + 0.95)], 0.035, m, segs=10)
        b.tube([(sx * 0.4, 0.9, p + 0.9), (sx * 0.4, 1.6, p + 0.9)], 0.025, m, segs=8)
        b.tube([(sx * 0.3, 1.6, 0.0), (sx * 0.3, 2.3, 0.0), (sx * 0.3, 1.6, p)], 0.03, m, segs=8)
    b.box((0.86, 0.72, 0.05), (0, 1.25, p), 'MI_Plastic_Blue', bevel=0.01)
    for k in range(5):
        t = (k + 1) / 6
        b.box((0.56, 0.06, 0.03), (0, 1.6 + 0.7 * (1 - t), p * t), 'MI_Plastic_Green', bevel=0.005)
    # The chute: a U-channel along a curve from the platform down to the ground.
    n = 14
    path = []
    for i in range(n + 1):
        t = i / n
        y = 0.9 - 3.4 * t
        z = p * (1 - t) ** 1.25 * (1 - 0.15 * math.sin(math.pi * t)) + 0.25 * t
        path.append((y, z))
    def chute(u, v):
        i = min(int(v * n), n - 1)
        f = v * n - i
        y = path[i][0] + (path[i + 1][0] - path[i][0]) * f
        z = path[i][1] + (path[i + 1][1] - path[i][1]) * f
        a = math.pi * (1 - u)
        return (0.32 * math.cos(a), y, z + 0.06 - 0.18 * math.sin(a) * 0.6 + 0.18)
    _sheet(b, chute, 10, n * 2, 'MI_Plastic_Red', thickness=0.02, size=(1.0, 3.4))
    for sx in (-1, 1):
        b.tube([(sx * 0.32, 0.9, p + 0.25), (sx * 0.32, -2.5, 0.43)], 0.025, 'MI_Plastic_Red', segs=8, bend=0.2)
    b.tube([(0, -2.5, 0.0), (0, -2.45, 0.38)], 0.04, m, segs=8)
    return _recentre(b, x=False)


@wprop('SM_Seesaw', 'Props/Playground', 'complex', 'town2', size=(3.4, 0.5, 1.0))
def seesaw():
    b = Builder('SM_Seesaw')
    b.prism([(-0.25, 0), (0.25, 0), (0.06, 0.5), (-0.06, 0.5)], 0.3, (0, 0, 0), 'MI_Metal_Green', axis='Y', bevel=0.01)
    plank = Builder('plank')
    plank.box((3.2, 0.26, 0.06), (0, 0, 0), 'MI_Wood_Painted', bevel=0.01)
    for sx in (-1, 1):
        plank.box((0.4, 0.28, 0.04), (sx * 1.4, 0, 0.05), 'MI_Rubber_Black', bevel=0.01)
        plank.tube([(sx * 1.15, -0.12, 0.03), (sx * 1.15, -0.12, 0.28), (sx * 1.15, 0.12, 0.28), (sx * 1.15, 0.12, 0.03)],
                   0.018, 'MI_Metal_Yellow', segs=8, bend=0.05)
        plank.lathe([(0, 0), (0.12, 0), (0.12, 0.08), (0.0, 0.08)], (sx * 1.55, 0, -0.08), 'MI_Rubber_Black', segs=12)
    _attach(b, plank, (0, 0, 0.52), (0, 9, 0))
    return _recentre(b)


@wprop('SM_Sandbox', 'Props/Playground', 'complex', 'town2', size=(3.2, 3.2, 0.5))
def sandbox():
    """Timber-framed sandbox with a little mound, a bucket and a spade left behind."""
    b = Builder('SM_Sandbox')
    s = 3.0
    for k in range(4):
        rot = 90 * k
        side = Builder('side')
        side.box((s + 0.2, 0.15, 0.32), (0, -s / 2, 0.16), 'MI_Wood_Raw', bevel=0.01)
        side.box((s + 0.3, 0.3, 0.04), (0, -s / 2 - 0.02, 0.34), 'MI_Wood_Raw', bevel=0.005)
        _attach(b, side, (0, 0, 0), (0, 0, rot))
    def sand(u, v):
        x = (u - 0.5) * (s - 0.16)
        y = (v - 0.5) * (s - 0.16)
        z = 0.2 + 0.12 * math.exp(-((x - 0.5) ** 2 + (y + 0.3) ** 2) / 0.35) + 0.02 * math.sin(x * 5) * math.cos(y * 4)
        return (x, y, z)
    _sheet(b, sand, 12, 12, 'MI_Sand', size=(s, s))
    b.lathe([(0, 0), (0.09, 0), (0.12, 0.17), (0.115, 0.18), (0.0, 0.18)], (-0.6, 0.5, 0.21), 'MI_Plastic_Red', rot=(0, 70, 30),
            segs=14)
    b.box((0.06, 0.3, 0.012), (0.3, -0.6, 0.24), 'MI_Plastic_Yellow', rot=(0, 10, 40), bevel=0.004)
    b.box((0.13, 0.15, 0.015), (0.3 + 0.18, -0.6 - 0.16, 0.25), 'MI_Plastic_Yellow', rot=(0, 10, 40), bevel=0.004)
    return _recentre(b)


@wprop('SM_Climber', 'Props/Playground', 'complex', 'town2', size=(2.6, 2.6, 2.0))
def climber():
    """Dome climbing frame (geodesic-ish tube lattice)."""
    b = Builder('SM_Climber')
    R = 1.25
    rings = [(0.0, 1.0), (0.45, 0.95), (0.9, 0.78), (1.3, 0.5), (1.6, 0.0)]
    n = 10
    pts = []
    for z, rf in rings:
        row = []
        cnt = n if rf > 0 else 1
        for i in range(cnt):
            a = 2 * math.pi * (i + (0.5 if len(pts) % 2 else 0)) / cnt
            row.append((R * rf * math.cos(a), R * rf * math.sin(a), z))
        pts.append(row)
    for r in range(len(pts)):
        row = pts[r]
        if len(row) > 1:
            for i in range(len(row)):
                b.tube([row[i], row[(i + 1) % len(row)]], 0.022, 'MI_Metal_Blue', segs=6)
        if r + 1 < len(pts):
            nxt = pts[r + 1]
            for i in range(len(row)):
                j = min(i, len(nxt) - 1)
                b.tube([row[i], nxt[j]], 0.022, 'MI_Metal_Blue', segs=6)
                if len(nxt) > 1:
                    b.tube([row[i], nxt[(i - 1) % len(nxt)]], 0.022, 'MI_Metal_Blue', segs=6)
    return _recentre(b)


@wprop('SM_Bench_Kids', 'Props/Playground', 'box', 'town2', size=(1.8, 0.6, 0.85))
def bench_kids():
    """Plain wooden bench with a painted frame (parents' bench by the playground)."""
    b = Builder('SM_Bench_Kids')
    for x in (-0.75, 0.75):
        b.box((0.06, 0.5, 0.42), (x, 0, 0.21), 'MI_Metal_Green', bevel=0.005)
        b.box((0.06, 0.06, 0.4), (x, 0.24, 0.62), 'MI_Metal_Green', bevel=0.005)
    for k in range(3):
        b.box((1.8, 0.13, 0.04), (0, -0.16 + k * 0.15, 0.44), 'MI_Wood_Planks', bevel=0.005, uv_offset=(k * 0.3, 0))
    for k in range(2):
        b.box((1.8, 0.03, 0.12), (0, 0.25, 0.6 + k * 0.16), 'MI_Wood_Planks', bevel=0.005)
    return b


# =======================================================================================
# Crime scene

@wprop('SM_Police_Tape', 'Props/CrimeScene', 'none', 'town2', kind='fence', size=(6.0, 0.1, 1.0))
def police_tape():
    """6 m of tape between two stakes (x = 0 and x = 6), sagging, with a little twist."""
    b = Builder('SM_Police_Tape')
    L = 6.0
    for x in (0.0, L):
        b.cyl(0.012, 1.05, (x, 0, 0.525), 'MI_Steel', segs=6)
        b.cyl(0.02, 0.04, (x, 0, 1.0), 'MI_Plastic_Red', segs=8)

    def tape(u, v):
        x = L * v
        sag = 0.18 * math.sin(math.pi * v)
        twist = 0.35 * math.sin(v * 9.0)
        h = 0.05
        return (x, (u - 0.5) * h * math.sin(twist), 0.97 - sag + (u - 0.5) * h * math.cos(twist))
    _sheet(b, tape, 1, 24, 'MI_Img_Police_Tape', thickness=0.002, size=(1.0, L / 2.0))
    # UVs: the sheet maps u across, v along; rotate so the text runs along the tape.
    me_uv = b.bm.loops.layers.uv.active
    for f in b.bm.faces:
        if b.mats[f.material_index] != 'MI_Img_Police_Tape':
            continue
        for loop in f.loops:
            u, v = loop[me_uv].uv
            loop[me_uv].uv = (v, u)
    return b


@wprop('SM_Body_Covered', 'Props/CrimeScene', 'convex', 'town2', size=(0.8, 2.0, 0.4))
def body_covered():
    """Someone under a white sheet on the ground (head at -Y), the shoes showing at +Y."""
    b = Builder('SM_Body_Covered')

    def body_height(x, y):
        # Lying on the back along Y: head, chest, hips, legs, feet up.
        h = 0.0
        for cy, cx, ry, rx, hz in ((-0.82, 0.0, 0.12, 0.1, 0.18), (-0.45, 0.0, 0.26, 0.22, 0.22), (-0.05, 0.0, 0.2, 0.2, 0.17),
                                   (0.35, -0.1, 0.32, 0.08, 0.12), (0.35, 0.1, 0.32, 0.08, 0.12), (0.78, -0.11, 0.06, 0.06, 0.2),
                                   (0.78, 0.11, 0.06, 0.06, 0.2), (-0.35, -0.3, 0.25, 0.05, 0.08), (-0.35, 0.3, 0.25, 0.05, 0.08)):
            d = ((y - cy) / ry) ** 2 + ((x - cx) / rx) ** 2
            if d < 1:
                h = max(h, hz * math.sqrt(1 - d))
        return h

    def sheet(u, v):
        x = (u - 0.5) * 0.95
        y = (v - 0.5) * 2.0
        z = body_height(x, y)
        # The sheet drapes: soften towards the edges.
        edge = min(u, 1 - u, v, 1 - v)
        z = max(z, 0.0) * (0.7 + 0.3 * min(1, edge * 6)) + 0.012 + 0.01 * math.sin(u * 17 + v * 11)
        return (x, y, z)
    _sheet(b, sheet, 20, 40, 'MI_Fabric_Sheet', thickness=0.004, size=(0.95, 2.0))
    for sx in (-1, 1):
        b.box((0.09, 0.08, 0.1), (sx * 0.11, 1.0, 0.07), 'MI_Leather_Black', bevel=0.02)
    return b


@wprop('SM_Evidence_Markers', 'Props/CrimeScene', 'none', 'town2', size=(1.5, 1.2, 0.12))
def evidence_markers():
    """Four folded yellow tent markers scattered on the ground."""
    b = Builder('SM_Evidence_Markers')
    for x, y, rot in ((0.0, 0.0, 10), (0.6, 0.35, -25), (-0.5, 0.5, 40), (0.3, -0.4, 75)):
        t = Builder('tent')
        t.prism([(-0.05, 0), (0.05, 0), (0.0, 0.1)], 0.09, (0, 0, 0), 'MI_Plastic_Yellow', axis='X')
        t.box((0.05, 0.004, 0.035), (0, -0.026, 0.05), 'MI_Plastic_Black', rot=(-27, 0, 0))
        _attach(b, t, (x, y, 0.0), (0, 0, rot))
    return _recentre(b)


# =======================================================================================
# Yard decoration (it is October in Hollowmere)

@wprop('SM_Pumpkin_Lantern', 'Props/Yard', 'convex', 'yard', size=(0.45, 0.45, 0.42))
def pumpkin_lantern():
    """Carved jack-o'-lantern (about 45 cm across): ribbed lobes, a stem, and a face whose eyes,
    nose and grin glow (ember emissive) - October decoration on porches and steps."""
    b = Builder('SM_Pumpkin_Lantern')
    prof = [(0.0, 0.0), (0.07, 0.0), (0.11, 0.04), (0.125, 0.13), (0.115, 0.22), (0.08, 0.29), (0.03, 0.32), (0.0, 0.32)]
    for k in range(10):
        a = 2 * math.pi * k / 10
        b.lathe(prof, (0.1 * math.cos(a), 0.1 * math.sin(a), 0), 'MI_Plastic_Orange', segs=10,
                scale=(1.0, 0.8, 1.0), rot=(0, 0, math.degrees(a)))
    b.cyl(0.02, 0.08, (0, 0, 0.35), 'MI_Wood_Dark', rot=(10, 5, 0), segs=6)
    face_y = -0.2
    for x in (-0.07, 0.07):
        b.prism([(x - 0.035, 0.19), (x + 0.035, 0.19), (x, 0.24)], 0.02, (0, face_y, 0), 'MI_Emit_Ember', axis='Y')
    b.prism([(-0.018, 0.15), (0.018, 0.15), (0.0, 0.175)], 0.02, (0, face_y - 0.004, 0), 'MI_Emit_Ember', axis='Y')
    grin = [(-0.1, 0.11), (-0.065, 0.075), (-0.04, 0.09), (-0.015, 0.068), (0.015, 0.068), (0.04, 0.09), (0.065, 0.075),
            (0.1, 0.11), (0.05, 0.055), (0.0, 0.048), (-0.05, 0.055)]
    b.prism(grin, 0.02, (0, face_y + 0.004, 0), 'MI_Emit_Ember', axis='Y')
    return b


@wprop('SM_Scarecrow', 'Props/Yard', 'convex', 'yard', size=(1.4, 0.4, 2.1))
def scarecrow():
    """A stake, a crossbar, an old coat and a sack head with stitched eyes - Halloween decor."""
    b = Builder('SM_Scarecrow')
    b.box((0.07, 0.07, 2.0), (0, 0.05, 1.0), 'MI_Wood_Raw', bevel=0.01)
    b.box((1.3, 0.06, 0.06), (0, 0.05, 1.5), 'MI_Wood_Raw', bevel=0.01)
    coat = Builder('coat')

    def coat_fn(u, v):
        a = 2 * math.pi * u
        z = 0.75 + 0.8 * v
        r = 0.2 + 0.05 * math.sin(v * math.pi) - 0.06 * v + 0.02 * math.sin(a * 5 + v * 3)
        return (r * math.cos(a) * 1.25, r * math.sin(a) * 0.7, z)
    _sheet(coat, coat_fn, 16, 6, 'MI_Fabric_Brown', thickness=0.01, size=(1.6, 0.8))
    for sx in (-1, 1):
        coat.tube([(sx * 0.2, 0, 1.5), (sx * 0.62, 0, 1.47)], 0.07, 'MI_Fabric_Brown', segs=8)
        for k in range(4):
            coat.box((0.01, 0.01, 0.12), (sx * (0.66 + 0.02 * k), -0.02 + 0.01 * k, 1.4), 'MI_Wood_Raw', rot=(0, sx * 20, 0))
    _attach(b, coat, (0, 0.05, 0))
    b.sphere(0.16, (0, 0.05, 1.78), 'MI_Cardboard', scale=(1, 0.9, 1.15), segs=12, rings=8)
    b.cyl(0.11, 0.06, (0, 0.05, 1.62), 'MI_Wood_Raw', segs=8)
    for x in (-0.06, 0.06):
        b.box((0.05, 0.01, 0.01), (x, -0.095, 1.82), 'MI_Plastic_Black', rot=(0, 45, 0))
        b.box((0.05, 0.01, 0.01), (x, -0.095, 1.82), 'MI_Plastic_Black', rot=(0, -45, 0))
    b.tube([(-0.08, -0.1, 1.72), (0.0, -0.11, 1.7), (0.08, -0.1, 1.72)], 0.006, 'MI_Plastic_Black', segs=4)
    b.lathe([(0, 1.92), (0.25, 1.9), (0.25, 1.92), (0.11, 1.95), (0.1, 2.08), (0.0, 2.09)], (0, 0.05, 0), 'MI_Fabric_Brown',
            segs=14)
    return b


@wprop('SM_Tombstone_Decor', 'Props/Yard', 'box', 'yard', size=(1.2, 0.3, 0.7))
def tombstone_decor():
    """Two plywood Halloween tombstones on the lawn (painted grey, a crooked cross)."""
    b = Builder('SM_Tombstone_Decor')
    for x, h, lean in ((-0.3, 0.62, -6), (0.35, 0.5, 9)):
        t = Builder('t')
        pts = [(-0.2, 0), (0.2, 0), (0.2, h - 0.2)] + [(0.2 * math.cos(math.radians(a)), h - 0.2 + 0.2 * math.sin(math.radians(a)))
                                                        for a in range(10, 180, 20)] + [(-0.2, h - 0.2)]
        t.prism(pts, 0.025, (0, 0, 0), 'MI_Concrete_Raw', axis='Y')
        t.box((0.03, 0.005, 0.16), (0, -0.015, h * 0.6), 'MI_Plastic_Black')
        t.box((0.1, 0.005, 0.03), (0, -0.015, h * 0.64), 'MI_Plastic_Black')
        _attach(b, t, (x, 0, 0), (lean, 0, 0))
    return _recentre(b)


@wprop('SM_Porch_Planter', 'Props/Yard', 'box', 'yard', size=(0.8, 0.3, 0.55))
def porch_planter():
    b = Builder('SM_Porch_Planter')
    b.box((0.8, 0.3, 0.25), (0, 0, 0.125), 'MI_Wood_Painted', bevel=0.01)
    b.box((0.76, 0.26, 0.02), (0, 0, 0.24), 'MI_Dirt')
    rnd = random.Random(8)
    for i in range(10):
        x = -0.32 + i * 0.07
        _blob(b, 0.07, (x, rnd.uniform(-0.05, 0.05), 0.3 + rnd.uniform(0, 0.06)), 'MI_Hedge', subdiv=1, amp=0.3, seed=i)
        b.sphere(0.03, (x + 0.02, rnd.uniform(-0.08, 0.0), 0.4 + rnd.uniform(0, 0.08)),
                 rnd.choice(('MI_Plastic_Red', 'MI_Plastic_Yellow', 'MI_Plastic_Orange')), segs=8, rings=5)
    return b


@wprop('SM_Bicycle', 'Props/Yard', 'convex', 'yard', size=(0.5, 1.75, 1.0))
def bicycle():
    """A kid's bike on its kickstand, front wheel to -Y."""
    b = Builder('SM_Bicycle')
    r = 0.3
    for y in (-0.55, 0.55):
        b.torus(r, 0.02, (0, y, r + 0.02), 'MI_Rubber_Black', rot=(0, 90, 0), segs=24, rsegs=6)
        b.torus(r - 0.03, 0.008, (0, y, r + 0.02), 'MI_Chrome', rot=(0, 90, 0), segs=20, rsegs=4)
        for k in range(8):
            a = math.pi * k / 8
            b.tube([(0, y + (r - 0.04) * math.cos(a), r + 0.02 + (r - 0.04) * math.sin(a)),
                    (0, y - (r - 0.04) * math.cos(a), r + 0.02 - (r - 0.04) * math.sin(a))], 0.002, 'MI_Chrome', segs=3)
    f = 'MI_Metal_Blue'
    bb = (0, 0.05, 0.3)
    seat = (0, 0.3, 0.72)
    head = (0, -0.4, 0.75)
    for a, c in ((bb, seat), (seat, head), (bb, head), (bb, (0, 0.55, r + 0.02)), (seat, (0, 0.55, r + 0.02)),
                 (head, (0, -0.55, r + 0.02))):
        b.tube([a, c], 0.018, f, segs=8)
    b.box((0.12, 0.25, 0.05), (0, 0.33, 0.77), 'MI_Leather_Black', bevel=0.02)
    b.tube([(-0.25, -0.42, 0.9), (0, -0.42, 0.86), (0.25, -0.42, 0.9)], 0.014, 'MI_Chrome', segs=8)
    b.tube([(0, -0.4, 0.75), (0, -0.42, 0.88)], 0.016, 'MI_Chrome', segs=8)
    b.tube([(0.03, 0.08, 0.3), (0.2, 0.15, 0.02)], 0.01, 'MI_Steel', segs=6)
    return _recentre(b)


@wprop('SM_Firewood_Stack', 'Props/Yard', 'box', 'yard', size=(1.6, 0.5, 0.9))
def firewood_stack():
    b = Builder('SM_Firewood_Stack')
    rnd = random.Random(12)
    for row in range(6):
        for i in range(9 - (row % 2)):
            x = -0.72 + i * 0.18 + (0.09 if row % 2 else 0)
            rr = rnd.uniform(0.06, 0.085)
            b.cyl(rr, 0.45, (x, 0, 0.08 + row * 0.15), 'MI_Bark', rot=(90, 0, rnd.uniform(-4, 4)), segs=7)
            b.cyl(rr * 0.95, 0.002, (x, -0.226, 0.08 + row * 0.15), 'MI_Wood_Raw', rot=(90, 0, 0), segs=7)
    for x in (-0.82, 0.82):
        b.box((0.06, 0.5, 0.95), (x, 0, 0.475), 'MI_Wood_Raw')
    return b


@wprop('SM_Clothesline', 'Props/Yard', 'complex', 'yard', size=(5.0, 0.6, 2.0))
def clothesline():
    """Two T-posts, three lines, two white sheets and a shirt hanging still (a dull, creepy shape at night)."""
    b = Builder('SM_Clothesline')
    L = 4.6
    for x in (-L / 2, L / 2):
        b.box((0.07, 0.07, 1.9), (x, 0, 0.95), 'MI_Metal_Grey', bevel=0.005)
        b.box((0.07, 0.6, 0.05), (x, 0, 1.85), 'MI_Metal_Grey', bevel=0.005)
    for y in (-0.25, 0.0, 0.25):
        pts = [(-L / 2 + L * i / 12, y, 1.84 - 0.08 * math.sin(math.pi * i / 12)) for i in range(13)]
        b.tube(pts, 0.004, 'MI_Plastic_White', segs=4)
    for x0, w, hgt in ((-1.6, 1.4, 1.3), (0.2, 1.2, 1.15)):
        def cloth(u, v, x0=x0, w=w, hgt=hgt):
            x = x0 + w * u
            sag = 0.08 * math.sin(math.pi * (x + L / 2) / L)
            z = 1.84 - sag - hgt * v + 0.03 * math.sin(u * 12) * v
            return (x, -0.25 + 0.04 * math.sin(u * 7 + v * 3) * v, z)
        _sheet(b, cloth, 10, 8, 'MI_Fabric_Sheet', thickness=0.003, size=(w, hgt))
    return b


@wprop('SM_Wreath', 'Props/Yard', 'none', 'yard', kind='wall', size=(0.5, 0.12, 0.5))
def wreath():
    """Dried autumn wreath for a front door (back on y = 0)."""
    b = Builder('SM_Wreath')
    for k in range(18):
        a = 2 * math.pi * k / 18
        _blob(b, 0.06, (0.19 * math.cos(a), -0.05, 0.25 + 0.19 * math.sin(a)), 'MI_Tree_Leaves', subdiv=1, amp=0.35, seed=k,
              scale=(1, 0.6, 1))
    b.box((0.1, 0.03, 0.12), (0, -0.09, 0.08), 'MI_Fabric_Curtain', bevel=0.01)
    return b
