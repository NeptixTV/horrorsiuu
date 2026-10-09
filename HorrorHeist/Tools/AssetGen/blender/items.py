"""
Equipment meshes (metres, lying as they would on a workbench, long axis along X) + their icons.
Registered in the prop library so the hideout can lay them out on the bench.

  python Tools/AssetGen/blender/items.py      export meshes + render icons
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402,F401  (registers bmesh/mathutils)
from hhblend import Builder  # noqa: E402
from props import prop  # noqa: E402


@prop('SM_EQ_Torch_Pocket', 'Equipment', 'convex')
def torch_pocket():
    b = Builder('SM_EQ_Torch_Pocket')
    b.lathe([(0, 0), (0.014, 0.0), (0.015, 0.004), (0.015, 0.09), (0.019, 0.105), (0.02, 0.135), (0.0185, 0.137),
             (0, 0.137)], (0, 0, 0.02), 'MI_Aluminium', rot=(0, 90, 0), segs=20)
    b.cyl(0.016, 0.003, (0.1385, 0, 0.02), 'MI_Glass', rot=(0, 90, 0), segs=20)
    b.cyl(0.005, 0.012, (0.07, 0, 0.0355), 'MI_Rubber_Black', segs=10)
    for k in range(6):
        b.torus(0.0152, 0.0012, (0.02 + k * 0.009, 0, 0.02), 'MI_Aluminium', rot=(0, 90, 0), segs=20, rsegs=4)
    b.tube([(-0.005, 0, 0.02), (-0.03, 0, 0.02), (-0.045, 0.01, 0.01)], 0.002, 'MI_Cable', segs=5, bend=0.01)
    return b


@prop('SM_EQ_Torch_Patrol', 'Equipment', 'convex')
def torch_patrol():
    b = Builder('SM_EQ_Torch_Patrol')
    b.lathe([(0, 0), (0.019, 0.0), (0.02, 0.006), (0.02, 0.27), (0.03, 0.31), (0.031, 0.35), (0.028, 0.352),
             (0, 0.352)], (0, 0, 0.031), 'MI_Metal_Black', rot=(0, 90, 0), segs=24)
    b.cyl(0.026, 0.003, (0.353, 0, 0.031), 'MI_Glass', rot=(0, 90, 0), segs=24)
    for k in range(14):
        b.torus(0.0203, 0.0014, (0.04 + k * 0.012, 0, 0.031), 'MI_Metal_Black', rot=(0, 90, 0), segs=20, rsegs=4)
    b.box((0.022, 0.012, 0.008), (0.27, 0, 0.052), 'MI_Rubber_Black', bevel=0.003)
    return b


@prop('SM_EQ_Headlamp', 'Equipment', 'none')
def headlamp():
    b = Builder('SM_EQ_Headlamp')
    b.torus(0.085, 0.006, (0, 0, 0.006), 'MI_Nylon_Black', scale=(1, 0.8, 1.6), segs=40, rsegs=6)
    b.box((0.05, 0.03, 0.035), (0, -0.07, 0.022), 'MI_Plastic_Black', bevel=0.008)
    b.cyl(0.013, 0.006, (0, -0.087, 0.024), 'MI_Glass', rot=(90, 0, 0), segs=16)
    b.torus(0.0135, 0.002, (0, -0.088, 0.024), 'MI_Plastic_Red', rot=(90, 0, 0), segs=16, rsegs=4)
    b.box((0.04, 0.025, 0.03), (0, 0.068, 0.02), 'MI_Plastic_Black', bevel=0.006)
    return b


@prop('SM_EQ_Lantern', 'Equipment', 'convex')
def lantern():
    """Storm lantern standing upright; the flame sits at (0, 0, 0.14)."""
    b = Builder('SM_EQ_Lantern')
    b.lathe([(0, 0), (0.07, 0), (0.075, 0.01), (0.075, 0.055), (0.06, 0.07), (0.035, 0.075), (0, 0.075)],
            mat='MI_Metal_Red', segs=28)
    b.lathe([(0.03, 0.075), (0.05, 0.1), (0.055, 0.15), (0.045, 0.2), (0.025, 0.215), (0.02, 0.215), (0.04, 0.2),
             (0.05, 0.15), (0.045, 0.1), (0.025, 0.08)], mat='MI_Glass', segs=24, closed=True)
    b.sphere(0.008, (0, 0, 0.13), 'MI_Emit_Bulb', scale=(1, 1, 1.8), segs=10, rings=6)
    b.cyl(0.004, 0.04, (0, 0, 0.1), 'MI_Paper', segs=6)
    for i in range(4):
        a = i * math.pi / 2 + math.pi / 4
        b.tube([(math.cos(a) * 0.06, math.sin(a) * 0.06, 0.07), (math.cos(a) * 0.068, math.sin(a) * 0.068, 0.15),
                (math.cos(a) * 0.045, math.sin(a) * 0.045, 0.22)], 0.0025, 'MI_Metal_Red', segs=5, bend=0.03)
    b.lathe([(0, 0.215), (0.05, 0.215), (0.052, 0.225), (0.03, 0.245), (0.012, 0.25), (0, 0.25)], mat='MI_Metal_Red',
            segs=24)
    b.torus(0.06, 0.003, (0, 0, 0.25), 'MI_Steel', rot=(90, 0, 0), segs=20, rsegs=5, arc=180)
    b.cyl(0.01, 0.012, (0.07, 0, 0.04), 'MI_Brass', rot=(0, 90, 0), segs=10)
    return b


@prop('SM_EQ_Lockpicks', 'Equipment', 'none')
def lockpicks():
    b = Builder('SM_EQ_Lockpicks')
    b.box((0.2, 0.11, 0.008), (0, 0, 0.004), 'MI_Leather', bevel=0.003)
    b.box((0.2, 0.05, 0.004), (0, 0.035, 0.010), 'MI_Leather', rot=(-6, 0, 0), bevel=0.002)
    for i in range(7):
        x = -0.075 + i * 0.025
        b.box((0.006, 0.09, 0.002), (x, -0.005, 0.012), 'MI_Chrome', rot=(0, 0, 4 - i), bevel=0.0005)
        b.cyl(0.004, 0.012, (x + 0.0, 0.04, 0.012), 'MI_Plastic_Black', rot=(90, 0, 0), segs=6)
    b.tube([(-0.1, 0.0, 0.006), (-0.13, -0.03, 0.004), (-0.16, 0.02, 0.003)], 0.002, 'MI_Leather', segs=4, bend=0.02)
    return b


@prop('SM_EQ_Crowbar', 'Equipment', 'convex')
def crowbar():
    b = Builder('SM_EQ_Crowbar')
    b.tube([(-0.36, 0, 0.012), (0.3, 0, 0.012), (0.36, 0, 0.03), (0.38, 0, 0.07), (0.35, 0, 0.09)], 0.011,
           'MI_Metal_Blue', segs=8, bend=0.05)
    b.prism([(-0.015, -0.006), (0.04, -0.004), (0.045, 0.004), (-0.015, 0.006)], 0.024, (-0.39, 0, 0.012),
            'MI_Steel', axis='Y')
    b.box((0.012, 0.006, 0.03), (0.35, 0, 0.1), 'MI_Steel', rot=(0, 40, 0))
    return b


@prop('SM_EQ_GlassCutter', 'Equipment', 'none')
def glass_cutter():
    b = Builder('SM_EQ_GlassCutter')
    b.lathe([(0, 0), (0.045, 0.0), (0.05, 0.004), (0.035, 0.018), (0.012, 0.025), (0, 0.025)], mat='MI_Rubber_Black',
            segs=24)
    b.cyl(0.008, 0.05, (0, 0, 0.05), 'MI_Steel', segs=10)
    b.box((0.13, 0.012, 0.01), (0.065, 0, 0.07), 'MI_Aluminium', bevel=0.003)
    b.cyl(0.006, 0.004, (0.125, 0, 0.06), 'MI_Steel', rot=(90, 0, 0), segs=12)
    b.cyl(0.012, 0.06, (-0.02, 0, 0.09), 'MI_Plastic_Red', rot=(0, 90, 0), segs=10)
    return b


@prop('SM_EQ_Walkie', 'Equipment', 'none')
def walkie_eq():
    b = Builder('SM_EQ_Walkie')
    b.box((0.17, 0.065, 0.04), (0, 0, 0.02), 'MI_Plastic_Black', bevel=0.008)
    b.cyl(0.006, 0.13, (0.15, 0.018, 0.022), 'MI_Rubber_Black', rot=(0, 90, 0), segs=8)
    for k in range(6):
        b.box((0.004, 0.04, 0.002), (-0.05 + k * 0.009, 0, 0.0405), 'MI_Plastic_Grey')
    b.box((0.04, 0.03, 0.002), (0.03, 0, 0.0405), 'MI_Emit_Green', bevel=0.001)
    b.cyl(0.007, 0.012, (0.09, -0.018, 0.022), 'MI_Plastic_Grey', rot=(0, 90, 0), segs=10)
    b.box((0.03, 0.006, 0.012), (-0.02, -0.035, 0.02), 'MI_Rubber_Black', bevel=0.002)
    return b


@prop('SM_EQ_SaltPouch', 'Equipment', 'none')
def salt_pouch():
    b = Builder('SM_EQ_SaltPouch')
    b.lathe([(0, 0), (0.035, 0.0), (0.055, 0.02), (0.058, 0.05), (0.045, 0.075), (0.02, 0.09), (0.016, 0.095),
             (0.026, 0.115), (0.0, 0.11)], mat='MI_Fabric_Shade', segs=20, scale=(1.0, 0.85, 1.0))
    b.torus(0.018, 0.003, (0, 0, 0.093), 'MI_Cable', segs=16, rsegs=4)
    b.tube([(0.015, 0, 0.093), (0.04, -0.02, 0.07), (0.05, -0.04, 0.02)], 0.0018, 'MI_Cable', segs=4, bend=0.02)
    b.grid_plane(0.12, 0.05, (0.09, 0.0, 0.001), 'MI_Plastic_White', sag=0.002, nx=6, ny=3)
    return b


@prop('SM_EQ_Camera', 'Equipment', 'box')
def instant_camera():
    b = Builder('SM_EQ_Camera')
    b.prism([(-0.065, 0), (0.065, 0), (0.065, 0.1), (-0.065, 0.1)], 0.11, (0, 0, 0), 'MI_Plastic_Black', axis='Y',
            bevel=0.01)
    b.box((0.13, 0.05, 0.03), (0, -0.06, 0.03), 'MI_Plastic_Black', bevel=0.008)
    b.cyl(0.028, 0.02, (0.0, -0.06, 0.065), 'MI_Plastic_Grey', rot=(90, 0, 0), segs=24)
    b.cyl(0.02, 0.004, (0.0, -0.072, 0.065), 'MI_Glass', rot=(90, 0, 0), segs=24)
    b.box((0.04, 0.004, 0.016), (0.035, -0.0565, 0.09), 'MI_Plastic_White', bevel=0.002)
    for k, c in enumerate(('MI_Plastic_Red', 'MI_Plastic_Yellow', 'MI_Metal_Green', 'MI_Metal_Blue')):
        b.box((0.13, 0.002, 0.004), (0, -0.0855 + 0.0, 0.01 + k * 0.006), c)
    b.plane(0.08, 0.08, (0, -0.075, 0.012), 'MI_Img_Polaroids', rot=(-75, 0, 0), uv_rect=(0.25, 0.5, 0.5, 1.0))
    b.cyl(0.006, 0.006, (-0.05, 0.0, 0.103), 'MI_Plastic_Red', segs=10)
    return b


@prop('SM_EQ_NoiseMeter', 'Equipment', 'none')
def noise_meter():
    b = Builder('SM_EQ_NoiseMeter')
    b.box((0.08, 0.035, 0.16), (0, 0, 0.08), 'MI_Plastic_Yellow', bevel=0.01)
    b.box((0.06, 0.004, 0.05), (0, -0.0185, 0.12), 'MI_Paper')
    b.box((0.002, 0.002, 0.04), (0.008, -0.021, 0.115), 'MI_Plastic_Red', rot=(0, -25, 0))
    b.cyl(0.008, 0.006, (0, -0.02, 0.06), 'MI_Plastic_Black', rot=(90, 0, 0), segs=12)
    b.cyl(0.01, 0.04, (0, 0, 0.18), 'MI_Plastic_Black', segs=10)
    b.sphere(0.02, (0, 0, 0.215), 'MI_Fabric_Grey', segs=14, rings=8)
    b.box((0.012, 0.004, 0.006), (-0.022, -0.0185, 0.04), 'MI_Emit_Red')
    return b


@prop('SM_EQ_Duffel', 'Equipment', 'convex')
def duffel_eq():
    b = Builder('SM_EQ_Duffel')
    b.sphere(0.22, (0, 0, 0.18), 'MI_Nylon_Black', scale=(2.4, 1.0, 0.85), segs=24, rings=14)
    for s in (-1, 1):
        b.torus(0.12, 0.012, (s * 0.12, 0, 0.36), 'MI_Nylon_Black', rot=(90, 0, 90), segs=14, rsegs=6, arc=180)
        b.cyl(0.18, 0.012, (s * 0.52, 0, 0.18), 'MI_Nylon_Olive', rot=(0, 90, 0), segs=20)
    b.tube([(-0.4, -0.16, 0.25), (0.4, -0.16, 0.25)], 0.006, 'MI_Chrome', segs=6)
    b.box((0.03, 0.01, 0.04), (0.3, -0.175, 0.24), 'MI_Chrome', bevel=0.003)
    return b


@prop('SM_EQ_Sack', 'Equipment', 'convex')
def sack():
    b = Builder('SM_EQ_Sack')
    b.lathe([(0, 0), (0.18, 0.0), (0.24, 0.05), (0.25, 0.18), (0.21, 0.32), (0.1, 0.42), (0.05, 0.45), (0.065, 0.5),
             (0.03, 0.53), (0.0, 0.52)], mat='MI_Fabric_Brown', segs=24, scale=(1.0, 0.8, 1.0))
    b.torus(0.05, 0.008, (0, 0, 0.45), 'MI_Cable', segs=16, rsegs=5, scale=(1, 0.8, 1))
    return b


EQUIPMENT_MESHES = ['SM_EQ_Torch_Pocket', 'SM_EQ_Torch_Patrol', 'SM_EQ_Headlamp', 'SM_EQ_Lantern', 'SM_EQ_Lockpicks',
                    'SM_EQ_Crowbar', 'SM_EQ_GlassCutter', 'SM_EQ_Walkie', 'SM_EQ_SaltPouch', 'SM_EQ_Camera',
                    'SM_EQ_NoiseMeter', 'SM_EQ_Duffel', 'SM_EQ_Sack']


def main():
    import bpy
    import catalog
    import cosmetics as CO
    import hhblend as H
    import props as P
    H.reset()
    manifest = H.MeshManifest(os.path.join(H.MESH_DIR, 'meshes.json'))
    objs = {}
    for name in EQUIPMENT_MESHES:
        obj = H.link(P.build(name).object())
        objs[name] = obj
        rel = f'SourceArt/Meshes/Equipment/{name}.fbx'
        H.export_mesh_object(obj, os.path.join(H.PROJECT, rel))
        manifest.add(name, rel, [m.name for m in obj.data.materials], 'Equipment', P.PROPS[name]['collision'], False)
        print('exported', name, flush=True)
    manifest.save()
    scene, cam = CO.icon_scene()
    for o in objs.values():
        o.hide_render = True
    os.makedirs(CO.ICON_DIR, exist_ok=True)
    for item in catalog.EQUIPMENT:
        obj = objs[item['mesh']]
        obj.hide_render = False
        CO.frame_objects(cam, [obj], direction=(0.55, -1.0, 0.75), margin=1.08)
        scene.render.filepath = os.path.join(CO.ICON_DIR, f"T_Icon_{item['id']}.png")
        bpy.ops.render.render(write_still=True)
        obj.hide_render = True
        print('icon', item['id'], flush=True)


if __name__ == '__main__':
    main()
