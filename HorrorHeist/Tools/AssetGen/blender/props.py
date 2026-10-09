"""
Procedural prop library for the hideout. Every function builds one static mesh.

Pivot conventions (Blender, metres):
  floor props   bottom centre, front faces -Y
  wall props    back face on y = 0, the prop extends towards -Y
  ceiling props top (mount point) at the origin, hanging down -Z
  doors         hinge at the origin, the leaf extends along +X
  switch box    back on x = 0, front faces +X (Unreal actor forward)
"""
import math
import random

from hhblend import Builder

PROPS = {}


def prop(name, folder='Props', collision='box', nanite=False):
    def deco(fn):
        PROPS[name] = dict(fn=fn, folder=folder, collision=collision, nanite=nanite)
        return fn
    return deco


# =======================================================================================
# Workshop

@prop('SM_Workbench', 'Props/Workshop')
def workbench():
    b = Builder('SM_Workbench')
    w, d, h = 2.0, 0.75, 0.92
    for i in range(5):
        b.box((w, d / 5 - 0.004, 0.055), (0, -d / 2 + d / 10 + i * d / 5, h - 0.0275), 'MI_Wood_Raw', bevel=0.004,
              uv_offset=(i * 0.37, i * 0.21))
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.box((0.05, 0.05, h - 0.055), (sx * (w / 2 - 0.06), sy * (d / 2 - 0.06), (h - 0.055) / 2), 'MI_Metal_Green',
                  bevel=0.004)
    for sy in (-1, 1):
        b.box((w - 0.1, 0.03, 0.08), (0, sy * (d / 2 - 0.06), h - 0.1), 'MI_Metal_Green', bevel=0.003)
        b.box((w - 0.1, 0.03, 0.05), (0, sy * (d / 2 - 0.06), 0.16), 'MI_Metal_Green', bevel=0.003)
    b.box((w - 0.12, d - 0.1, 0.02), (0, 0, 0.2), 'MI_Wood_Dark')
    b.box((w, 0.025, 0.14), (0, d / 2 - 0.0125, h + 0.07), 'MI_Wood_Raw', bevel=0.003)
    return b


@prop('SM_Pegboard', 'Props/Workshop')
def pegboard():
    b = Builder('SM_Pegboard')
    w, h = 1.8, 1.0
    b.box((w, 0.006, h), (0, -0.026, h / 2), 'MI_Pegboard')
    for z in (0.0, h):
        b.box((w + 0.04, 0.028, 0.04), (0, -0.014, z), 'MI_Wood_Raw', bevel=0.003)
    for x in (-w / 2, w / 2):
        b.box((0.04, 0.028, h), (x, -0.014, h / 2), 'MI_Wood_Raw', bevel=0.003)
    return b


@prop('SM_PegHook', 'Props/Workshop', 'none')
def peg_hook():
    b = Builder('SM_PegHook')
    b.tube([(0, 0, 0), (0, -0.07, 0), (0, -0.075, 0.025)], 0.0025, 'MI_Steel', segs=6, bend=0.01)
    return b


@prop('SM_Tool_Hammer', 'Props/Workshop', 'none')
def hammer():
    b = Builder('SM_Tool_Hammer')
    b.cyl(0.014, 0.3, (0, 0, -0.15), 'MI_Wood_Raw', segs=10)
    b.box((0.11, 0.028, 0.03), (0.0, 0, 0.005), 'MI_Steel', bevel=0.003)
    b.cyl(0.016, 0.03, (-0.07, 0, 0.005), 'MI_Steel', rot=(0, 90, 0), segs=10)
    return b


@prop('SM_Tool_Wrench', 'Props/Workshop', 'none')
def wrench():
    b = Builder('SM_Tool_Wrench')
    b.box((0.018, 0.006, 0.22), (0, 0, -0.11), 'MI_Chrome', bevel=0.002)
    b.prism([(-0.025, 0), (0.025, 0), (0.03, 0.02), (0.01, 0.045), (0.006, 0.02), (-0.006, 0.02), (-0.01, 0.045),
             (-0.03, 0.02)], 0.007, (0, 0, -0.005), 'MI_Chrome')
    b.torus(0.017, 0.005, (0, 0, -0.235), 'MI_Chrome', rot=(90, 0, 0), segs=12, rsegs=6)
    return b


@prop('SM_Tool_Screwdriver', 'Props/Workshop', 'none')
def screwdriver():
    b = Builder('SM_Tool_Screwdriver')
    b.lathe([(0, -0.11), (0.014, -0.105), (0.016, -0.06), (0.012, -0.01), (0.006, 0)], mat='MI_Plastic_Red', segs=8)
    b.cyl(0.003, 0.12, (0, 0, 0.06), 'MI_Chrome', segs=6)
    return b


@prop('SM_Tool_Saw', 'Props/Workshop', 'none')
def saw():
    b = Builder('SM_Tool_Saw')
    b.prism([(0, 0), (0.42, 0.03), (0.42, 0.06), (0, 0.11)], 0.0015, (0.04, 0, -0.06), 'MI_Steel', axis='Y')
    b.prism([(-0.1, -0.04), (0.06, -0.04), (0.07, 0.12), (-0.08, 0.13), (-0.11, 0.05)], 0.022, (0, 0, -0.06),
            'MI_Wood_Dark', bevel=0.004)
    return b


@prop('SM_Tool_Pliers', 'Props/Workshop', 'none')
def pliers():
    b = Builder('SM_Tool_Pliers')
    for s in (-1, 1):
        b.box((0.012, 0.008, 0.13), (s * 0.018, 0, -0.08), 'MI_Plastic_Red', rot=(0, s * 6, 0), bevel=0.003)
        b.box((0.01, 0.008, 0.06), (s * 0.005, 0, 0.015), 'MI_Steel', rot=(0, -s * 4, 0))
    b.cyl(0.008, 0.014, (0, 0, -0.015), 'MI_Steel', rot=(90, 0, 0), segs=8)
    return b


@prop('SM_BenchVise', 'Props/Workshop', 'box')
def bench_vise():
    b = Builder('SM_BenchVise')
    b.box((0.16, 0.14, 0.03), (0, 0, 0.015), 'MI_Metal_Blue', bevel=0.004)
    b.box((0.14, 0.06, 0.1), (0, 0.03, 0.08), 'MI_Metal_Blue', bevel=0.006)
    b.box((0.14, 0.05, 0.09), (0, -0.05, 0.075), 'MI_Metal_Blue', bevel=0.006)
    b.box((0.13, 0.008, 0.035), (0, -0.003, 0.12), 'MI_Steel')
    b.cyl(0.01, 0.2, (0, -0.15, 0.07), 'MI_Steel', rot=(90, 0, 0), segs=8)
    b.cyl(0.006, 0.16, (0, -0.26, 0.07), 'MI_Chrome', rot=(0, 90, 0), segs=6)
    return b


@prop('SM_Toolbox', 'Props/Workshop', 'box')
def toolbox():
    b = Builder('SM_Toolbox')
    b.box((0.52, 0.22, 0.2), (0, 0, 0.1), 'MI_Metal_Red', bevel=0.008)
    b.box((0.53, 0.23, 0.05), (0, 0, 0.225), 'MI_Metal_Red', bevel=0.008)
    b.box((0.06, 0.01, 0.04), (0, -0.115, 0.19), 'MI_Chrome', bevel=0.003)
    b.tube([(-0.12, 0, 0.25), (-0.12, 0, 0.3), (0.12, 0, 0.3), (0.12, 0, 0.25)], 0.008, 'MI_Plastic_Black', bend=0.02)
    return b


@prop('SM_Locker_Triple', 'Props/Workshop', 'box')
def lockers():
    b = Builder('SM_Locker_Triple')
    w, d, h = 0.38, 0.48, 1.85
    for i in range(3):
        x = (i - 1) * w
        b.box((w - 0.006, 0.02, h - 0.12), (x, -d / 2 + 0.01, 0.1 + (h - 0.12) / 2), 'MI_Metal_Blue', bevel=0.004,
              uv_offset=(i * 0.3, 0))
        for k in range(5):
            b.box((0.2, 0.006, 0.012), (x, -d / 2 - 0.001, 1.55 + k * 0.03), 'MI_Plastic_Black')
            b.box((0.2, 0.006, 0.012), (x, -d / 2 - 0.001, 0.25 + k * 0.03), 'MI_Plastic_Black')
        b.box((0.025, 0.03, 0.12), (x + w / 2 - 0.06, -d / 2 - 0.012, 1.05), 'MI_Chrome', bevel=0.004)
        b.box((0.05, 0.004, 0.03), (x, -d / 2 - 0.002, 1.7), 'MI_Paper')
    b.box((3 * w, d - 0.02, h - 0.1), (0, 0.01, 0.1 + (h - 0.1) / 2), 'MI_Metal_Blue')
    b.box((3 * w - 0.04, d - 0.04, 0.1), (0, 0, 0.05), 'MI_Metal_Black')
    return b


@prop('SM_Shelf_Metal', 'Props/Workshop', 'box')
def shelf_metal():
    b = Builder('SM_Shelf_Metal')
    w, d, h = 1.2, 0.45, 1.9
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.box((0.035, 0.035, h), (sx * (w / 2 - 0.0175), sy * (d / 2 - 0.0175), h / 2), 'MI_Metal_Grey', bevel=0.002)
    for z in (0.12, 0.6, 1.1, 1.6):
        b.box((w, d, 0.025), (0, 0, z), 'MI_Metal_Grey', bevel=0.003)
    return b


@prop('SM_Box_A', 'Props/Clutter', 'box')
def box_a():
    b = Builder('SM_Box_A')
    b.box((0.5, 0.4, 0.34), (0, 0, 0.17), 'MI_Cardboard', bevel=0.004)
    b.box((0.06, 0.402, 0.002), (0, 0, 0.341), 'MI_Plastic_Beige')
    b.box((0.06, 0.002, 0.08), (0, -0.201, 0.3), 'MI_Plastic_Beige')
    return b


@prop('SM_Box_B', 'Props/Clutter', 'box')
def box_b():
    b = Builder('SM_Box_B')
    b.box((0.38, 0.3, 0.28), (0, 0, 0.14), 'MI_Cardboard', bevel=0.004, uv_offset=(0.3, 0.2))
    b.box((0.38, 0.06, 0.002), (0, 0, 0.281), 'MI_Plastic_Beige')
    b.box((0.18, 0.002, 0.1), (0.05, -0.151, 0.15), 'MI_Paper')
    return b


@prop('SM_Box_Open', 'Props/Clutter', 'box')
def box_open():
    b = Builder('SM_Box_Open')
    w, d, h = 0.6, 0.42, 0.26
    b.box((w, d, 0.01), (0, 0, 0.005), 'MI_Cardboard')
    for s in (-1, 1):
        b.box((w, 0.008, h), (0, s * d / 2, h / 2), 'MI_Cardboard')
        b.box((0.008, d, h), (s * w / 2, 0, h / 2), 'MI_Cardboard')
        b.box((w, 0.006, d / 2), (0, s * (d / 2 + 0.07), h + 0.05), 'MI_Cardboard', rot=(s * -35, 0, 0))
    b.box((w - 0.04, d - 0.04, 0.14), (0, 0, 0.08), 'MI_Paper')
    return b


@prop('SM_Crate_Wood', 'Props/Clutter', 'box')
def crate():
    b = Builder('SM_Crate_Wood')
    w, d, h = 0.8, 0.6, 0.5
    b.box((w - 0.04, d - 0.04, h - 0.04), (0, 0, h / 2), 'MI_Wood_Dark')
    for z in (0.06, 0.25, 0.44):
        for s in (-1, 1):
            b.box((w, 0.02, 0.1), (0, s * d / 2, z), 'MI_Wood_Raw', bevel=0.003, uv_offset=(z, s))
            b.box((0.02, d, 0.1), (s * w / 2, 0, z), 'MI_Wood_Raw', bevel=0.003, uv_offset=(z * 2, s))
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.box((0.05, 0.05, h), (sx * (w / 2 - 0.015), sy * (d / 2 - 0.015), h / 2), 'MI_Wood_Raw', bevel=0.004)
    b.box((w, d, 0.02), (0, 0, h), 'MI_Wood_Raw', bevel=0.003)
    return b


@prop('SM_Barrel', 'Props/Clutter', 'convex')
def barrel():
    b = Builder('SM_Barrel')
    r, h = 0.29, 0.88
    prof = [(0, 0), (r - 0.01, 0), (r, 0.01), (r, 0.02), (r + 0.008, 0.03), (r, 0.04)]
    for z in (0.29, 0.59):
        prof += [(r, z - 0.02), (r + 0.01, z), (r, z + 0.02)]
    prof += [(r, h - 0.04), (r + 0.008, h - 0.03), (r, h - 0.02), (r - 0.01, h), (0, h - 0.005)]
    b.lathe(prof, mat='MI_Metal_Blue', segs=28)
    b.cyl(0.03, 0.012, (0.16, 0, h - 0.004), 'MI_Metal_Grey', segs=10)
    return b


@prop('SM_Tire', 'Props/Clutter', 'convex')
def tire():
    b = Builder('SM_Tire')
    b.torus(0.25, 0.1, (0, 0, 0.1), 'MI_Tire', segs=32, rsegs=12, scale=(1, 1, 1.0))
    return b


@prop('SM_PaintCan', 'Props/Clutter', 'none')
def paint_can():
    b = Builder('SM_PaintCan')
    b.lathe([(0, 0), (0.085, 0), (0.085, 0.18), (0.08, 0.19), (0.075, 0.19), (0, 0.19)], mat='MI_Metal_Cream', segs=20)
    b.torus(0.07, 0.003, (0, 0, 0.19), 'MI_Steel', rot=(90, 0, 0), segs=12, rsegs=4, arc=180)
    return b


@prop('SM_Bucket', 'Props/Clutter', 'none')
def bucket():
    b = Builder('SM_Bucket')
    b.lathe([(0, 0.0), (0.12, 0), (0.15, 0.3), (0.155, 0.31), (0.145, 0.31), (0.115, 0.012), (0, 0.012)],
            mat='MI_Plastic_Yellow', segs=20)
    return b


@prop('SM_Sawhorse', 'Props/Workshop', 'box')
def sawhorse():
    b = Builder('SM_Sawhorse')
    b.box((1.0, 0.09, 0.05), (0, 0, 0.72), 'MI_Wood_Raw', bevel=0.004)
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.box((0.05, 0.035, 0.78), (sx * 0.4, sy * 0.12, 0.36), 'MI_Wood_Raw', rot=(sy * -12, 0, 0), bevel=0.003)
    return b


@prop('SM_FireExtinguisher', 'Props/Clutter', 'convex')
def extinguisher():
    b = Builder('SM_FireExtinguisher')
    b.lathe([(0, 0), (0.075, 0), (0.08, 0.02), (0.08, 0.42), (0.06, 0.48), (0.02, 0.5), (0, 0.5)], mat='MI_Metal_Red',
            segs=20)
    b.cyl(0.018, 0.05, (0, 0, 0.52), 'MI_Plastic_Black', segs=10)
    b.box((0.1, 0.02, 0.012), (0.03, 0, 0.555), 'MI_Plastic_Black', rot=(0, -12, 0))
    b.tube([(0.02, 0, 0.53), (0.09, 0, 0.5), (0.1, 0, 0.3), (0.085, 0, 0.18)], 0.008, 'MI_Plastic_Black', bend=0.04)
    b.box((0.09, 0.002, 0.12), (0, -0.081, 0.26), 'MI_Paper')
    return b


# =======================================================================================
# Mission board, desk & paper

@prop('SM_Corkboard', 'Props/Board', 'box')
def corkboard():
    b = Builder('SM_Corkboard')
    w, h = 2.4, 1.3
    b.box((w, 0.012, h), (0, -0.016, h / 2), 'MI_Cork')
    for z in (0.0, h):
        b.box((w + 0.07, 0.035, 0.05), (0, -0.0175, z), 'MI_Wood_Dark', bevel=0.004)
    for x in (-w / 2, w / 2):
        b.box((0.05, 0.035, h), (x, -0.0175, h / 2), 'MI_Wood_Dark', bevel=0.004)
    b.box((w, 0.01, h), (0, -0.005, h / 2), 'MI_Wood_Dark')
    return b


def _paper(name, w, h, mat, uv=(0, 0, 1, 1), curl=0.0):
    b = Builder(name)
    b.plane(w, h, (0, -0.001 - curl, 0), mat, uv_rect=uv)
    return b


@prop('SM_Board_Map', 'Props/Board', 'none')
def board_map():
    return _paper('SM_Board_Map', 0.86, 0.645, 'MI_Img_Map')


for _i in range(8):
    _u = (_i % 4) / 4
    _v = 1 - (_i // 4 + 1) / 2
    _sticky = _i in (1, 4, 7)
    _size = 0.08 if _sticky else 0.13

    def _make(i=_i, u=_u, v=_v, size=_size):
        return _paper(f'SM_Note_{i}', size, size, 'MI_Img_Notes', (u + 0.004, v + 0.008, u + 0.246, v + 0.492))
    prop(f'SM_Note_{_i}', 'Props/Board', 'none')(_make)

for _i in range(8):
    _u = (_i % 4) / 4
    _v = 1 - (_i // 4 + 1) / 2

    def _make_p(i=_i, u=_u, v=_v):
        return _paper(f'SM_Polaroid_{i}', 0.1, 0.1, 'MI_Img_Polaroids', (u, v, u + 0.25, v + 0.5))
    prop(f'SM_Polaroid_{_i}', 'Props/Board', 'none')(_make_p)


@prop('SM_Poster_Missing', 'Props/Board', 'none')
def poster_missing():
    return _paper('SM_Poster_Missing', 0.42, 0.56, 'MI_Img_PosterMissing')


@prop('SM_Poster_Museum', 'Props/Board', 'none')
def poster_museum():
    return _paper('SM_Poster_Museum', 0.42, 0.56, 'MI_Img_PosterMuseum')


@prop('SM_Calendar', 'Props/Board', 'none')
def calendar():
    b = _paper('SM_Calendar', 0.3, 0.4, 'MI_Img_Calendar')
    b.cyl(0.004, 0.01, (0, -0.005, 0.19), 'MI_Steel', rot=(90, 0, 0), segs=6)
    return b


@prop('SM_Pushpin', 'Props/Board', 'none')
def pushpin():
    b = Builder('SM_Pushpin')
    b.cyl(0.006, 0.004, (0, -0.006, 0), 'MI_Plastic_Red', rot=(90, 0, 0), segs=8)
    b.cyl(0.003, 0.008, (0, -0.002, 0), 'MI_Plastic_Red', rot=(90, 0, 0), segs=6)
    return b


@prop('SM_Newspaper', 'Props/Desk', 'none')
def newspaper():
    b = Builder('SM_Newspaper')
    b.plane(0.36, 0.36, (0, 0, 0.003), 'MI_Img_Newspaper', rot=(-90, 0, 0))
    b.box((0.36, 0.36, 0.004), (0.004, 0.003, 0.001), 'MI_Paper')
    return b


@prop('SM_Desk_Wood', 'Props/Desk', 'box')
def desk_wood():
    b = Builder('SM_Desk_Wood')
    w, d, h = 1.4, 0.7, 0.76
    b.box((w, d, 0.035), (0, 0, h - 0.0175), 'MI_Wood_Dark', bevel=0.006)
    b.box((0.42, d - 0.06, h - 0.05), (w / 2 - 0.23, 0, (h - 0.05) / 2 + 0.01), 'MI_Wood_Dark', bevel=0.004)
    for k in range(3):
        z = 0.12 + k * 0.22
        b.box((0.38, 0.02, 0.19), (w / 2 - 0.23, -d / 2 + 0.02, z + 0.08), 'MI_Wood_Raw', bevel=0.003)
        b.box((0.08, 0.02, 0.015), (w / 2 - 0.23, -d / 2 + 0.005, z + 0.12), 'MI_Brass', bevel=0.003)
    for sy in (-1, 1):
        b.box((0.05, 0.05, h - 0.035), (-w / 2 + 0.05, sy * (d / 2 - 0.05), (h - 0.035) / 2), 'MI_Wood_Dark',
              bevel=0.004)
    b.box((w - 0.5, 0.02, 0.3), (-0.2, d / 2 - 0.04, h - 0.2), 'MI_Wood_Dark')
    return b


@prop('SM_Chair_Wood', 'Props/Desk', 'box')
def chair_wood():
    b = Builder('SM_Chair_Wood')
    s = 0.44
    b.box((s, s, 0.035), (0, 0, 0.45), 'MI_Wood_Raw', bevel=0.006)
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.box((0.035, 0.035, 0.45), (sx * (s / 2 - 0.03), sy * (s / 2 - 0.03), 0.225), 'MI_Wood_Raw', bevel=0.003)
        b.box((0.035, 0.035, 0.5), (sx * (s / 2 - 0.03), s / 2 - 0.03, 0.72), 'MI_Wood_Raw', rot=(-6, 0, 0), bevel=0.003)
    for z in (0.75, 0.88):
        b.box((s - 0.04, 0.025, 0.07), (0, s / 2 - 0.01 + (z - 0.6) * 0.1, z), 'MI_Wood_Raw', rot=(-6, 0, 0), bevel=0.003)
    b.box((s - 0.06, 0.02, 0.02), (0, 0, 0.15), 'MI_Wood_Raw')
    return b


@prop('SM_Stool', 'Props/Desk', 'convex')
def stool():
    b = Builder('SM_Stool')
    b.cyl(0.17, 0.04, (0, 0, 0.64), 'MI_Wood_Raw', segs=20)
    for i in range(4):
        a = i * math.pi / 2 + math.pi / 4
        b.tube([(math.cos(a) * 0.12, math.sin(a) * 0.12, 0.62), (math.cos(a) * 0.19, math.sin(a) * 0.19, 0)], 0.014,
               'MI_Metal_Black', segs=8)
    b.torus(0.16, 0.008, (0, 0, 0.22), 'MI_Metal_Black', segs=20, rsegs=6)
    return b


@prop('SM_DeskLamp', 'Lights', 'none')
def desk_lamp():
    """Bulb at (0.19, 0, 0.415), shining along (0.42, 0, -0.9)."""
    b = Builder('SM_DeskLamp')
    b.cyl(0.08, 0.025, (0, 0, 0.0125), 'MI_Metal_Green', segs=20)
    b.tube([(0, 0, 0.02), (0.02, 0, 0.3), (0.16, 0, 0.45)], 0.008, 'MI_Metal_Green', segs=8)
    b.lathe([(0.025, 0.06), (0.05, 0.05), (0.09, -0.02), (0.085, -0.025), (0.045, 0.04), (0.02, 0.05)],
            (0.18, 0, 0.43), 'MI_Metal_Green', rot=(0, -25, 0), segs=20)
    b.sphere(0.025, (0.19, 0, 0.415), 'MI_Emit_Bulb', segs=10, rings=6)
    return b


@prop('SM_Mug', 'Props/Clutter', 'none')
def mug():
    b = Builder('SM_Mug')
    b.lathe([(0, 0), (0.04, 0), (0.042, 0.095), (0.038, 0.095), (0.036, 0.008), (0, 0.008)], mat='MI_Ceramic', segs=16)
    b.torus(0.028, 0.006, (0.045, 0, 0.05), 'MI_Ceramic', rot=(90, 0, 0), segs=10, rsegs=6, arc=200)
    return b


@prop('SM_BeerBottle', 'Props/Clutter', 'none')
def beer_bottle():
    b = Builder('SM_BeerBottle')
    b.lathe([(0, 0), (0.03, 0), (0.031, 0.01), (0.031, 0.15), (0.012, 0.19), (0.011, 0.225), (0.013, 0.23),
             (0.0, 0.232)], mat='MI_Glass_Bottle', segs=14)
    b.cyl(0.0315, 0.06, (0, 0, 0.09), 'MI_Paper', segs=14)
    return b


@prop('SM_Can', 'Props/Clutter', 'none')
def soda_can():
    b = Builder('SM_Can')
    b.lathe([(0, 0.004), (0.026, 0), (0.033, 0.012), (0.033, 0.108), (0.027, 0.12), (0, 0.118)], mat='MI_Metal_Red',
            segs=14)
    return b


@prop('SM_PizzaBox', 'Props/Clutter', 'box')
def pizza_box():
    b = Builder('SM_PizzaBox')
    b.box((0.4, 0.4, 0.045), (0, 0, 0.0225), 'MI_Cardboard', bevel=0.003)
    b.box((0.4, 0.4, 0.045), (0.02, -0.01, 0.0675), 'MI_Cardboard', rot=(0, 0, 8), bevel=0.003)
    return b


@prop('SM_Ashtray', 'Props/Clutter', 'none')
def ashtray():
    b = Builder('SM_Ashtray')
    b.lathe([(0, 0), (0.06, 0), (0.065, 0.03), (0.055, 0.03), (0.045, 0.012), (0, 0.012)], mat='MI_Glass_Dirty', segs=16)
    for i in range(3):
        a = i * 2.2
        b.cyl(0.004, 0.045, (math.cos(a) * 0.02, math.sin(a) * 0.02, 0.016), 'MI_Paper', rot=(85, 0, math.degrees(a)),
              segs=6)
    return b


@prop('SM_Magazine', 'Props/Clutter', 'none')
def magazine():
    b = Builder('SM_Magazine')
    b.box((0.21, 0.28, 0.006), (0, 0, 0.003), 'MI_Paper')
    b.plane(0.2, 0.27, (0, 0, 0.0065), 'MI_Img_Newspaper', rot=(-90, 0, 0), uv_rect=(0.05, 0.3, 0.95, 0.95))
    return b


@prop('SM_Binder', 'Props/Desk', 'none')
def binder():
    b = Builder('SM_Binder')
    b.box((0.06, 0.29, 0.32), (0, 0, 0.16), 'MI_Plastic_Black', bevel=0.004)
    b.box((0.002, 0.08, 0.12), (-0.031, 0, 0.2), 'MI_Paper')
    return b


@prop('SM_Clipboard', 'Props/Desk', 'none')
def clipboard():
    b = Builder('SM_Clipboard')
    b.box((0.23, 0.32, 0.004), (0, 0, 0.002), 'MI_Wood_Raw')
    b.plane(0.21, 0.28, (0, -0.01, 0.0045), 'MI_Img_Notes', rot=(-90, 0, 0), uv_rect=(0.505, 0.51, 0.745, 0.99))
    b.box((0.09, 0.03, 0.012), (0, 0.14, 0.008), 'MI_Chrome', bevel=0.003)
    return b


# =======================================================================================
# Lounge

def _cushion(b, size, loc, mat, rot=(0, 0, 0)):
    b.box(size, loc, mat, rot=rot, bevel=min(size) * 0.3, segs=3)


@prop('SM_Couch', 'Props/Lounge', 'box')
def couch():
    b = Builder('SM_Couch')
    w, d = 2.0, 0.9
    m = 'MI_Fabric_Olive'
    b.box((w - 0.1, d - 0.1, 0.25), (0, 0, 0.2), m, bevel=0.03, segs=2)
    for i in range(3):
        _cushion(b, (0.6, 0.62, 0.14), ((i - 1) * 0.6, -0.08, 0.38), m, rot=(0, 0, (i - 1) * 1.5))
        _cushion(b, (0.6, 0.18, 0.5), ((i - 1) * 0.6, 0.27, 0.62), m, rot=(-12, 0, 0))
    for s in (-1, 1):
        _cushion(b, (0.18, d, 0.42), (s * (w / 2 - 0.09), 0, 0.42), m)
    _cushion(b, (w - 0.1, 0.16, 0.5), (0, d / 2 - 0.08, 0.55), m)
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.cyl(0.025, 0.08, (sx * (w / 2 - 0.08), sy * (d / 2 - 0.08), 0.04), 'MI_Wood_Dark', segs=10, radius2=0.03)
    return b


@prop('SM_Armchair', 'Props/Lounge', 'box')
def armchair():
    b = Builder('SM_Armchair')
    m = 'MI_Fabric_Brown'
    b.box((0.8, 0.8, 0.25), (0, 0, 0.2), m, bevel=0.03)
    _cushion(b, (0.56, 0.62, 0.14), (0, -0.06, 0.38), m)
    _cushion(b, (0.56, 0.2, 0.55), (0, 0.27, 0.64), m, rot=(-10, 0, 0))
    for s in (-1, 1):
        _cushion(b, (0.16, 0.8, 0.44), (s * 0.34, 0, 0.42), m)
    _cushion(b, (0.8, 0.16, 0.66), (0, 0.33, 0.6), m)
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.cyl(0.022, 0.08, (sx * 0.33, sy * 0.33, 0.04), 'MI_Wood_Dark', segs=10, radius2=0.028)
    return b


@prop('SM_CoffeeTable', 'Props/Lounge', 'box')
def coffee_table():
    b = Builder('SM_CoffeeTable')
    b.box((1.1, 0.6, 0.04), (0, 0, 0.4), 'MI_Wood_Dark', bevel=0.008)
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.cyl(0.025, 0.38, (sx * 0.48, sy * 0.24, 0.19), 'MI_Wood_Dark', segs=10, radius2=0.02)
    b.box((0.98, 0.5, 0.015), (0, 0, 0.12), 'MI_Wood_Dark')
    return b


@prop('SM_Rug', 'Props/Lounge', 'none')
def rug():
    b = Builder('SM_Rug')
    b.box((2.4, 1.7, 0.012), (0, 0, 0.006), 'MI_Fabric_Rug', bevel=0.004)
    for s in (-1, 1):
        for i in range(36):
            b.box((0.05, 0.01, 0.003), (s * 1.225, (i - 17.5) * 0.045, 0.002), 'MI_Fabric_Shade')
    return b


@prop('SM_TV_CRT', 'Props/Lounge', 'box')
def tv_crt():
    """Screen centre at (0.0, -0.27, 0.28); the Unreal rect light sits just in front."""
    b = Builder('SM_TV_CRT')
    b.box((0.62, 0.5, 0.5), (0, 0.02, 0.25), 'MI_Wood_Dark', bevel=0.02)
    b.box((0.58, 0.46, 0.44), (0, 0.18, 0.24), 'MI_Plastic_Black', bevel=0.04)
    b.box((0.44, 0.02, 0.36), (-0.06, -0.235, 0.28), 'MI_Plastic_Black', bevel=0.012)
    b.grid_plane(0.38, 0.3, (-0.06, -0.248, 0.28), 'MI_Screen_TV', rot=(90, 0, 0), sag=0.015, nx=6, ny=6,
                 uv_scale=2.6)
    b.box((0.12, 0.02, 0.4), (0.24, -0.235, 0.26), 'MI_Plastic_Grey', bevel=0.006)
    for z in (0.38, 0.28):
        b.cyl(0.022, 0.03, (0.24, -0.255, z), 'MI_Plastic_Black', rot=(90, 0, 0), segs=12)
    for k in range(5):
        b.box((0.08, 0.004, 0.006), (0.24, -0.247, 0.12 + k * 0.02), 'MI_Plastic_Black')
    for s in (-1, 1):
        b.tube([(0, 0.1, 0.5), (s * 0.22, 0.2, 0.92)], 0.004, 'MI_Chrome', segs=6)
    b.sphere(0.035, (0, 0.1, 0.51), 'MI_Plastic_Black', scale=(1, 1, 0.6))
    return b


@prop('SM_TVStand', 'Props/Lounge', 'box')
def tv_stand():
    b = Builder('SM_TVStand')
    b.box((0.95, 0.45, 0.03), (0, 0, 0.5), 'MI_Wood_Dark', bevel=0.005)
    b.box((0.95, 0.45, 0.03), (0, 0, 0.08), 'MI_Wood_Dark', bevel=0.005)
    for s in (-1, 1):
        b.box((0.03, 0.45, 0.5), (s * 0.46, 0, 0.27), 'MI_Wood_Dark', bevel=0.004)
    b.box((0.95, 0.02, 0.5), (0, 0.215, 0.27), 'MI_Wood_Dark')
    for i in range(6):
        b.box((0.03, 0.2, 0.18), (-0.35 + i * 0.035, 0.0, 0.185), ('MI_Plastic_Black', 'MI_Plastic_Red', 'MI_Cardboard')[i % 3])
    return b


@prop('SM_Radio_Vintage', 'Props/Lounge', 'box')
def radio_vintage():
    b = Builder('SM_Radio_Vintage')
    b.box((0.48, 0.22, 0.3), (0, 0, 0.15), 'MI_Wood_Dark', bevel=0.03, segs=3)
    b.box((0.26, 0.01, 0.2), (-0.08, -0.11, 0.15), 'MI_Fabric_Brown', bevel=0.004)
    b.box((0.14, 0.008, 0.06), (0.14, -0.11, 0.2), 'MI_Emit_Amber', bevel=0.003)
    for x in (0.1, 0.18):
        b.cyl(0.022, 0.03, (x, -0.12, 0.08), 'MI_Bakelite', rot=(90, 0, 0), segs=14)
    b.box((0.3, 0.2, 0.012), (0, 0, 0.006), 'MI_Wood_Dark')
    return b


@prop('SM_FloorLamp', 'Lights', 'none')
def floor_lamp():
    """Bulb at (0, 0, 1.45)."""
    b = Builder('SM_FloorLamp')
    b.lathe([(0, 0), (0.16, 0), (0.16, 0.02), (0.05, 0.04), (0, 0.04)], mat='MI_Brass', segs=20)
    b.cyl(0.012, 1.4, (0, 0, 0.74), 'MI_Brass', segs=8)
    b.lathe([(0.24, 1.3), (0.13, 1.62), (0.12, 1.615), (0.23, 1.305)], mat='MI_Emit_Shade', segs=24, closed=True)
    b.sphere(0.035, (0, 0, 1.45), 'MI_Emit_Bulb', segs=10, rings=6)
    return b


@prop('SM_SpaceHeater', 'Props/Lounge', 'box')
def space_heater():
    """Glow at (0, -0.08, 0.3)."""
    b = Builder('SM_SpaceHeater')
    b.box((0.42, 0.16, 0.5), (0, 0, 0.27), 'MI_Metal_Cream', bevel=0.02)
    b.box((0.34, 0.01, 0.3), (0, -0.075, 0.3), 'MI_Emit_Ember')
    for i in range(8):
        b.box((0.006, 0.012, 0.32), (-0.15 + i * 0.043, -0.085, 0.3), 'MI_Chrome')
    b.box((0.36, 0.2, 0.03), (0, 0, 0.015), 'MI_Metal_Black', bevel=0.005)
    b.cyl(0.018, 0.02, (0.16, -0.08, 0.48), 'MI_Bakelite', rot=(90, 0, 0), segs=10)
    return b


# =======================================================================================
# Customisation corner

@prop('SM_Mirror_Standing', 'Props/Wardrobe', 'box')
def mirror_standing():
    b = Builder('SM_Mirror_Standing')
    w, h = 0.62, 1.6
    tilt = (-6, 0, 0)
    for x in (-w / 2, w / 2):
        b.box((0.05, 0.05, h + 0.1), (x, 0.02, 0.05 + (h + 0.1) / 2), 'MI_Wood_Dark', rot=tilt, bevel=0.006)
    for z in (0.15, 0.15 + h):
        b.box((w, 0.05, 0.05), (0, 0.02 + (z - 0.9) * 0.1, z), 'MI_Wood_Dark', rot=tilt, bevel=0.006)
    b.plane(w - 0.04, h - 0.04, (0, -0.002 + 0.0, 0.15 + h / 2), 'MI_Mirror', rot=tilt)
    b.box((w - 0.04, 0.01, h - 0.04), (0, 0.035, 0.15 + h / 2), 'MI_Wood_Dark', rot=tilt)
    for x in (-w / 2, w / 2):
        b.box((0.06, 0.5, 0.04), (x, 0.08, 0.02), 'MI_Wood_Dark', bevel=0.006)
    return b


@prop('SM_ClothingRack', 'Props/Wardrobe', 'box')
def clothing_rack():
    b = Builder('SM_ClothingRack')
    w, h = 1.5, 1.62
    b.tube([(-w / 2, 0, 0.08), (-w / 2, 0, h), (w / 2, 0, h), (w / 2, 0, 0.08)], 0.016, 'MI_Chrome', bend=0.06)
    for s in (-1, 1):
        b.cyl(0.016, 0.5, (s * w / 2, 0, 0.08), 'MI_Chrome', rot=(90, 0, 0), segs=10)
        for t in (-1, 1):
            b.sphere(0.035, (s * w / 2, t * 0.23, 0.035), 'MI_Rubber_Black', segs=10, rings=6)
    return b


@prop('SM_Hanger', 'Props/Wardrobe', 'none')
def hanger():
    b = Builder('SM_Hanger')
    b.tube([(-0.21, 0, -0.17), (0, 0, -0.07), (0.21, 0, -0.17), (-0.21, 0, -0.17)], 0.003, 'MI_Steel', segs=5)
    b.torus(0.02, 0.003, (0, 0, -0.03), 'MI_Steel', rot=(90, 0, 0), segs=10, rsegs=4, arc=260)
    return b


def _garment(name, mat, length, shoulders=0.46, hood=False):
    b = Builder(name)
    hw = shoulders / 2
    poly = [(-hw, -0.07), (-hw - 0.05, -0.14), (-hw - 0.07, -length * 0.6), (-hw - 0.02, -length * 0.62),
            (-hw + 0.04, -0.3), (-hw + 0.02, -length), (hw - 0.02, -length), (hw - 0.04, -0.3), (hw + 0.02, -length * 0.62),
            (hw + 0.07, -length * 0.6), (hw + 0.05, -0.14), (hw, -0.07), (0.06, -0.05), (-0.06, -0.05)]
    b.prism(poly, 0.09, (0, 0, -0.02), mat, bevel=0.02)
    if hood:
        b.sphere(0.13, (0, 0.03, -0.12), mat, scale=(1, 0.6, 0.9))
    return b


@prop('SM_Garment_Coat', 'Props/Wardrobe', 'none')
def garment_coat():
    return _garment('SM_Garment_Coat', 'MI_Fabric_Navy', 1.05)


@prop('SM_Garment_Jacket', 'Props/Wardrobe', 'none')
def garment_jacket():
    return _garment('SM_Garment_Jacket', 'MI_Leather_Black', 0.68)


@prop('SM_Garment_Hoodie', 'Props/Wardrobe', 'none')
def garment_hoodie():
    return _garment('SM_Garment_Hoodie', 'MI_Fabric_Grey', 0.7, hood=True)


@prop('SM_Bench_Wood', 'Props/Wardrobe', 'box')
def bench_wood():
    b = Builder('SM_Bench_Wood')
    for i in range(3):
        b.box((1.2, 0.11, 0.035), (0, (i - 1) * 0.12, 0.44), 'MI_Wood_Raw', bevel=0.004, uv_offset=(i * 0.3, 0))
    for s in (-1, 1):
        b.box((0.05, 0.32, 0.42), (s * 0.5, 0, 0.21), 'MI_Metal_Black', bevel=0.004)
    return b


@prop('SM_Shoebox', 'Props/Wardrobe', 'none')
def shoebox():
    b = Builder('SM_Shoebox')
    b.box((0.34, 0.2, 0.12), (0, 0, 0.06), 'MI_Cardboard', bevel=0.003)
    b.box((0.35, 0.21, 0.035), (0, 0, 0.118), 'MI_Plastic_Red', bevel=0.003)
    return b


# =======================================================================================
# The fence's cage

@prop('SM_Cage_Panel', 'Props/Cage', 'box')
def cage_panel():
    """1.2 m wide, 2.4 m tall, centred on x, standing on y = 0."""
    b = Builder('SM_Cage_Panel')
    w, h = 1.2, 2.4
    b.tube([(-w / 2, 0, 0), (-w / 2, 0, h), (w / 2, 0, h), (w / 2, 0, 0)], 0.022, 'MI_Steel', bend=0.0)
    b.cyl(0.018, w, (0, 0, 1.2), 'MI_Steel', rot=(0, 90, 0), segs=10)
    b.plane(w, h, (0, 0, h / 2), 'MI_ChainLink', uv_rect=(0, 0, w / 0.2, h / 0.2))
    return b


@prop('SM_Counter', 'Props/Cage', 'box')
def counter():
    b = Builder('SM_Counter')
    w, d, h = 1.6, 0.6, 1.02
    b.box((w, d, 0.05), (0, 0, h - 0.025), 'MI_Wood_Raw', bevel=0.008)
    b.box((w - 0.06, d - 0.08, h - 0.08), (0, 0.02, (h - 0.08) / 2 + 0.03), 'MI_Wood_Painted', bevel=0.006)
    for i in range(6):
        b.box((0.02, 0.01, h - 0.2), (-w / 2 + 0.13 + i * 0.27, -d / 2 + 0.015, (h - 0.08) / 2 + 0.03), 'MI_Wood_Dark')
    b.box((w - 0.08, d - 0.12, 0.06), (0, 0.02, 0.03), 'MI_Metal_Black')
    return b


@prop('SM_CashBox', 'Props/Cage', 'box')
def cash_box():
    b = Builder('SM_CashBox')
    b.box((0.3, 0.24, 0.1), (0, 0, 0.05), 'MI_Metal_Green', bevel=0.006)
    b.box((0.31, 0.25, 0.02), (0, 0.04, 0.11), 'MI_Metal_Green', rot=(20, 0, 0), bevel=0.004)
    b.box((0.04, 0.006, 0.02), (0, -0.122, 0.07), 'MI_Chrome')
    for i in range(4):
        b.box((0.06, 0.15, 0.025), (-0.1 + i * 0.065, 0, 0.09), 'MI_Cash')
    return b


@prop('SM_Phone_Rotary', 'Props/Cage', 'box')
def phone_rotary():
    b = Builder('SM_Phone_Rotary')
    b.prism([(-0.11, 0), (0.11, 0), (0.09, 0.1), (-0.09, 0.1)], 0.2, (0, 0, 0), 'MI_Bakelite', axis='Y', bevel=0.012)
    b.cyl(0.065, 0.012, (0, -0.06, 0.08), 'MI_Plastic_White', rot=(-30, 0, 0), segs=20)
    b.cyl(0.02, 0.016, (0, -0.06, 0.086), 'MI_Bakelite', rot=(-30, 0, 0), segs=12)
    b.box((0.24, 0.05, 0.04), (0, 0.02, 0.125), 'MI_Bakelite', bevel=0.015)
    for s in (-1, 1):
        b.sphere(0.035, (s * 0.1, 0.02, 0.12), 'MI_Bakelite', scale=(1, 1, 0.7))
    b.tube([(0.1, 0.1, 0.03), (0.18, 0.18, 0.0), (0.25, 0.12, 0.0)], 0.004, 'MI_Cable', segs=5, bend=0.04)
    return b


@prop('SM_Safe', 'Props/Cage', 'box')
def safe():
    b = Builder('SM_Safe')
    b.box((0.6, 0.6, 0.8), (0, 0, 0.42), 'MI_Metal_Green', bevel=0.02)
    b.box((0.5, 0.02, 0.66), (0, -0.305, 0.42), 'MI_Metal_Green', bevel=0.008)
    b.cyl(0.06, 0.03, (0.05, -0.32, 0.55), 'MI_Brass', rot=(90, 0, 0), segs=24)
    b.cyl(0.015, 0.06, (-0.12, -0.33, 0.4), 'MI_Chrome', rot=(90, 0, 0), segs=10)
    b.box((0.16, 0.02, 0.02), (-0.12, -0.36, 0.4), 'MI_Chrome', bevel=0.006)
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.cyl(0.03, 0.03, (sx * 0.25, sy * 0.25, 0.015), 'MI_Metal_Black', segs=10)
    return b


@prop('SM_Duffel', 'Props/Cage', 'convex')
def duffel():
    b = Builder('SM_Duffel')
    b.sphere(0.22, (0, 0, 0.18), 'MI_Nylon_Black', scale=(2.4, 1.0, 0.85), segs=20, rings=12)
    for s in (-1, 1):
        b.torus(0.12, 0.012, (s * 0.12, 0, 0.36), 'MI_Nylon_Black', rot=(90, 0, 90), segs=12, rsegs=6, arc=180)
    b.tube([(-0.4, -0.15, 0.25), (0.4, -0.15, 0.25)], 0.006, 'MI_Chrome', segs=6)
    for s in (-1, 1):
        b.cyl(0.18, 0.01, (s * 0.52, 0, 0.18), 'MI_Nylon_Olive', rot=(0, 90, 0), segs=16)
    return b


@prop('SM_Sign_Fence', 'Props/Cage', 'none')
def sign_fence():
    b = Builder('SM_Sign_Fence')
    b.box((0.82, 0.02, 0.42), (0, -0.01, 0), 'MI_Wood_Dark', bevel=0.005)
    b.plane(0.8, 0.4, (0, -0.0205, 0), 'MI_Img_SignFence')
    for s in (-1, 1):
        b.tube([(s * 0.36, 0, 0.21), (0, 0, 0.45)], 0.002, 'MI_Steel', segs=4)
    return b


# =======================================================================================
# Security desk

@prop('SM_Desk_Metal', 'Props/Desk', 'box')
def desk_metal():
    b = Builder('SM_Desk_Metal')
    w, d, h = 1.5, 0.75, 0.75
    b.box((w, d, 0.035), (0, 0, h - 0.0175), 'MI_Metal_Grey', bevel=0.006)
    b.box((w - 0.04, d - 0.04, 0.012), (0, 0, h + 0.006), 'MI_Plastic_Beige')
    for x in (-w / 2 + 0.22, w / 2 - 0.22):
        b.box((0.42, d - 0.04, h - 0.06), (x, 0, (h - 0.06) / 2 + 0.02), 'MI_Metal_Grey', bevel=0.004)
        for k in range(2):
            b.box((0.38, 0.015, 0.3), (x, -d / 2 + 0.015, 0.2 + k * 0.33), 'MI_Metal_Grey', bevel=0.004)
            b.box((0.12, 0.02, 0.015), (x, -d / 2, 0.3 + k * 0.33), 'MI_Chrome', bevel=0.003)
    return b


@prop('SM_CRT_Monitor', 'Props/Desk', 'box')
def crt_monitor():
    b = Builder('SM_CRT_Monitor')
    b.box((0.36, 0.34, 0.32), (0, 0, 0.22), 'MI_Plastic_Beige', bevel=0.02)
    b.box((0.3, 0.26, 0.24), (0, 0.18, 0.21), 'MI_Plastic_Beige', bevel=0.04)
    b.grid_plane(0.27, 0.21, (0, -0.172, 0.23), 'MI_Screen_Monitor', rot=(90, 0, 0), sag=0.008, nx=4, ny=4,
                 uv_scale=3.5)
    b.box((0.2, 0.2, 0.06), (0, 0.02, 0.03), 'MI_Plastic_Beige', bevel=0.01)
    b.box((0.02, 0.005, 0.01), (0.14, -0.172, 0.08), 'MI_Emit_Green')
    return b


@prop('SM_Keyboard', 'Props/Desk', 'none')
def keyboard():
    b = Builder('SM_Keyboard')
    b.box((0.46, 0.17, 0.03), (0, 0, 0.015), 'MI_Plastic_Beige', bevel=0.006)
    for r in range(5):
        for c in range(15):
            b.box((0.024, 0.024, 0.012), (-0.2 + c * 0.0285, -0.06 + r * 0.028, 0.034), 'MI_Plastic_Grey', bevel=0.002)
    return b


@prop('SM_Walkie', 'Props/Desk', 'none')
def walkie():
    b = Builder('SM_Walkie')
    b.box((0.06, 0.035, 0.16), (0, 0, 0.08), 'MI_Plastic_Black', bevel=0.008)
    b.cyl(0.006, 0.12, (0.018, 0, 0.22), 'MI_Rubber_Black', segs=6)
    b.box((0.045, 0.004, 0.05), (0, -0.018, 0.11), 'MI_Plastic_Grey')
    return b


# =======================================================================================
# Boiler room

@prop('SM_Boiler', 'Props/Boiler', 'convex')
def boiler():
    """Flame window at (0, -0.46, 0.45)."""
    b = Builder('SM_Boiler')
    b.lathe([(0, 0), (0.45, 0), (0.45, 0.05), (0.43, 0.06), (0.43, 1.55), (0.45, 1.56), (0.45, 1.6), (0.3, 1.72),
             (0.12, 1.75), (0, 1.75)], mat='MI_Metal_Rust', segs=28)
    b.cyl(0.12, 0.6, (0, 0, 2.0), 'MI_Steel', segs=14)
    b.box((0.28, 0.06, 0.24), (0, -0.44, 0.45), 'MI_Metal_Black', bevel=0.01)
    b.box((0.14, 0.02, 0.06), (0, -0.475, 0.45), 'MI_Emit_Ember')
    for z, x in ((1.1, -0.15), (1.1, 0.15)):
        b.cyl(0.055, 0.04, (x, -0.44, z), 'MI_Brass', rot=(90, 0, 0), segs=16)
        b.cyl(0.045, 0.01, (x, -0.465, z), 'MI_Plastic_White', rot=(90, 0, 0), segs=16)
    b.tube([(0.35, 0, 1.3), (0.7, 0, 1.3), (0.7, 0, 2.6)], 0.05, 'MI_Copper', bend=0.12)
    b.tube([(-0.35, 0, 0.3), (-0.75, 0, 0.3), (-0.75, 0, 2.6)], 0.05, 'MI_Metal_Rust', bend=0.12)
    return b


@prop('SM_WaterHeater', 'Props/Boiler', 'convex')
def water_heater():
    b = Builder('SM_WaterHeater')
    b.lathe([(0, 0), (0.28, 0), (0.29, 0.03), (0.29, 1.42), (0.25, 1.5), (0, 1.52)], mat='MI_Metal_Cream', segs=24)
    for x in (-0.08, 0.08):
        b.tube([(x, 0, 1.5), (x, 0, 1.7)], 0.02, 'MI_Copper', segs=8)
    b.box((0.14, 0.02, 0.18), (0, -0.29, 0.3), 'MI_Metal_Grey', bevel=0.005)
    return b


@prop('SM_BreakerBox', 'Props/Boiler', 'box')
def breaker_box():
    """Back on x = 0, front at +X. The lever pivot is at (0.13, 0, 0.0)."""
    b = Builder('SM_BreakerBox')
    b.box((0.11, 0.36, 0.5), (0.055, 0, 0.0), 'MI_Metal_Grey', bevel=0.008)
    b.box((0.02, 0.06, 0.12), (0.12, 0, 0.0), 'MI_Metal_Black', bevel=0.004)
    b.box((0.004, 0.14, 0.1), (0.112, 0, 0.17), 'MI_Paper')
    b.box((0.004, 0.06, 0.06), (0.112, 0.12, -0.17), 'MI_Metal_Yellow')
    b.tube([(0.05, 0, 0.25), (0.05, 0, 0.6)], 0.016, 'MI_Steel', segs=8)
    return b


@prop('SM_BreakerLever', 'Props/Boiler', 'none')
def breaker_lever():
    """Pivot at the origin; the handle points +X (pitch rotates it up/down)."""
    b = Builder('SM_BreakerLever')
    b.cyl(0.012, 0.07, (0, 0, 0), 'MI_Steel', rot=(90, 0, 0), segs=10)
    b.box((0.13, 0.016, 0.02), (0.065, 0, 0), 'MI_Steel', bevel=0.004)
    b.lathe([(0, 0), (0.016, 0.01), (0.018, 0.05), (0.012, 0.07), (0, 0.072)], (0.12, 0, 0), 'MI_Plastic_Red',
            rot=(0, 90, 0), segs=12)
    return b


@prop('SM_MopBucket', 'Props/Boiler', 'box')
def mop_bucket():
    b = Builder('SM_MopBucket')
    b.box((0.5, 0.3, 0.3), (0, 0, 0.2), 'MI_Plastic_Yellow', bevel=0.03)
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.sphere(0.03, (sx * 0.2, sy * 0.11, 0.03), 'MI_Rubber_Black', segs=8, rings=5)
    b.cyl(0.012, 1.3, (0.12, 0.05, 0.85), 'MI_Wood_Raw', rot=(8, -10, 0), segs=8)
    return b


# =======================================================================================
# Lights (fixtures); emissive slots are driven by HHPracticalLight

@prop('SM_Lamp_Industrial', 'Lights', 'none')
def lamp_industrial():
    """Ceiling mount at origin; bulb at (0, 0, -0.78)."""
    b = Builder('SM_Lamp_Industrial')
    b.cyl(0.05, 0.02, (0, 0, -0.01), 'MI_Metal_Black', segs=14)
    b.cyl(0.004, 0.62, (0, 0, -0.33), 'MI_Cable', segs=6)
    b.cyl(0.025, 0.08, (0, 0, -0.68), 'MI_Metal_Black', segs=12)
    b.lathe([(0.03, -0.0), (0.06, -0.03), (0.25, -0.2), (0.255, -0.21), (0.245, -0.21), (0.055, -0.04), (0.025, -0.01)],
            (0, 0, -0.69), 'MI_Metal_Green', segs=32)
    b.sphere(0.04, (0, 0, -0.78), 'MI_Emit_Bulb', segs=12, rings=8)
    return b


@prop('SM_Lamp_Fluorescent', 'Lights', 'none')
def lamp_fluorescent():
    """Ceiling mount at origin; tubes at z = -0.62."""
    b = Builder('SM_Lamp_Fluorescent')
    for s in (-1, 1):
        b.cyl(0.003, 0.5, (s * 0.5, 0, -0.25), 'MI_Steel', segs=4)
    b.box((1.28, 0.16, 0.06), (0, 0, -0.53), 'MI_Metal_Cream', bevel=0.006)
    b.box((1.22, 0.12, 0.01), (0, 0, -0.565), 'MI_Plastic_White')
    for s in (-1, 1):
        b.cyl(0.013, 1.18, (0, s * 0.035, -0.59), 'MI_Emit_Fluoro', rot=(0, 90, 0), segs=10)
    return b


@prop('SM_Lamp_Bulb', 'Lights', 'none')
def lamp_bulb():
    """Ceiling mount at origin; bulb at (0, 0, -0.98)."""
    b = Builder('SM_Lamp_Bulb')
    b.cyl(0.03, 0.02, (0, 0, -0.01), 'MI_Bakelite', segs=10)
    b.cyl(0.003, 0.88, (0, 0, -0.46), 'MI_Cable', segs=5)
    b.lathe([(0.0, 0.0), (0.018, 0.0), (0.02, -0.05), (0.014, -0.06), (0, -0.06)], (0, 0, -0.88), 'MI_Bakelite', segs=10)
    b.lathe([(0.0, 0.0), (0.012, -0.0), (0.032, -0.04), (0.03, -0.08), (0.0, -0.1)], (0, 0, -0.94), 'MI_Emit_Bulb',
            segs=12)
    b.tube([(0.016, 0, -0.92), (0.03, 0.0, -1.2)], 0.0015, 'MI_Brass', segs=4)
    return b


@prop('SM_Lamp_Caged', 'Lights', 'none')
def lamp_caged():
    """Wall mount (back on y = 0); bulb at (0, -0.12, 0)."""
    b = Builder('SM_Lamp_Caged')
    b.cyl(0.07, 0.03, (0, -0.015, 0), 'MI_Metal_Black', rot=(90, 0, 0), segs=16)
    b.sphere(0.04, (0, -0.12, 0), 'MI_Emit_Bulb', segs=12, rings=8)
    for i in range(6):
        a = i * math.pi / 3
        b.tube([(math.cos(a) * 0.065, -0.03, math.sin(a) * 0.065), (math.cos(a) * 0.075, -0.12, math.sin(a) * 0.075),
                (0, -0.19, 0)], 0.003, 'MI_Metal_Black', segs=4, bend=0.03)
    b.torus(0.075, 0.004, (0, -0.12, 0), 'MI_Metal_Black', rot=(90, 0, 0), segs=16, rsegs=4)
    return b


@prop('SM_Lamp_Bulkhead', 'Lights', 'none')
def lamp_bulkhead():
    """Wall mount (back on y = 0); glass centre at (0, -0.06, 0)."""
    b = Builder('SM_Lamp_Bulkhead')
    b.sphere(0.12, (0, -0.03, 0), 'MI_Metal_Black', scale=(1.3, 0.35, 0.8), segs=16, rings=8)
    b.sphere(0.1, (0, -0.06, 0), 'MI_Emit_Bulb', scale=(1.3, 0.35, 0.8), segs=16, rings=8)
    for k in range(3):
        b.box((0.26, 0.012, 0.01), (0, -0.095, -0.05 + k * 0.05), 'MI_Metal_Black')
    return b


@prop('SM_WorkLight', 'Lights', 'box')
def work_light():
    """Halogen work light on a tripod; lamp face at (0, -0.09, 1.7) facing -Y."""
    b = Builder('SM_WorkLight')
    for i in range(3):
        a = math.radians(90 + i * 120)
        b.tube([(0, 0, 0.75), (math.cos(a) * 0.45, math.sin(a) * 0.45, 0.0)], 0.012, 'MI_Metal_Yellow', segs=8)
        b.sphere(0.02, (math.cos(a) * 0.45, math.sin(a) * 0.45, 0.01), 'MI_Rubber_Black', segs=8, rings=5)
    b.cyl(0.018, 1.0, (0, 0, 1.2), 'MI_Metal_Yellow', segs=10)
    b.cyl(0.014, 0.36, (0, 0, 1.7), 'MI_Metal_Black', rot=(0, 90, 0), segs=8)
    b.box((0.26, 0.12, 0.2), (0, 0.0, 1.7), 'MI_Metal_Yellow', bevel=0.012)
    b.box((0.22, 0.01, 0.16), (0, -0.062, 1.7), 'MI_Emit_Bulb')
    for k in range(4):
        b.box((0.006, 0.012, 0.17), (-0.09 + k * 0.06, -0.072, 1.7), 'MI_Metal_Black')
    for s_ in (-1, 1):
        b.box((0.02, 0.05, 0.24), (s_ * 0.16, 0.0, 1.7), 'MI_Metal_Black', bevel=0.004)
    b.tube([(0, 0.06, 1.62), (0.05, 0.2, 0.9), (0.2, 0.5, 0.0), (0.6, 0.7, 0.0)], 0.006, 'MI_Cable', segs=5, bend=0.2)
    return b


@prop('SM_StreetLamp', 'Lights', 'box')
def street_lamp():
    """Lamp head at (0.9, 0, 4.4)."""
    b = Builder('SM_StreetLamp')
    b.lathe([(0, 0), (0.12, 0), (0.1, 0.4), (0.06, 0.45), (0.05, 4.5), (0, 4.5)], mat='MI_Metal_Black', segs=14)
    b.tube([(0, 0, 4.4), (0.4, 0, 4.6), (0.9, 0, 4.55)], 0.035, 'MI_Metal_Black', bend=0.3)
    b.lathe([(0.0, 0.08), (0.12, 0.05), (0.22, -0.05), (0.05, -0.04), (0, -0.05)], (0.9, 0, 4.45), 'MI_Metal_Black',
            segs=16)
    b.sphere(0.12, (0.9, 0, 4.39), 'MI_Emit_Street', scale=(1, 1, 0.4), segs=14, rings=6)
    return b


# =======================================================================================
# Doors, windows, garage

@prop('SM_Door_Steel', 'Architecture/Doors', 'box')
def door_steel():
    b = Builder('SM_Door_Steel')
    w, h, t = 0.9, 2.04, 0.045
    b.box((w, t, h), (w / 2, 0, h / 2 + 0.01), 'MI_Metal_Grey', bevel=0.004)
    b.box((w - 0.08, 0.004, 0.25), (w / 2, -t / 2 - 0.002, 0.15), 'MI_Steel')
    for s in (-1, 1):
        b.cyl(0.012, 0.05, (w - 0.08, s * (t / 2 + 0.025), 1.02), 'MI_Chrome', rot=(90, 0, 0), segs=10)
        b.box((0.13, 0.02, 0.02), (w - 0.13, s * (t / 2 + 0.05), 1.02), 'MI_Chrome', bevel=0.006)
        b.box((0.05, 0.006, 0.16), (w - 0.08, s * (t / 2 + 0.003), 1.0), 'MI_Chrome', bevel=0.002)
    for z in (0.25, 1.0, 1.75):
        b.cyl(0.012, 0.1, (0.0, 0, z), 'MI_Steel', segs=10)
    b.box((0.2, 0.004, 0.08), (w / 2, -t / 2 - 0.002, 1.55), 'MI_Paper')
    return b


@prop('SM_Door_Wood', 'Architecture/Doors', 'box')
def door_wood():
    b = Builder('SM_Door_Wood')
    w, h, t = 0.86, 2.0, 0.04
    for i in range(6):
        b.box((w / 6 - 0.004, t, h), ((i + 0.5) * w / 6, 0, h / 2 + 0.02), 'MI_Wood_Planks', bevel=0.003, uv_rotate=True,
              uv_offset=(i * 0.13, 0))
    for z in (0.3, 1.0, 1.7):
        b.box((w - 0.06, 0.025, 0.12), (w / 2, 0.032, z), 'MI_Wood_Raw', bevel=0.004)
    b.box((0.06, 0.025, 0.12), (w / 2, 0.032, 0.65), 'MI_Wood_Raw', rot=(0, 52, 0))
    b.box((0.06, 0.025, 0.12), (w / 2, 0.032, 1.35), 'MI_Wood_Raw', rot=(0, 52, 0))
    for z in (0.3, 1.7):
        b.box((0.3, 0.006, 0.04), (0.15, -t / 2 - 0.003, z), 'MI_Metal_Black')
    b.torus(0.04, 0.006, (w - 0.1, -t / 2 - 0.02, 1.0), 'MI_Metal_Black', rot=(90, 0, 0), segs=12, rsegs=5)
    b.cyl(0.02, 0.02, (w - 0.1, -t / 2 - 0.01, 1.04), 'MI_Metal_Black', rot=(90, 0, 0), segs=10)
    return b


@prop('SM_DoorFrame', 'Architecture/Doors', 'complex')
def door_frame():
    """For a 0.92 x 2.06 opening in a 0.25 m wall; pivot bottom centre of the opening."""
    b = Builder('SM_DoorFrame')
    w, h, d = 0.92, 2.06, 0.27
    for s in (-1, 1):
        b.box((0.05, d, h + 0.05), (s * (w / 2 + 0.025), 0, (h + 0.05) / 2), 'MI_Metal_Grey', bevel=0.004)
        for t in (-1, 1):
            b.box((0.07, 0.015, h + 0.08), (s * (w / 2 + 0.035), t * (d / 2 + 0.0075), (h + 0.08) / 2), 'MI_Metal_Grey',
                  bevel=0.003)
    b.box((w + 0.1, d, 0.05), (0, 0, h + 0.025), 'MI_Metal_Grey', bevel=0.004)
    for t in (-1, 1):
        b.box((w + 0.14, 0.015, 0.07), (0, t * (d / 2 + 0.0075), h + 0.045), 'MI_Metal_Grey', bevel=0.003)
    return b


@prop('SM_Window_Basement', 'Architecture/Windows', 'box')
def window_basement():
    """1.2 x 0.6 opening, centred; frame depth fits a 0.25 m wall."""
    b = Builder('SM_Window_Basement')
    w, h, d = 1.2, 0.6, 0.27
    for s in (-1, 1):
        b.box((0.05, d, h + 0.1), (s * (w / 2 + 0.025), 0, 0), 'MI_Metal_Black', bevel=0.004)
        b.box((w + 0.1, d, 0.05), (0, 0, s * (h / 2 + 0.025)), 'MI_Metal_Black', bevel=0.004)
    b.box((0.03, 0.03, h), (0, 0.06, 0), 'MI_Metal_Black')
    b.box((w, 0.03, 0.03), (0, 0.06, 0), 'MI_Metal_Black')
    b.box((w, 0.006, h), (0, 0.07, 0), 'MI_Glass_Dirty')
    for i in range(7):
        b.cyl(0.012, h + 0.08, (-w / 2 + 0.1 + i * (w - 0.2) / 6, -0.1, 0), 'MI_Metal_Rust', segs=8)
    b.box((w + 0.1, 0.02, 0.03), (0, -0.1, h / 2 - 0.02), 'MI_Metal_Rust')
    b.box((w + 0.1, 0.02, 0.03), (0, -0.1, -h / 2 + 0.02), 'MI_Metal_Rust')
    return b


@prop('SM_RollUpDoor', 'Architecture/Doors', 'complex')
def rollup_door():
    """3.2 m wide, 2.9 m tall; centred on x, wall face at y = 0 (door slightly inside)."""
    b = Builder('SM_RollUpDoor')
    w, h = 3.2, 2.9
    slat = 0.1
    win_row = 1.6
    rows = int(h / slat)
    for i in range(rows):
        z = i * slat + slat / 2
        depth = 0.03 if i % 2 == 0 else 0.022
        if abs(z - win_row) < 0.13:
            for k in range(5):
                x0 = -w / 2 + k * w / 5
                b.box((w / 5 - 0.3, depth, slat - 0.004), (x0 + (w / 5 - 0.3) / 2, -0.03, z), 'MI_Metal_Grey',
                      uv_offset=(0, i * 0.37))
                b.box((0.3, 0.004, slat), (x0 + w / 5 - 0.15, -0.025, z), 'MI_Glass_Dirty')
            continue
        b.box((w, depth, slat - 0.004), (0, -0.03, z), 'MI_Metal_Grey', bevel=0.003, uv_offset=(0, i * 0.37))
    b.box((w, 0.05, 0.06), (0, -0.04, 0.03), 'MI_Metal_Black', bevel=0.005)
    for k in range(5):
        x0 = -w / 2 + k * w / 5 + w / 5 - 0.15
        for zz in (win_row - 0.13, win_row + 0.13):
            b.box((0.32, 0.03, 0.02), (x0, -0.035, zz), 'MI_Metal_Black')
        for xx in (x0 - 0.15, x0 + 0.15):
            b.box((0.02, 0.03, 0.27), (xx, -0.035, win_row), 'MI_Metal_Black')
    for s in (-1, 1):
        b.box((0.08, 0.1, h + 0.1), (s * (w / 2 + 0.04), -0.06, (h + 0.1) / 2), 'MI_Steel', bevel=0.004)
    b.cyl(0.25, w + 0.2, (0, -0.28, h + 0.25), 'MI_Metal_Grey', rot=(0, 90, 0), segs=24)
    for s in (-1, 1):
        b.box((0.02, 0.55, 0.55), (s * (w / 2 + 0.11), -0.28, h + 0.25), 'MI_Steel', bevel=0.004)
    b.tube([(w / 2 - 0.2, -0.05, 1.0), (w / 2 - 0.2, -0.08, 1.0)], 0.02, 'MI_Chrome', segs=8)
    b.box((0.15, 0.04, 0.03), (w / 2 - 0.2, -0.1, 1.0), 'MI_Chrome', bevel=0.006)
    return b


# =======================================================================================
# Structure

@prop('SM_Column_Steel', 'Architecture', 'convex')
def column_steel():
    b = Builder('SM_Column_Steel')
    b.box((0.3, 0.3, 0.02), (0, 0, 0.01), 'MI_Metal_Black', bevel=0.004)
    b.cyl(0.1, 3.06, (0, 0, 1.55), 'MI_Metal_Red', segs=20)
    b.box((0.3, 0.3, 0.02), (0, 0, 3.08), 'MI_Metal_Black', bevel=0.004)
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.cyl(0.015, 0.02, (sx * 0.11, sy * 0.11, 0.025), 'MI_Steel', segs=6)
    return b


@prop('SM_IBeam', 'Architecture', 'box')
def ibeam():
    """14.5 m long along X, top at z = 0."""
    b = Builder('SM_IBeam')
    L = 14.5
    b.box((L, 0.2, 0.016), (0, 0, -0.008), 'MI_Metal_Red', bevel=0.002)
    b.box((L, 0.2, 0.016), (0, 0, -0.292), 'MI_Metal_Red', bevel=0.002)
    b.box((L, 0.012, 0.27), (0, 0, -0.15), 'MI_Metal_Red')
    for i in range(15):
        b.box((0.008, 0.19, 0.27), (-L / 2 + 0.5 + i, 0, -0.15), 'MI_Metal_Red')
    return b


@prop('SM_Duct', 'Architecture', 'box')
def duct():
    """4 m galvanised duct along X, top at z = 0."""
    b = Builder('SM_Duct')
    for i in range(4):
        b.box((0.98, 0.4, 0.28), (-1.5 + i, 0, -0.14), 'MI_Steel', bevel=0.004, uv_offset=(i * 0.3, 0))
        b.box((0.04, 0.42, 0.3), (-2 + i, 0, -0.14), 'MI_Steel', bevel=0.004)
    for x in (-1.6, 0.4):
        b.box((0.03, 0.44, 0.02), (x, 0, -0.29), 'MI_Steel')
        for s in (-1, 1):
            b.box((0.02, 0.02, 0.3), (x, s * 0.22, -0.15), 'MI_Steel')
    return b


@prop('SM_FloorDrain', 'Architecture', 'none')
def floor_drain():
    b = Builder('SM_FloorDrain')
    b.cyl(0.13, 0.008, (0, 0, 0.004), 'MI_Steel', segs=24)
    for i in range(7):
        b.box((0.012, 0.2, 0.004), (-0.09 + i * 0.03, 0, 0.0095), 'MI_Metal_Black')
    return b


@prop('SM_Clock_Wall', 'Props/Clutter', 'none')
def clock_wall():
    b = Builder('SM_Clock_Wall')
    b.lathe([(0, 0), (0.17, 0), (0.18, -0.01), (0.18, -0.04), (0.165, -0.05), (0.155, -0.045), (0, -0.045)],
            (0, 0, 0), 'MI_Metal_Black', rot=(-90, 0, 0), segs=32)
    b.plane(0.31, 0.31, (0, -0.0455, 0), 'MI_Img_ClockFace')
    return b


@prop('SM_Pipe_Valve', 'Architecture', 'none')
def pipe_valve():
    b = Builder('SM_Pipe_Valve')
    b.cyl(0.04, 0.12, (0, 0, 0), 'MI_Brass', rot=(0, 90, 0), segs=12)
    b.cyl(0.012, 0.08, (0, 0, 0.06), 'MI_Brass', segs=8)
    b.torus(0.06, 0.008, (0, 0, 0.1), 'MI_Metal_Red', segs=16, rsegs=6)
    for i in range(3):
        a = i * 2 * math.pi / 3
        b.box((0.06, 0.008, 0.008), (math.cos(a) * 0.03, math.sin(a) * 0.03, 0.1), 'MI_Metal_Red', rot=(0, 0, math.degrees(a)))
    return b


# =======================================================================================
# The van ("Brannigan & Sons")

VAN_LENGTH = 5.1
VAN_WIDTH = 2.0


def _van_profile():
    """Side outline in (y, z): rear at y = -2.55, nose at +2.55, with wheel arches."""
    arch = []

    def add_arch(cy, r=0.45, z0=0.375):
        pts = []
        for i in range(13):
            a = math.pi - i * math.pi / 12
            pts.append((cy + math.cos(a) * r, z0 + math.sin(a) * r))
        return pts

    outline = [(-2.55, 0.36)]
    outline += add_arch(-1.55)
    outline += [(-1.05, 0.36), (0.95, 0.36)]
    outline += add_arch(1.45)
    outline += [(1.95, 0.36), (2.55, 0.38), (2.58, 0.75), (2.55, 1.05), (2.1, 1.18), (1.55, 1.95), (1.35, 2.08),
                (-2.5, 2.1), (-2.55, 2.05)]
    return outline


@prop('SM_Van_Body', 'Vehicles', 'convex')
def van_body():
    b = Builder('SM_Van_Body')
    hw = VAN_WIDTH / 2
    b.prism(_van_profile(), VAN_WIDTH - 0.02, (0, 0, 0), 'MI_Van_Paint', axis='X', bevel=0.04)
    # Glass (opaque glossy).
    b.prism([(1.42, 1.92), (2.03, 1.2), (2.08, 1.24), (1.52, 1.98)], VAN_WIDTH - 0.24, (0, 0, 0), 'MI_Glass_Van', axis='X')
    for s in (-1, 1):
        b.prism([(0.9, 1.3), (1.95, 1.25), (1.45, 1.9), (0.9, 1.9)], 0.01, (s * (hw - 0.003), 0, 0), 'MI_Glass_Van',
                axis='X')
        b.box((0.006, 0.02, 0.92), (s * (hw - 0.002), 0.82, 1.18), 'MI_Plastic_Black')
        b.box((0.006, 0.02, 0.92), (s * (hw - 0.002), -0.6, 1.18), 'MI_Plastic_Black')
        b.box((0.006, 0.06, 0.02), (s * (hw + 0.01), 1.1, 1.2), 'MI_Chrome')
        b.box((0.03, 0.12, 0.2), (s * (hw + 0.1), 1.45, 1.6), 'MI_Plastic_Black', bevel=0.01)
        b.tube([(s * (hw - 0.02), 1.5, 1.55), (s * (hw + 0.1), 1.45, 1.55)], 0.008, 'MI_Plastic_Black', segs=6)
    for s in (-1, 1):
        b.box((0.44, 0.01, 0.5), (s * 0.48, -2.556, 1.5), 'MI_Glass_Van')
        b.box((0.03, 0.06, 0.18), (s * (hw - 0.12), -2.565, 0.9), 'MI_Emit_Red', bevel=0.006)
        b.box((0.03, 0.06, 0.07), (s * (hw - 0.12), -2.565, 0.76), 'MI_Plastic_White', bevel=0.004)
    b.box((0.006, 0.02, 1.6), (0, -2.562, 1.2), 'MI_Plastic_Black')
    b.box((0.1, 0.03, 0.03), (0.15, -2.575, 1.15), 'MI_Chrome', bevel=0.006)
    # Front: grille, headlights, bumpers.
    b.box((1.2, 0.04, 0.3), (0, 2.56, 0.85), 'MI_Chrome', bevel=0.01)
    for k in range(5):
        b.box((1.12, 0.03, 0.025), (0, 2.585, 0.74 + k * 0.05), 'MI_Plastic_Black')
    for s in (-1, 1):
        b.cyl(0.09, 0.05, (s * 0.75, 2.57, 0.86), 'MI_Chrome', rot=(90, 0, 0), segs=20)
        b.cyl(0.075, 0.02, (s * 0.75, 2.6, 0.86), 'MI_Glass_Dirty', rot=(90, 0, 0), segs=20)
        b.box((0.12, 0.03, 0.06), (s * 0.75, 2.58, 0.68), 'MI_Emit_Amber', bevel=0.005)
    b.box((VAN_WIDTH + 0.04, 0.14, 0.14), (0, 2.6, 0.45), 'MI_Chrome', bevel=0.03)
    b.box((VAN_WIDTH + 0.04, 0.14, 0.14), (0, -2.6, 0.45), 'MI_Metal_Black', bevel=0.03)
    b.box((0.32, 0.01, 0.16), (0, -2.558, 0.66), 'MI_Paper')
    b.box((0.32, 0.01, 0.16), (0, 2.672, 0.42), 'MI_Paper')
    # Running gear seen under the body.
    b.box((1.4, 4.4, 0.14), (0, 0, 0.38), 'MI_Metal_Black')
    for y in (-1.55, 1.45):
        b.cyl(0.05, VAN_WIDTH - 0.2, (0, y, 0.375), 'MI_Metal_Black', rot=(0, 90, 0), segs=10)
        for s in (-1, 1):
            b.cyl(0.44, 0.24, (s * (hw - 0.12), y, 0.375), 'MI_Metal_Black', rot=(0, 90, 0), segs=20, caps=False)
    b.tube([(0.5, -2.45, 0.33), (0.55, -2.7, 0.3)], 0.03, 'MI_Chrome', segs=10)
    # Roof rack.
    for s in (-1, 1):
        b.tube([(s * 0.85, -2.2, 2.1), (s * 0.85, -2.2, 2.2), (s * 0.85, 1.0, 2.2), (s * 0.85, 1.0, 2.1)], 0.02,
               'MI_Metal_Black', bend=0.05)
    for y in (-1.8, -0.9, 0.0, 0.8):
        b.cyl(0.015, 1.7, (0, y, 2.2), 'MI_Metal_Black', rot=(0, 90, 0), segs=8)
    b.box((0.5, 0.7, 0.18), (0.3, -1.1, 2.31), 'MI_Nylon_Olive', bevel=0.04)
    b.box((0.4, 0.3, 0.12), (-0.4, 0.2, 2.28), 'MI_Cardboard', bevel=0.01)
    return b


@prop('SM_Van_Wheel', 'Vehicles', 'none')
def van_wheel():
    """Axle along X; the outer face points +X."""
    b = Builder('SM_Van_Wheel')
    b.lathe([(0.2, -0.11), (0.33, -0.12), (0.37, -0.09), (0.375, 0.0), (0.37, 0.09), (0.33, 0.12), (0.2, 0.11)],
            mat='MI_Tire', rot=(0, 90, 0), segs=28)
    b.lathe([(0, 0.13), (0.06, 0.13), (0.12, 0.12), (0.2, 0.1), (0.21, 0.05), (0.2, -0.05)], mat='MI_Chrome',
            rot=(0, 90, 0), segs=24)
    for i in range(6):
        a = i * math.pi / 3
        b.cyl(0.012, 0.02, (0.13, math.cos(a) * 0.09, math.sin(a) * 0.09), 'MI_Steel', rot=(0, 90, 0), segs=6)
    return b


@prop('SM_Van_SlidingDoor', 'Vehicles', 'box')
def van_sliding_door():
    """Right-hand (+X) side door; origin at the door's centre on the body side."""
    b = Builder('SM_Van_SlidingDoor')
    w, h = 1.25, 1.62
    b.box((0.03, w + 0.04, h + 0.04), (0.0, 0, 0), 'MI_Plastic_Black')
    b.box((0.03, w, h), (0.015, 0, 0), 'MI_Van_Paint', bevel=0.012)
    b.box((0.01, 0.62, 0.42), (0.033, -0.12, 0.42), 'MI_Glass_Van')
    b.box((0.03, 0.22, 0.035), (0.045, 0.45, 0.05), 'MI_Chrome', bevel=0.008)
    b.box((0.04, w, 0.03), (0.02, 0, -h / 2 + 0.03), 'MI_Steel')
    return b


@prop('SM_Van_Livery', 'Vehicles', 'none')
def van_livery():
    """Livery decal plane, 2.2 x 0.55 m, facing -Y (rotate onto the van side)."""
    b = Builder('SM_Van_Livery')
    b.plane(2.2, 0.55, (0, 0, 0), 'MI_Img_VanLivery')
    return b


# =======================================================================================

def build(name):
    """Returns a Builder for the named prop."""
    random.seed(hash(name) & 0xffff)
    return PROPS[name]['fn']()
