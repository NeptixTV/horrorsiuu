"""
Inventory icons for the loot meshes in props_world.py, in the same icon scene / framing / size as
the equipment icons (items.py): 256 x 256 transparent PNGs. Does not export meshes or touch
meshes.json.

  python3 Tools/AssetGen/blender/render_loot_icons.py [SM_Loot_TV ...]
      -> SourceArt/Textures/Icons/T_Icon_LT_<Suffix>.png  (SM_Loot_JewelryBox -> T_Icon_LT_JewelryBox.png)
"""
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402

import cosmetics as CO  # noqa: E402
import hhblend as H  # noqa: E402
import props as P  # noqa: E402
import props_world as W  # noqa: E402

# Per-item camera direction (default matches items.py); flat things are seen a little more from above.
DIRECTION = {
    'SM_Loot_TV': (0.5, -1.0, 0.35),
    'SM_Loot_Painting': (0.45, -1.0, 0.3),
    'SM_Loot_Whiskey': (0.45, -1.0, 0.45),
    'SM_Loot_Candlestick': (0.45, -1.0, 0.45),
    'SM_Loot_Vase': (0.45, -1.0, 0.45),
    'SM_Loot_Perfume': (0.45, -1.0, 0.5),
    'SM_Loot_Figurine': (0.4, -1.0, 0.4),
    'SM_Loot_MantelClock': (0.45, -1.0, 0.4),
    'SM_Loot_Camera': (0.55, -1.0, 0.5),
    'SM_Loot_Handbag': (0.55, -1.0, 0.5),
    'SM_Loot_Locket': (0.25, -0.8, 1.7),
    'SM_Loot_Ring': (0.4, -1.0, 0.95),
    'SM_Loot_Silverware': (0.45, -1.0, 1.1),
    'SM_Loot_Necklace': (0.35, -1.0, 1.3),
    'SM_Loot_Phone': (0.45, -1.0, 1.0),
    'SM_Loot_Tablet': (0.45, -1.0, 1.0),
}


def icon_name(mesh):
    return 'T_Icon_LT_' + mesh[len('SM_Loot_'):] + '.png'


def main():
    names = [a for a in sys.argv[1:] if a.startswith('SM_Loot_')] or W.LOOT
    H.reset()
    # Catalogue images that are not generated yet get labelled stand-ins so the run does not fail;
    # icons using them are listed at the end and should be re-rendered once the textures exist.
    import preview_world_props as Q
    missing = set(Q.placeholder_materials(os.path.join(tempfile.gettempdir(), 'hh_placeholders')))
    stale = []
    scene, cam = CO.icon_scene()
    os.makedirs(CO.ICON_DIR, exist_ok=True)
    for name in names:
        b = P.build(name)
        if missing & set(b.mats):
            stale.append(name)
        obj = H.link(b.object())
        CO.frame_objects(cam, [obj], direction=DIRECTION.get(name, (0.55, -1.0, 0.75)), margin=1.08)
        scene.render.filepath = os.path.join(CO.ICON_DIR, icon_name(name))
        bpy.ops.render.render(write_still=True)
        bpy.data.objects.remove(obj)
        print('icon', icon_name(name), flush=True)
    if stale:
        print('WARNING: rendered with stand-in textures (re-run for these once the textures exist):',
              ' '.join(stale), flush=True)


if __name__ == '__main__':
    main()
