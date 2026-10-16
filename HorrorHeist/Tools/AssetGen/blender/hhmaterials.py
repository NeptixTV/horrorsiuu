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
    'MI_Emit_Blue': emissive((0.08, 0.25, 1.0), 8.0, (0.02, 0.05, 0.3)),
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
    # The haunting (textures: textures/gen_haunt_images.py)
    'MI_Decal_Handprints': decal('T_Decal_Handprints', opacity=0.95, rough=0.35),
    'MI_Decal_GetOut': decal('T_Decal_GetOut', opacity=0.95, rough=0.45),
    'MI_Decal_StillHere': decal('T_Decal_StillHere', opacity=0.95, rough=0.45),
    'MI_Decal_Scratches': decal('T_Decal_Scratches', opacity=0.9, rough=0.9),
    'MI_Decal_ChalkCircle': decal('T_Decal_ChalkCircle', opacity=0.9, rough=0.95),
    'MI_Decal_ChildDrawing': decal('T_Decal_ChildDrawing', opacity=1.0, rough=0.8),

    # --- Town: ground, streets, exteriors (textures: textures/gen_world_textures.py) -----
    'MI_Asphalt': surface('T_Asphalt_Wet', (0.78, 0.78, 0.78), uv=0.25, rough_add=0.12),
    'MI_Sidewalk': surface('T_Concrete_Sidewalk', uv=0.5),
    'MI_Curb': surface('T_Concrete_Floor', (0.70, 0.70, 0.68), uv=0.8),
    'MI_Road_Paint_White': surface('T_Concrete_Floor', (0.85, 0.85, 0.82), uv=0.5, rough_add=0.1),
    'MI_Road_Paint_Yellow': surface('T_Concrete_Floor', (0.80, 0.58, 0.10), uv=0.5, rough_add=0.1),
    'MI_Grass': surface('T_Grass', uv=0.5),
    'MI_Dirt': surface('T_Dirt', uv=0.5),
    'MI_Gravel': surface('T_Gravel', uv=0.8),
    'MI_Hedge': surface('T_Foliage', (0.55, 0.70, 0.45), uv=1.2),
    'MI_Tree_Leaves': surface('T_Foliage', (0.70, 0.62, 0.40), uv=1.0),
    'MI_Bark': surface('T_Bark', uv=1.5),
    'MI_Roof_Shingles_Dark': surface('T_Roof_Shingles', (0.42, 0.42, 0.44), uv=0.8),
    'MI_Roof_Shingles_Brown': surface('T_Roof_Shingles', (0.55, 0.40, 0.30), uv=0.8),
    'MI_Siding_White': surface('T_Siding_Wood', (0.82, 0.81, 0.77), uv=0.5),
    'MI_Siding_Grey': surface('T_Siding_Wood', (0.52, 0.54, 0.55), uv=0.5),
    'MI_Siding_Blue': surface('T_Siding_Wood', (0.36, 0.45, 0.52), uv=0.5),
    'MI_Siding_Green': surface('T_Siding_Wood', (0.40, 0.47, 0.40), uv=0.5),
    'MI_Siding_Worn': surface('T_Siding_Worn', uv=0.5),
    'MI_Brick_Red': surface('T_Brick_Old', (0.95, 0.85, 0.80), uv=1.0, rough_add=0.05),
    'MI_Stone_Wall': surface('T_Stone_Wall', uv=0.5),
    'MI_Trim_White': surface('T_Paint_Wood', (0.86, 0.85, 0.80), uv=1.0),
    'MI_Trim_Dark': surface('T_Paint_Wood', (0.16, 0.15, 0.14), uv=1.0),
    'MI_Paint_Wood_Red': surface('T_Paint_Wood', (0.40, 0.08, 0.06), uv=1.0),
    'MI_Car_Paint_Red': surface('T_Metal_Painted', (0.40, 0.04, 0.04), uv=0.5, rough=0.55),
    'MI_Car_Paint_Blue': surface('T_Metal_Painted', (0.08, 0.14, 0.28), uv=0.5, rough=0.55),
    'MI_Car_Paint_Silver': surface('T_Metal_Painted', (0.55, 0.56, 0.57), uv=0.5, rough=0.5),
    'MI_Car_Paint_Black': surface('T_Metal_Painted', (0.03, 0.03, 0.035), uv=0.5, rough=0.5),
    'MI_Car_Paint_White': surface('T_Metal_Painted', (0.72, 0.72, 0.70), uv=0.5, rough=0.45),
    'MI_Car_Paint_Green': surface('T_Metal_Painted', (0.12, 0.22, 0.15), uv=0.5, rough=0.55),
    'MI_Glass_Window': glass((0.80, 0.86, 0.88), 0.22, 0.05),
    'MI_Glass_Shop': glass((0.78, 0.84, 0.86), 0.15, 0.04),
    'MI_Emit_Window_Warm': emissive((1.0, 0.66, 0.36), 4.0, (0.5, 0.4, 0.3), 0.9),
    'MI_Emit_Neon_Red': emissive((1.0, 0.10, 0.08), 20.0, (0.4, 0.05, 0.05)),
    # Traffic signals: bright enough to read in daylight.
    'MI_Emit_Signal_Red': emissive((1.0, 0.06, 0.03), 60.0, (0.5, 0.05, 0.03), 0.2),
    'MI_Emit_Signal_Amber': emissive((1.0, 0.50, 0.04), 60.0, (0.5, 0.3, 0.03), 0.2),
    'MI_Emit_Signal_Green': emissive((0.15, 1.0, 0.55), 60.0, (0.05, 0.4, 0.2), 0.2),
    # Downtown surfaces (tinted from existing texture sets).
    'MI_Floor_Marble': surface('T_Tiles_Square', (0.86, 0.84, 0.80), uv=0.5, rough=0.6),
    'MI_Floor_Vinyl': surface('T_Tiles_Square', (0.80, 0.80, 0.78), uv=1.0, rough=0.9),
    'MI_Paint_Bank_Green': surface('T_Plaster', (0.30, 0.40, 0.33), uv=0.5),
    'MI_Metal_White': surface('T_Metal_Painted', (0.80, 0.80, 0.78), uv=1.0, rough=0.8),
    'MI_Metal_Vault': surface('T_Metal_Steel', (1.0, 1.0, 0.97), uv=0.8, rough=0.5),
    'MI_Plastic_Blue': solid((0.06, 0.20, 0.55), 0.45),
    'MI_Plastic_Green': solid((0.10, 0.45, 0.18), 0.45),
    'MI_Plastic_Orange': solid((0.80, 0.30, 0.04), 0.45),
    'MI_Rubber_Mat': surface('T_Rubber', (0.95, 0.42, 0.30), uv=0.5, rough=1.1),
    'MI_Sand': surface('T_Concrete_Sidewalk', (0.98, 0.84, 0.62), uv=1.2, rough=1.1),
    'MI_Fabric_Sheet': surface('T_Cotton', (0.86, 0.86, 0.84), uv=1.0),
    'MI_Glass_Teller': glass((0.85, 0.9, 0.9), 0.12, 0.03),

    # --- Interiors ----------------------------------------------------------------------
    'MI_Wallpaper_Floral': surface('T_Wallpaper_Floral', uv=1.0),
    'MI_Wallpaper_Stripe': surface('T_Wallpaper_Stripe', uv=1.0),
    'MI_Wallpaper_Damask': surface('T_Wallpaper_Damask', uv=1.0),
    'MI_Paint_White': surface('T_Plaster', (0.80, 0.79, 0.75), uv=0.5),
    'MI_Paint_Cream': surface('T_Plaster', (0.78, 0.72, 0.58), uv=0.5),
    'MI_Paint_Sage': surface('T_Plaster', (0.52, 0.58, 0.48), uv=0.5),
    'MI_Paint_Blue': surface('T_Plaster', (0.46, 0.54, 0.62), uv=0.5),
    'MI_Wood_Floor': surface('T_Wood_Floor', uv=0.5),
    'MI_Wood_Floor_Dark': surface('T_Wood_Floor', (0.50, 0.38, 0.30), uv=0.5),
    'MI_Carpet_Beige': surface('T_Carpet', (0.72, 0.64, 0.52), uv=1.0),
    'MI_Carpet_Burgundy': surface('T_Carpet', (0.40, 0.12, 0.12), uv=1.0),
    'MI_Tiles_White': surface('T_Tiles_Square', uv=1.0),
    'MI_Tiles_Checker': surface('T_Tiles_Checker', uv=0.8333),          # 4 x 4 tiles of 30 cm per texture
    'MI_Countertop': surface('T_Countertop', uv=1.0),
    'MI_Cabinet_Wood': surface('T_Wood_Raw', (0.62, 0.45, 0.30), uv=1.0),
    'MI_Cabinet_White': surface('T_Paint_Wood', (0.82, 0.81, 0.77), uv=1.0),
    'MI_Fabric_Sofa': surface('T_Fabric_Weave', (0.36, 0.30, 0.24), uv=2.0),
    'MI_Fabric_Sofa_Green': surface('T_Fabric_Weave', (0.20, 0.27, 0.20), uv=2.0),
    'MI_Fabric_Bedding': surface('T_Cotton', (0.78, 0.76, 0.70), uv=2.0),
    'MI_Fabric_Curtain': surface('T_Fabric_Weave', (0.32, 0.07, 0.07), uv=1.5),
    'MI_Fabric_Curtain_Light': surface('T_Cotton', (0.75, 0.70, 0.58), uv=1.5),
    'MI_Porcelain': solid((0.82, 0.82, 0.80), 0.12),
    'MI_Screen_Black': solid((0.006, 0.006, 0.007), 0.05),
    'MI_Plastic_Laptop': solid((0.09, 0.09, 0.10), 0.35, 0.2),
    'MI_Gold': solid((0.86, 0.66, 0.30), 0.25, 1.0),
    'MI_Silver': solid((0.85, 0.85, 0.86), 0.2, 1.0),
    'MI_Velvet_Blue': surface('T_Fabric_Weave', (0.08, 0.10, 0.25), uv=4.0, rough=1.1),
    'MI_Img_Painting_Lore': image('T_Painting_Lore', 0.55),
    'MI_Img_Painting_Landscape': image('T_Painting_Landscape', 0.55),
    'MI_Img_Photo_Family': image('T_Photo_Family', 0.3),
    'MI_Img_Blueprint_Vance': image('T_Blueprint_Vance', 0.8),

    # --- Signs (text rendered with the project fonts) -----------------------------------
    'MI_Img_Sign_Brannigan': image('T_Sign_Brannigan', 0.6),
    'MI_Img_Sign_Laundromat': image('T_Sign_Laundromat', 0.6),
    'MI_Img_Sign_Hardware': image('T_Sign_Hardware', 0.6),
    'MI_Img_Sign_Museum': image('T_Sign_Museum', 0.6),
    'MI_Img_Sign_Clothing': image('T_Sign_Clothing', 0.6),
    'MI_Img_Sign_Diner': image('T_Sign_Diner', 0.6),
    'MI_Img_Sign_Parking': image('T_Sign_Parking', 0.5),
    'MI_Img_Sign_Street': image('T_Sign_StreetNames', 0.5),
    'MI_Img_Sign_Stop': image('T_Sign_Stop', 0.5),
    'MI_Img_Sign_Speed': image('T_Sign_Speed', 0.5),
    'MI_Img_House_Numbers': image('T_House_Numbers', 0.4, masked=True),
    'MI_Img_Shop_Interior': image('T_Shop_Interior', 0.7),
    # Downtown (textures: textures/gen_town_images.py)
    'MI_Img_Sign_Bank': image('T_Sign_Bank', 0.5),
    'MI_Img_Sign_Market': image('T_Sign_Market', 0.5),
    'MI_Img_Sign_Gazette': image('T_Sign_Gazette', 0.6),
    'MI_Img_Sign_Pharmacy': image('T_Sign_Pharmacy', 0.6),
    'MI_Img_Sign_Barber': image('T_Sign_Barber', 0.6),
    'MI_Img_Sign_Playground': image('T_Sign_Playground', 0.6),
    'MI_Img_Groceries': image('T_Groceries', 0.55),
    'MI_Img_Aisle_Signs': image('T_Aisle_Signs', 0.5),
    'MI_Img_Poster_Bank': image('T_Poster_Bank', 0.7),
    'MI_Img_Poster_Market': image('T_Poster_Market', 0.7),
    'MI_Img_Police_Tape': image('T_Police_Tape', 0.45),
    'MI_Screen_ATM': screen('T_ATM_Screen', (0.8, 0.88, 1.0), 2.5, 0.0),

    # --- Town decals --------------------------------------------------------------------
    'MI_Decal_Leaves': decal('T_Decal_Leaves', opacity=1.0, rough=0.8),
    'MI_Decal_Puddle': decal('T_Decal_Puddle', (0.6, 0.6, 0.6), opacity=0.9, rough=0.05),
    'MI_Decal_Mud': decal('T_Decal_Mud', opacity=0.9, rough=0.7),

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
    'MI_Char_Rubber2': character('T_Plaster', uv=4.0, use_tint2=True, rough=1.1),   # light soles, tinted
    'MI_Char_Glass': glass((0.35, 0.4, 0.42), 0.25, 0.05),
    'MI_Char_Hair': character('T_Fabric_Weave', (0.75, 0.75, 0.75), uv=8.0, rough=0.75),
    'MI_Char_Plastic': character('T_Plaster', uv=2.0, rough=0.55),
    'MI_Char_Plastic2': character('T_Plaster', uv=2.0, rough=0.55, use_tint2=True),
    'MI_Char_Metal': character('T_Metal_Steel', uv=2.0),
    'MI_Char_Paper': character('T_Cardboard', uv=2.0),
    'MI_Char_Eye': solid((0.03, 0.025, 0.02), 0.1),
    'MI_Char_EyeWhite': solid((0.72, 0.69, 0.64), 0.15),
}

TEXTURE_SETS = sorted({v['tex'] for v in CATALOG.values() if 'tex' in v})
IMAGES = sorted({v['image'] for v in CATALOG.values() if 'image' in v})
