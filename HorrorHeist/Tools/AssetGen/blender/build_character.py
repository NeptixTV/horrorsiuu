"""
Builds and exports the crew body, its animations and all cosmetics.

  python Tools/AssetGen/blender/build_character.py            export everything
  python Tools/AssetGen/blender/build_character.py --preview  also render QA sheets to SourceArt/Previews

Outputs
  SourceArt/Meshes/Character/SK_HH_Body.fbx          skeletal mesh (+ skeleton)
  SourceArt/Meshes/Character/Anims/A_HH_*.fbx        animations (armature only)
  SourceArt/Meshes/Cosmetics/*.fbx                   skinned and attached cosmetics
  SourceArt/Meshes/meshes.json                        (adds entries)
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

import character as C  # noqa: E402
import hhblend as H  # noqa: E402

OUT = os.path.join(H.PROJECT, 'SourceArt', 'Meshes')
PREVIEW = os.path.join(H.PROJECT, 'SourceArt', 'Previews')

FBX_SKEL = dict(H.FBX_COMMON, primary_bone_axis='Y', secondary_bone_axis='X', armature_nodetype='NULL',
                use_armature_deform_only=True)


def export_skeletal(objs, arm, path, with_anim=False):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    H.deselect_all()
    for o in objs:
        o.select_set(True)
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    types = {'ARMATURE', 'MESH'} if objs else {'ARMATURE'}
    kw = dict(FBX_SKEL)
    if with_anim:
        kw.update(bake_anim=True, bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
                  bake_anim_force_startend_keying=True, bake_anim_step=1.0, bake_anim_simplify_factor=0.0,
                  bake_anim_use_all_bones=True)
    else:
        kw.update(bake_anim=False)
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types=types, **kw)


def export_animations(arm, actions, manifest):
    scene = bpy.context.scene
    scene.render.fps = C.FPS
    for name, action in actions.items():
        arm.animation_data.action = action
        if hasattr(arm.animation_data, 'action_slot') and action.slots:
            arm.animation_data.action_slot = action.slots[0]
        f0, f1 = C.action_frames(action)
        scene.frame_start, scene.frame_end = f0, f1
        rel = f'SourceArt/Meshes/Character/Anims/{action.name}.fbx'
        export_skeletal([], arm, os.path.join(H.PROJECT, rel), with_anim=True)
        manifest.add(action.name, rel, [], 'Character/Anims', 'none', False, kind='anim',
                     extra=dict(skeleton='SK_HH_Body', frames=f1 - f0, fps=C.FPS))
        print('anim', action.name, f1 - f0, 'frames', flush=True)
    arm.animation_data.action = None


# =======================================================================================
# Preview

def preview_setup(scene):
    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'CPU'
    scene.cycles.samples = 24
    scene.cycles.use_denoising = True
    scene.render.resolution_x = 360
    scene.render.resolution_y = 540
    world = bpy.data.worlds.new('W')
    world.color = (0.25, 0.25, 0.27)
    scene.world = world
    sun = bpy.data.lights.new('Sun', 'SUN')
    sun.energy = 3.5
    so = H.link(bpy.data.objects.new('Sun', sun))
    so.rotation_euler = (math.radians(50), math.radians(10), math.radians(-35))
    cam = H.link(bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')))
    cam.data.lens = 85
    cam.data.clip_start = 10
    cam.data.clip_end = 5000
    scene.camera = cam
    try:
        scene.view_settings.view_transform = 'AgX'
    except TypeError:
        pass
    return cam


def aim(cam, pos, target):
    cam.location = pos
    cam.rotation_euler = (Vector(target) - Vector(pos)).to_track_quat('-Z', 'Y').to_euler()


def render(scene, path):
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


def preview_sheet(paths, out, cols):
    from PIL import Image
    ims = [Image.open(p).convert('RGB') for p in paths]
    w, h = ims[0].size
    rows = (len(ims) + cols - 1) // cols
    sheet = Image.new('RGB', (w * cols, h * rows), (20, 20, 20))
    for i, im in enumerate(ims):
        sheet.paste(im, ((i % cols) * w, (i // cols) * h))
    sheet.save(out, quality=85)


def preview_body(arm, actions, extra_objs=()):
    scene = bpy.context.scene
    cam = preview_setup(scene)
    tmp = os.path.join(PREVIEW, '_char')
    os.makedirs(tmp, exist_ok=True)
    paths = []
    arm.animation_data.action = None
    for pb in arm.pose.bones:
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.location = (0, 0, 0)
    for i, (pos, tgt) in enumerate((((0, -520, 100), (0, 0, 92)), ((520, -60, 100), (0, 0, 92)),
                                    ((240, -380, 150), (0, 0, 95)), ((0, -150, 168), (0, 0, 166)))):
        aim(cam, pos, tgt)
        path = os.path.join(tmp, f'rest_{i}.png')
        render(scene, path)
        paths.append(path)
    preview_sheet(paths, os.path.join(PREVIEW, 'Character_Rest.jpg'), 4)
    paths = []
    aim(cam, (420, -330, 110), (0, 0, 80))
    for name, action in actions.items():
        arm.animation_data.action = action
        if hasattr(arm.animation_data, 'action_slot') and action.slots:
            arm.animation_data.action_slot = action.slots[0]
        f0, f1 = C.action_frames(action)
        for k in range(4):
            scene.frame_set(int(f0 + (f1 - f0) * k / 4))
            path = os.path.join(tmp, f'{name}_{k}.png')
            render(scene, path)
            paths.append(path)
    preview_sheet(paths, os.path.join(PREVIEW, 'Character_Anims.jpg'), 8)
    arm.animation_data.action = None


def main():
    H.reset(unit_scale=0.01)
    bpy.context.scene.render.fps = C.FPS
    arm = C.build_armature()
    body = C.build_body_mesh()
    C.skin(body, arm)
    print('body', len(body.data.vertices), 'verts', len(body.data.polygons), 'faces', flush=True)
    actions = C.build_animations(arm)
    manifest = H.MeshManifest(os.path.join(OUT, 'meshes.json'))
    if '--no-export' not in sys.argv:
        rel = 'SourceArt/Meshes/Character/SK_HH_Body.fbx'
        arm.animation_data.action = None
        export_skeletal([body], arm, os.path.join(H.PROJECT, rel))
        manifest.add('SK_HH_Body', rel, [m.name for m in body.data.materials], 'Character', 'none', False,
                     kind='skeletal')
        export_animations(arm, actions, manifest)
        import cosmetics
        cosmetics.build_all(body, arm, manifest, preview='--preview' in sys.argv)
        manifest.save()
    if '--preview' in sys.argv:
        preview_body(arm, actions)


if __name__ == '__main__':
    main()
