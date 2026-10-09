"""
Material catalogue shared by the Blender generators (preview shading) and the Unreal setup
script (which builds one Material Instance per entry from the master named in 'master').

Mesh material slots are named after these entries, so a slot called 'MI_Wood_Raw' gets the
Unreal instance /Game/HorrorHeist/Materials/Instances/MI_Wood_Raw.

UVs are authored in metres (1 UV unit = 1 m); 'uv' says how often the texture repeats per
metre, i.e. 1 / (real-world size of the texture).
"""

# Masters (created by the Unreal setup script):
#   Surface   BaseColorMap, NormalMap, ORMMap | Tint | UVScale, RoughnessScale, RoughnessOffset,
#             MetallicScale, NormalStrength
#   Solid     BaseColor | Roughness, Metallic
#   Emissive  BaseColor, EmissiveColor | EmissiveStrength, Roughness
#   Glass     Tint | Opacity, Roughness
#   Image     ImageMap | Roughness            (opaque)
#   ImageMask ImageMap | Roughness            (masked by alpha, two-sided)
#   Screen    ScreenMap | ScreenColor | EmissiveStrength, ScrollSpeed
#   Decal     DecalMap | Tint | Opacity, Roughness
#   Character BaseColorMap, NormalMap, ORMMap | Tint, Tint2, SkinTint | UVScale, UseTint2,
#             UseSkinTint, RoughnessScale, MetallicScale


def surface(tex, tint=(1, 1, 1), uv=1.0, rough=1.0, rough_add=0.0, metal=1.0, normal=1.0):
    return dict(master='Surface', tex=tex, tint=tint, uv=uv, rough=rough, rough_add=rough_add,
                metal=metal, normal=normal)


def solid(color, rough=0.5, metal=0.0):
    return dict(master='Solid', color=color, rough=rough, metal=metal)


def emissive(color, strength=8.0, base=(0.8, 0.8, 0.8), rough=0.4):
    return dict(master='Emissive', emissive=color, strength=strength, color=base, rough=rough)


def glass(tint=(0.9, 0.95, 0.95), opacity=0.18, rough=0.08):
    return dict(master='Glass', tint=tint, opacity=opacity, rough=rough)


def image(tex, rough=0.8, masked=False):
    return dict(master='ImageMask' if masked else 'Image', image=tex, rough=rough)


def screen(tex='T_TV_Static', color=(0.75, 0.85, 0.95), strength=3.0, scroll=0.6):
    return dict(master='Screen', image=tex, emissive=color, strength=strength, scroll=scroll)


def decal(tex, tint=(1, 1, 1), opacity=1.0, rough=0.6):
    return dict(master='Decal', image=tex, tint=tint, opacity=opacity, rough=rough)


def character(tex, tint=(1, 1, 1), uv=3.0, use_tint2=False, skin=False, rough=1.0, metal=1.0):
    return dict(master='Character', tex=tex, tint=tint, uv=uv, use_tint2=use_tint2, skin=skin,
                rough=rough, metal=metal)


CATALOG = {
    # --- Architecture -------------------------------------------------------------------
    'MI_Concrete_Floor': surface('T_Concrete_Floor', uv=0.4),
    'MI_Concrete_Ceiling': surface('T_Concrete_Floor', (0.72, 0.72, 0.70), uv=0.33),
    'MI_Concrete_Raw': surface('T_Concrete_Floor', (0.85, 0.84, 0.82), uv=0.5),
    'MI_Block_Lower': surface('T_Cinderblock_Painted', (0.20, 0.27, 0.24), uv=0.625),
    'MI_Block_Upper': surface('T_Cinderblock_Painted', (0.74, 0.72, 0.66), uv=0.625),
    'MI_Block_Raw': surface('T_Cinderblock_Painted', (0.52, 0.51, 0.49), uv=0.625, rough=1.05),
    'MI_Brick': surface('T_Brick_Old', uv=1.0),
    'MI_Plaster': surface('T_Plaster', (0.82, 0.80, 0.74), uv=0.5),
    'MI_Asphalt_Wet': surface('T_Asphalt_Wet', uv=0.5),

    # --- Wood ---------------------------------------------------------------------------
    'MI_Wood_Planks': surface('T_Wood_Planks', uv=1.0),
    'MI_Wood_Raw': surface('T_Wood_Raw', uv=1.0),
    'MI_Wood_Dark': surface('T_Wood_Raw', (0.42, 0.30, 0.22), uv=1.0),
    'MI_Wood_Painted': surface('T_Metal_Painted', (0.36, 0.40, 0.36), uv=1.0, metal=0.0, rough=1.1),
    'MI_Pegboard': surface('T_Pegboard', uv=2.5),

    # --- Metal --------------------------------------------------------------------------
    'MI_Metal_Green': surface('T_Metal_Painted', (0.16, 0.25, 0.20)),
    'MI_Metal_Grey': surface('T_Metal_Painted', (0.40, 0.41, 0.40)),
    'MI_Metal_Blue': surface('T_Metal_Painted', (0.14, 0.20, 0.28)),
    'MI_Metal_Red': surface('T_Metal_Painted', (0.42, 0.06, 0.05)),
    'MI_Metal_Yellow': surface('T_Metal_Painted', (0.62, 0.45, 0.08)),
    'MI_Metal_Cream': surface('T_Metal_Painted', (0.72, 0.69, 0.60)),
    'MI_Metal_Black': surface('T_Metal_Painted', (0.05, 0.05, 0.05)),
    'MI_Van_Paint': surface('T_Metal_Painted', (0.78, 0.76, 0.70), uv=0.5, rough=0.8),
    'MI_Metal_Rust': surface('T_Metal_Rust'),
    'MI_Steel': surface('T_Metal_Steel'),
    'MI_Chrome': surface('T_Metal_Steel', (0.95, 0.95, 0.95), rough=0.25),
    'MI_ChainLink': image('T_ChainLink', rough=0.45, masked=True),

    # --- Soft goods ---------------------------------------------------------------------
    'MI_Fabric_Olive': surface('T_Fabric_Weave', (0.24, 0.26, 0.15), uv=2.0),
    'MI_Fabric_Brown': surface('T_Fabric_Weave', (0.33, 0.21, 0.13), uv=2.0),
    'MI_Fabric_Rug': surface('T_Fabric_Weave', (0.40, 0.10, 0.07), uv=1.5),
    'MI_Fabric_Grey': surface('T_Fabric_Weave', (0.30, 0.30, 0.29), uv=2.0),
    'MI_Fabric_Navy': surface('T_Fabric_Weave', (0.08, 0.10, 0.17), uv=2.0),
    'MI_Fabric_Shade': surface('T_Fabric_Weave', (0.80, 0.70, 0.52), uv=3.0),
    'MI_Leather': surface('T_Leather', (0.32, 0.18, 0.10), uv=2.0),
    'MI_Leather_Black': surface('T_Leather', (0.06, 0.055, 0.05), uv=2.0),
    'MI_Cardboard': surface('T_Cardboard', uv=1.5),
    'MI_Cork': surface('T_Cork', uv=2.0),
    'MI_Rubber': surface('T_Rubber', uv=2.0),
    'MI_Tire': surface('T_Rubber', (0.35, 0.35, 0.35), uv=3.0),
    'MI_Nylon_Black': surface('T_Nylon', (0.07, 0.07, 0.075), uv=3.0),
    'MI_Nylon_Olive': surface('T_Nylon', (0.20, 0.22, 0.13), uv=3.0),

    # --- Flat colours -------------------------------------------------------------------
    'MI_Plastic_Black': solid((0.02, 0.02, 0.02), 0.45),
    'MI_Plastic_Grey': solid((0.18, 0.18, 0.17), 0.5),
    'MI_Plastic_Beige': solid((0.55, 0.50, 0.40), 0.5),
    'MI_Plastic_Red': solid((0.45, 0.04, 0.03), 0.4),
    'MI_Plastic_Yellow': solid((0.75, 0.55, 0.05), 0.45),
    'MI_Plastic_White': solid((0.70, 0.70, 0.68), 0.4),
    'MI_Rubber_Black': solid((0.025, 0.025, 0.025), 0.85),
    'MI_Bakelite': solid((0.10, 0.05, 0.025), 0.3),
    'MI_Paper': solid((0.72, 0.70, 0.64), 0.85),
    'MI_Brass': solid((0.78, 0.58, 0.30), 0.35, 1.0),
    'MI_Copper': solid((0.72, 0.40, 0.25), 0.4, 1.0),
    'MI_Aluminium': solid((0.80, 0.80, 0.80), 0.35, 1.0),
    'MI_Mirror': solid((0.92, 0.92, 0.92), 0.02, 1.0),
    'MI_Ceramic': solid((0.80, 0.78, 0.72), 0.2),
    'MI_Glass_Bottle': glass((0.20, 0.32, 0.12), 0.55, 0.05),
    'MI_Wax': solid((0.80, 0.74, 0.60), 0.5),
    'MI_Cash': solid((0.42, 0.50, 0.38), 0.8),
    'MI_Cable': solid((0.03, 0.03, 0.03), 0.6),

    # --- Glass --------------------------------------------------------------------------
    'MI_Glass': glass(),
    'MI_Glass_Dirty': glass((0.75, 0.75, 0.68), 0.32, 0.25),
    'MI_Glass_Van': solid((0.012, 0.014, 0.015), 0.04),

    # --- Light emitters (EmissiveStrength is driven by HHPracticalLight) ----------------
    'MI_Emit_Bulb': emissive((1.0, 0.72, 0.42), 30.0, (0.9, 0.85, 0.7)),
    'MI_Emit_Fluoro': emissive((0.85, 0.95, 1.0), 20.0, (0.9, 0.9, 0.9)),
    'MI_Emit_Shade': emissive((1.0, 0.62, 0.32), 3.0, (0.8, 0.7, 0.5), 0.8),
    'MI_Emit_Ember': emissive((1.0, 0.28, 0.04), 12.0, (0.2, 0.1, 0.05)),
    'MI_Emit_Red': emissive((1.0, 0.05, 0.03), 6.0, (0.3, 0.02, 0.02)),
    'MI_Emit_Green': emissive((0.2, 1.0, 0.25), 4.0, (0.05, 0.2, 0.05)),
    'MI_Emit_Amber': emissive((1.0, 0.45, 0.05), 4.0, (0.3, 0.15, 0.02)),
    'MI_Emit_Street': emissive((1.0, 0.55, 0.22), 25.0, (0.8, 0.6, 0.4)),
    'MI_Screen_TV': screen('T_TV_Static', (0.70, 0.80, 0.95), 2.5, 0.8),
    'MI_Screen_Monitor': screen('T_TV_Static', (0.55, 0.85, 0.65), 1.6, 0.2),

    # --- Printed matter -----------------------------------------------------------------
    'MI_Img_Map': image('T_Map_Hollowmere'),
    'MI_Img_Notes': image('T_Notes_Atlas'),
    'MI_Img_PosterMissing': image('T_Poster_Missing'),
    'MI_Img_PosterMuseum': image('T_Poster_Museum', 0.6),
    'MI_Img_Calendar': image('T_Calendar'),
    'MI_Img_Newspaper': image('T_Newspaper'),
    'MI_Img_SignFence': image('T_Sign_Fence', 0.5),
    'MI_Img_VanLivery': image('T_Van_Livery', 0.55, masked=True),
    'MI_Img_Polaroids': image('T_Polaroids', 0.35),
    'MI_Img_ClockFace': image('T_ClockFace', 0.3),

    # --- Decals -------------------------------------------------------------------------
    'MI_Decal_Oil': decal('T_Decal_OilStain', opacity=0.9, rough=0.25),
    'MI_Decal_Water': decal('T_Decal_WaterStain', opacity=0.8, rough=0.7),
    'MI_Decal_Grime': decal('T_Decal_Grime', opacity=0.8, rough=0.9),
    'MI_Decal_Footprints': decal('T_Decal_Footprints', opacity=0.9, rough=0.85),
    'MI_Decal_Cracks': decal('T_Decal_Cracks', opacity=1.0, rough=0.9),

    # --- Characters ---------------------------------------------------------------------
    'MI_Char_Skin': character('T_Skin', (0.80, 0.62, 0.50), uv=4.0, skin=True),
    'MI_Char_Cotton': character('T_Cotton'),
    'MI_Char_Cotton2': character('T_Cotton', use_tint2=True),
    'MI_Char_Knit': character('T_Knit', uv=4.0),
    'MI_Char_Knit2': character('T_Knit', uv=4.0, use_tint2=True),
    'MI_Char_Denim': character('T_Denim', uv=3.0),
    'MI_Char_Leather': character('T_Leather', uv=3.0),
    'MI_Char_Nylon': character('T_Nylon', uv=3.0),
    'MI_Char_Nylon2': character('T_Nylon', uv=3.0, use_tint2=True),
    'MI_Char_Fabric': character('T_Fabric_Weave', uv=3.0),
    'MI_Char_Rubber': character('T_Rubber', (0.25, 0.25, 0.25), uv=4.0),
    'MI_Char_Hair': character('T_Fabric_Weave', (0.75, 0.75, 0.75), uv=8.0, rough=0.75),
    'MI_Char_Plastic': character('T_Plaster', uv=2.0, rough=0.55),
    'MI_Char_Plastic2': character('T_Plaster', uv=2.0, rough=0.55, use_tint2=True),
    'MI_Char_Metal': character('T_Metal_Steel', uv=2.0),
    'MI_Char_Paper': character('T_Cardboard', uv=2.0),
    'MI_Char_Eye': solid((0.03, 0.025, 0.02), 0.1),
}

TEXTURE_SETS = sorted({v['tex'] for v in CATALOG.values() if 'tex' in v})
IMAGES = sorted({v['image'] for v in CATALOG.values() if 'image' in v})
