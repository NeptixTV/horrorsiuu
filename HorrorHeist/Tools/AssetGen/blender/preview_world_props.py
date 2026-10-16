"""
QA for props_world.py: contact sheets plus bounding-box / pivot / material / triangle checks.

  python3 Tools/AssetGen/blender/preview_world_props.py --check [--group loot|house|town|nature|yard|all]
  python3 Tools/AssetGen/blender/preview_world_props.py --group loot --out /tmp/loot.jpg
  python3 Tools/AssetGen/blender/preview_world_props.py SM_Sofa_3Seat SM_Fridge --out /tmp/sheet.jpg [--front]
      [--res 256] [--cols 6] [--samples 20]

--check prints every prop's folder, collision, measured size (W x D x H) against the requested
size, its triangle count and pivot problems (floor props: min z = 0; wall props: max y = 0;
ceiling props: max z = 0; fence segments / gates: start at x = 0), and verifies that every
material slot is a catalogue entry. Exit code 1 if anything fails.

Catalogue images that are not generated yet get a labelled stand-in texture (name + "TOP" arrow)
so previews still render and image orientation can be checked.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

import hhblend as H  # noqa: E402
import preview_props as PP  # noqa: E402
import props as P  # noqa: E402
import props_world as W  # noqa: E402
import props_town2  # noqa: E402,F401
from hhmaterials import CATALOG  # noqa: E402

TOL = 0.01


def _arg(flag, default=None):
    if flag in sys.argv:
        return sys.argv[sys.argv.index(flag) + 1]
    return default


def placeholder_materials(tmp):
    """Flat stand-ins for image materials whose texture does not exist yet (the image shows its
    name, 'TOP' at the top edge and 'L' bottom-left; the street atlas gets its 4 rows)."""
    from PIL import Image, ImageDraw
    os.makedirs(tmp, exist_ok=True)
    made = []
    for name, spec in CATALOG.items():
        if spec['master'] not in ('Image', 'ImageMask', 'Decal', 'Screen'):
            continue
        if H._tex_path(spec['image']) is not None:
            continue
        path = os.path.join(tmp, spec['image'] + '.png')
        if not os.path.exists(path):
            hue = (hash(spec['image']) & 0xff)
            if spec['image'] == 'T_Sign_StreetNames':
                img = Image.new('RGB', (1024, 512), (20, 90, 40))
                dr = ImageDraw.Draw(img)
                for k, txt in enumerate(('MAIN ST', 'ASHGROVE LN', 'MARROW ST', 'CHURCH RD')):
                    dr.rectangle((4, k * 128 + 4, 1019, k * 128 + 123), outline=(240, 240, 240), width=6)
                    dr.text((40, k * 128 + 40), txt, fill=(255, 255, 255), font_size=56)
            else:
                img = Image.new('RGB', (512, 512), (60 + hue % 120, 70 + (hue * 7) % 100, 90 + (hue * 13) % 110))
                dr = ImageDraw.Draw(img)
                dr.rectangle((6, 6, 505, 505), outline=(250, 250, 250), width=10)
                dr.text((150, 20), 'TOP', fill=(255, 255, 0), font_size=60)
                dr.text((20, 420), 'L', fill=(255, 255, 0), font_size=70)
                dr.text((30, 230), spec['image'].replace('T_', ''), fill=(255, 255, 255), font_size=40)
            img.save(path)
        mat = bpy.data.materials.new(name)
        mat.use_nodes = True
        nt = mat.node_tree
        bsdf = nt.nodes.get('Principled BSDF')
        tex = nt.nodes.new('ShaderNodeTexImage')
        tex.image = bpy.data.images.load(path, check_existing=True)
        nt.links.new(tex.outputs['Color'], bsdf.inputs['Base Color'])
        bsdf.inputs['Roughness'].default_value = 0.6
        made.append(name)
    return made


def measure(obj):
    me = obj.data
    vs = [v.co for v in me.vertices]
    lo = Vector((min(v.x for v in vs), min(v.y for v in vs), min(v.z for v in vs)))
    hi = Vector((max(v.x for v in vs), max(v.y for v in vs), max(v.z for v in vs)))
    tris = sum(len(p.vertices) - 2 for p in me.polygons)
    return lo, hi, tris


def tri_budget(name, size):
    folder = P.PROPS[name]['folder']
    if 'Car' in name or 'Tree' in name:
        return 20000
    if size and max(size) >= 0.7:
        return 8000
    if 'Loot' in folder:
        return 3000 if (size is None or max(size) < 0.3) else 5000
    return 3000 if (size and max(size) < 0.4) else 8000


def check(names):
    bad = 0
    rows = []
    for name in names:
        meta = W.WORLD[name]
        b = P.build(name)
        missing = [m for m in b.mats if m not in CATALOG]
        obj = H.link(b.object())
        lo, hi, tris = measure(obj)
        dims = hi - lo
        kind = meta['kind']
        flags = []
        if missing:
            flags.append('MAT?' + ','.join(missing))
        if kind in ('floor', 'fence', 'wall') and abs(lo.z) > TOL:
            flags.append(f'minz={lo.z:+.3f}')
        if kind == 'gate' and not -TOL <= lo.z <= 0.08:      # gate leaves hang a little above the ground
            flags.append(f'minz={lo.z:+.3f}')
        if kind == 'floor':
            cx, cy = (lo.x + hi.x) / 2, (lo.y + hi.y) / 2
            if abs(cx) > max(0.02, 0.06 * dims.x) or abs(cy) > max(0.02, 0.06 * dims.y):
                flags.append(f'off-centre({cx:+.3f},{cy:+.3f})')
        if kind == 'wall' and abs(hi.y) > TOL:
            flags.append(f'maxy={hi.y:+.3f}')
        if kind == 'ceiling' and abs(hi.z) > TOL:
            flags.append(f'maxz={hi.z:+.3f}')
        if kind in ('fence', 'gate') and abs(lo.x) > 0.06:
            flags.append(f'minx={lo.x:+.3f}')
        if kind == 'fence' and abs((lo.y + hi.y) / 2) > 0.03:
            flags.append(f'cy={(lo.y + hi.y) / 2:+.3f}')
        size = meta['size']
        dev = ''
        if size:
            ds = []
            for got, want in zip(dims, size):
                if want and abs(got - want) > max(0.02, 0.15 * want):
                    ds.append(f'{got / want:.2f}x')
                else:
                    ds.append('ok')
            if any(d != 'ok' for d in ds):
                dev = 'DEV[' + ' '.join(ds) + ']'
        budget = tri_budget(name, size)
        if tris > budget:
            flags.append(f'TRIS>{budget}')
        hard = [f for f in flags if not f.startswith('off-centre') and not f.startswith('TRIS')]
        bad += bool(hard)
        rows.append((name, P.PROPS[name]['folder'], P.PROPS[name]['collision'], dims, size, tris, flags, dev))
        bpy.data.objects.remove(obj)
    for name, folder, col, dims, size, tris, flags, dev in rows:
        want = ' x '.join(f'{s:.3g}' for s in size) if size else '-'
        print(f'{name:28s} {folder:12s} {col:8s} {dims.x:6.3f} x {dims.y:6.3f} x {dims.z:6.3f}  '
              f'(spec {want:20s}) {tris:6d} tris  {" ".join(flags)} {dev}')
    print(f'{len(rows)} props checked, {bad} with errors')
    return bad


def add_lights(scene, centre, radius):
    for o in [o for o in scene.objects if o.type == 'LIGHT']:
        bpy.data.objects.remove(o)
    for off, power, size in (((0.9, -1.2, 1.4), 900, 1.2), ((-1.6, -0.6, 0.6), 300, 1.6), ((0, 1.6, 1.2), 500, 0.8)):
        light = bpy.data.lights.new('L', 'AREA')
        k = max(radius, 0.05) / 1.0
        light.energy = power * k * k * 0.45
        light.size = size * k * 2.0
        lo = H.link(bpy.data.objects.new('L', light))
        lo.location = centre + Vector(off) * k * 3.2
        lo.rotation_euler = (centre - lo.location).to_track_quat('-Z', 'Y').to_euler()


def render(names, out, front=False):
    from PIL import Image, ImageDraw
    scene = bpy.context.scene
    res = int(_arg('--res', 256))
    PP.setup_render(scene, res=res, samples=int(_arg('--samples', 20)))
    cam = H.link(bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')))
    scene.camera = cam
    tmp = os.path.join(os.path.dirname(os.path.abspath(out)), '_world_props')
    os.makedirs(tmp, exist_ok=True)
    rendered = []
    for name in names:
        kind = W.WORLD[name]['kind']
        obj = H.link(P.build(name).object())
        lo, hi, tris = measure(obj)
        centre = (lo + hi) / 2
        radius = max((hi - lo).length / 2, 0.03)
        helpers = []
        if kind in ('floor', 'fence', 'gate'):
            bpy.ops.mesh.primitive_plane_add(size=radius * 12, location=(centre.x, centre.y, 0))
            helpers.append(bpy.context.active_object)
        elif kind == 'wall':
            bpy.ops.mesh.primitive_plane_add(size=radius * 12, location=(centre.x, 0.0005, centre.z),
                                             rotation=(math.pi / 2, 0, 0))
            helpers.append(bpy.context.active_object)
        for hobj in helpers:
            m = bpy.data.materials.get('QA_Ground')
            if m is None:
                m = bpy.data.materials.new('QA_Ground')
                m.use_nodes = True
                bsdf = m.node_tree.nodes.get('Principled BSDF')
                bsdf.inputs['Base Color'].default_value = (0.11, 0.11, 0.115, 1)
                bsdf.inputs['Roughness'].default_value = 0.8
            hobj.data.materials.append(m)
        d = Vector((0.0, -1.0, 0.25)) if front else Vector((0.75, -1.0, 0.55))
        if kind == 'ceiling':
            d = Vector((0.75, -1.0, -0.45))
        d.normalize()
        cam.location = centre + d * radius * 2.9
        cam.rotation_euler = (centre - cam.location).to_track_quat('-Z', 'Y').to_euler()
        cam.data.lens = 50
        cam.data.clip_start = radius * 0.02
        cam.data.clip_end = radius * 40
        add_lights(scene, centre, radius)
        scene.render.filepath = os.path.join(tmp, name + '.png')
        bpy.ops.render.render(write_still=True)
        dims = hi - lo
        rendered.append((name, scene.render.filepath, f'{dims.x:.2f}x{dims.y:.2f}x{dims.z:.2f} {tris}t'))
        bpy.data.objects.remove(obj)
        for hobj in helpers:
            bpy.data.objects.remove(hobj)
        print('rendered', name, flush=True)
    cols = int(_arg('--cols', 6))
    cell = res
    rows = (len(rendered) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * cell, rows * (cell + 30)), (20, 20, 20))
    draw = ImageDraw.Draw(sheet)
    for i, (name, path, info) in enumerate(rendered):
        x = (i % cols) * cell
        y = (i // cols) * (cell + 30)
        sheet.paste(Image.open(path).convert('RGB'), (x, y))
        draw.text((x + 4, y + cell + 1), name.replace('SM_', ''), fill=(235, 235, 235), font_size=13)
        draw.text((x + 4, y + cell + 15), info, fill=(170, 170, 170), font_size=11)
    sheet.save(out, quality=88)
    print('sheet', out)


def main():
    args = [a for a in sys.argv[1:] if a.startswith('SM_')]
    group = _arg('--group')
    names = args or W.names(group or 'all')
    out = _arg('--out', 'world_props.jpg')
    H.reset()
    placeholder_materials(os.path.join(os.path.dirname(os.path.abspath(out)), '_placeholders'))
    if '--check' in sys.argv:
        sys.exit(1 if check(names) else 0)
    render(names, out, front='--front' in sys.argv)


if __name__ == '__main__':
    main()
