"""
QA: renders every prop (or the ones named on the command line) into a contact sheet.
  python Tools/AssetGen/blender/preview_props.py [SM_Name ...] --out sheet.jpg
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

import hhblend as H  # noqa: E402
import props as P  # noqa: E402


def setup_render(scene, res=256, samples=24):
    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'CPU'
    scene.cycles.samples = samples
    scene.cycles.use_denoising = True
    scene.render.resolution_x = res
    scene.render.resolution_y = res
    scene.render.film_transparent = False
    world = bpy.data.worlds.new('W')
    world.color = (0.18, 0.18, 0.2)
    scene.world = world
    try:
        scene.view_settings.view_transform = 'AgX'
    except TypeError:
        pass


def frame(obj, cam):
    corners = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    lo = Vector((min(c.x for c in corners), min(c.y for c in corners), min(c.z for c in corners)))
    hi = Vector((max(c.x for c in corners), max(c.y for c in corners), max(c.z for c in corners)))
    centre = (lo + hi) / 2
    radius = max((hi - lo).length / 2, 0.05)
    direction = Vector((0.75, -1.0, 0.55)).normalized()
    cam.location = centre + direction * radius * 2.9
    cam.rotation_euler = (centre - cam.location).to_track_quat('-Z', 'Y').to_euler()
    cam.data.lens = 50
    cam.data.clip_start = radius * 0.05
    cam.data.clip_end = radius * 20


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    out = 'prop_sheet.jpg'
    if '--out' in sys.argv:
        out = sys.argv[sys.argv.index('--out') + 1]
        args = [a for a in args if a != out]
    names = args or sorted(P.PROPS)
    scene = H.reset()
    setup_render(scene)
    cam = H.link(bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')))
    scene.camera = cam
    for loc, power, size in (((3, -4, 5), 900, 3), ((-5, -2, 2), 300, 4), ((0, 5, 4), 500, 2)):
        light = bpy.data.lights.new('L', 'AREA')
        light.energy = power
        light.size = size
        lo = H.link(bpy.data.objects.new('L', light))
        lo.location = loc
        lo.rotation_euler = (Vector((0, 0, 0.5)) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    tmp = os.path.join(os.path.dirname(os.path.abspath(out)), '_props')
    os.makedirs(tmp, exist_ok=True)
    rendered = []
    for name in names:
        obj = H.link(P.build(name).object())
        lights = [o for o in scene.objects if o.type == 'LIGHT']
        frame(obj, cam)
        scale = max(obj.dimensions) / 1.5
        for lo in lights:
            lo.data.energy = lo.data.energy  # keep
        scene.render.filepath = os.path.join(tmp, name + '.png')
        bpy.ops.render.render(write_still=True)
        rendered.append((name, scene.render.filepath))
        bpy.data.objects.remove(obj)
        print('rendered', name, round(scale, 2), flush=True)

    from PIL import Image, ImageDraw
    cols = 8
    cell = 256
    rows = (len(rendered) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * cell, rows * (cell + 16)), (20, 20, 20))
    draw = ImageDraw.Draw(sheet)
    for i, (name, path) in enumerate(rendered):
        x = (i % cols) * cell
        y = (i // cols) * (cell + 16)
        sheet.paste(Image.open(path).convert('RGB'), (x, y))
        draw.text((x + 4, y + cell + 2), name.replace('SM_', ''), fill=(230, 230, 230))
    sheet.save(out, quality=85)
    print('sheet', out)


if __name__ == '__main__':
    main()
