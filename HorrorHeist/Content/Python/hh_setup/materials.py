"""Master materials, material instances (one per catalogue entry) and physical materials."""
import os
import sys

import unreal

from . import util as U

MASTER_DIR = U.ROOT + '/Materials/Masters'
INSTANCE_DIR = U.ROOT + '/Materials/Instances'
PHYS_DIR = U.ROOT + '/Materials/Physical'

MP = unreal.MaterialProperty
X = unreal.MaterialExpressionTextureSampleParameter2D


def catalog():
    tools = os.path.join(U.PROJECT, 'Tools', 'AssetGen', 'blender')
    if tools not in sys.path:
        sys.path.insert(0, tools)
    import hhmaterials
    return hhmaterials.CATALOG


# ---------------------------------------------------------------------------------------------
# Graph helpers

class Graph:
    def __init__(self, mat):
        self.mat = mat
        self.y = 0

    def node(self, cls, x=-600, y=None, **props):
        if y is None:
            y = self.y
            self.y += 140
        e = U.MEL.create_material_expression(self.mat, cls, x, y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def scalar(self, name, default, x=-900):
        return self.node(unreal.MaterialExpressionScalarParameter, x, parameter_name=name, default_value=float(default))

    def vector(self, name, default, x=-900):
        return self.node(unreal.MaterialExpressionVectorParameter, x, parameter_name=name,
                         default_value=U.colour(default))

    def texture(self, name, default, sampler, x=-900):
        tex = U.find(default, required=False)
        e = self.node(X, x, parameter_name=name, sampler_type=sampler)
        if tex is not None:
            e.set_editor_property('texture', tex)
        return e

    def op(self, cls, a, b=None, a_pin='', b_pin='', x=-400):
        e = self.node(cls, x)
        self.link(a, a_pin, e, 'A')
        if b is not None:
            self.link(b, b_pin, e, 'B')
        return e

    def link(self, a, a_pin, b, b_pin):
        if isinstance(a, (int, float)):
            return
        if not U.MEL.connect_material_expressions(a, a_pin, b, b_pin):
            U.warn(f'{self.mat.get_name()}: could not connect {a.get_name()}.{a_pin} -> {b.get_name()}.{b_pin}')

    def out(self, e, pin, prop):
        if not U.MEL.connect_material_property(e, pin, prop):
            U.warn(f'{self.mat.get_name()}: could not connect {e.get_name()}.{pin} -> {prop}')

    def const(self, value):
        return self.node(unreal.MaterialExpressionConstant, -700, r=float(value))

    def const3(self, rgb):
        return self.node(unreal.MaterialExpressionConstant3Vector, -700, constant=U.colour(rgb))


def new_master(name, setup=None):
    mat = U.create_or_load(name, MASTER_DIR, unreal.Material, unreal.MaterialFactoryNew())
    U.MEL.delete_all_material_expressions(mat)
    for key, value in (setup or {}).items():
        mat.set_editor_property(key, value)
    mat.set_editor_property('used_with_skeletal_mesh', True)
    return mat, Graph(mat)


def finish(mat):
    U.MEL.layout_material_expressions(mat)
    U.MEL.recompile_material(mat)
    U.remember(mat)
    return mat


def _uv(g):
    tc = g.node(unreal.MaterialExpressionTextureCoordinate, -1300)
    scale = g.scalar('UVScale', 1.0, -1300)
    return g.op(unreal.MaterialExpressionMultiply, tc, scale, x=-1100)


def _pbr_maps(g, uv):
    s = unreal.MaterialSamplerType
    bc = g.texture('BaseColorMap', 'T_Concrete_Floor_BC', s.SAMPLERTYPE_COLOR)
    nm = g.texture('NormalMap', 'T_Concrete_Floor_N', s.SAMPLERTYPE_NORMAL)
    orm = g.texture('ORMMap', 'T_Concrete_Floor_ORM', s.SAMPLERTYPE_MASKS)
    for t in (bc, nm, orm):
        g.link(uv, '', t, 'UVs')
    return bc, nm, orm


def _orm_outputs(g, nm, orm):
    rough = g.op(unreal.MaterialExpressionMultiply, orm, g.scalar('RoughnessScale', 1.0), a_pin='G')
    rough = g.op(unreal.MaterialExpressionAdd, rough, g.scalar('RoughnessOffset', 0.0))
    clamp = g.node(unreal.MaterialExpressionClamp, -200)
    g.link(rough, '', clamp, 'Input')
    g.out(clamp, '', MP.MP_ROUGHNESS)
    metal = g.op(unreal.MaterialExpressionMultiply, orm, g.scalar('MetallicScale', 1.0), a_pin='B')
    g.out(metal, '', MP.MP_METALLIC)
    g.out(orm, 'R', MP.MP_AMBIENT_OCCLUSION)
    lerp = g.node(unreal.MaterialExpressionLinearInterpolate, -300)
    g.link(g.const3((0, 0, 1)), '', lerp, 'A')
    g.link(nm, 'RGB', lerp, 'B')
    g.link(g.scalar('NormalStrength', 1.0), '', lerp, 'Alpha')
    g.out(lerp, '', MP.MP_NORMAL)


def master_surface():
    mat, g = new_master('M_HH_Surface')
    uv = _uv(g)
    bc, nm, orm = _pbr_maps(g, uv)
    base = g.op(unreal.MaterialExpressionMultiply, bc, g.vector('Tint', (1, 1, 1)), a_pin='RGB')
    g.out(base, '', MP.MP_BASE_COLOR)
    _orm_outputs(g, nm, orm)
    return finish(mat)


def master_character():
    mat, g = new_master('M_HH_Character')
    uv = _uv(g)
    bc, nm, orm = _pbr_maps(g, uv)
    t12 = g.node(unreal.MaterialExpressionLinearInterpolate, -600)
    g.link(g.vector('Tint', (1, 1, 1)), '', t12, 'A')
    g.link(g.vector('Tint2', (1, 1, 1)), '', t12, 'B')
    g.link(g.scalar('UseTint2', 0.0), '', t12, 'Alpha')
    tint = g.node(unreal.MaterialExpressionLinearInterpolate, -450)
    g.link(t12, '', tint, 'A')
    g.link(g.vector('SkinTint', (0.86, 0.67, 0.54)), '', tint, 'B')
    g.link(g.scalar('UseSkinTint', 0.0), '', tint, 'Alpha')
    base = g.op(unreal.MaterialExpressionMultiply, bc, tint, a_pin='RGB')
    g.out(base, '', MP.MP_BASE_COLOR)
    _orm_outputs(g, nm, orm)
    return finish(mat)


def master_solid():
    mat, g = new_master('M_HH_Solid')
    g.out(g.vector('BaseColor', (0.5, 0.5, 0.5)), '', MP.MP_BASE_COLOR)
    g.out(g.scalar('Roughness', 0.5), '', MP.MP_ROUGHNESS)
    g.out(g.scalar('Metallic', 0.0), '', MP.MP_METALLIC)
    return finish(mat)


def master_emissive():
    mat, g = new_master('M_HH_Emissive')
    g.out(g.vector('BaseColor', (0.8, 0.8, 0.8)), '', MP.MP_BASE_COLOR)
    g.out(g.scalar('Roughness', 0.4), '', MP.MP_ROUGHNESS)
    em = g.op(unreal.MaterialExpressionMultiply, g.vector('EmissiveColor', (1, 0.8, 0.6)),
              g.scalar('EmissiveStrength', 8.0))
    g.out(em, '', MP.MP_EMISSIVE_COLOR)
    return finish(mat)


def master_glass():
    mat, g = new_master('M_HH_Glass', {
        'blend_mode': unreal.BlendMode.BLEND_TRANSLUCENT,
        'translucency_lighting_mode': unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING})
    g.out(g.vector('Tint', (0.9, 0.95, 0.95)), '', MP.MP_BASE_COLOR)
    g.out(g.scalar('Roughness', 0.08), '', MP.MP_ROUGHNESS)
    g.out(g.scalar('Opacity', 0.2), '', MP.MP_OPACITY)
    g.out(g.const(0.6), '', MP.MP_SPECULAR)
    return finish(mat)


def master_image(masked):
    setup = {'blend_mode': unreal.BlendMode.BLEND_MASKED, 'two_sided': True} if masked else {}
    mat, g = new_master('M_HH_ImageMask' if masked else 'M_HH_Image', setup)
    tex = g.texture('ImageMap', 'T_Van_Livery' if masked else 'T_Map_Hollowmere',
                    unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    g.out(tex, 'RGB', MP.MP_BASE_COLOR)
    g.out(g.scalar('Roughness', 0.8), '', MP.MP_ROUGHNESS)
    g.out(g.const(0.3), '', MP.MP_SPECULAR)
    if masked:
        g.out(tex, 'A', MP.MP_OPACITY_MASK)
    return finish(mat)


def master_screen():
    mat, g = new_master('M_HH_Screen')
    time = g.node(unreal.MaterialExpressionTime, -1500)
    frames = g.op(unreal.MaterialExpressionMultiply, time, None, x=-1350)
    frames.set_editor_property('const_b', 24.0)
    floor = g.node(unreal.MaterialExpressionFloor, -1200)
    g.link(frames, '', floor, 'Input')
    jitter = g.op(unreal.MaterialExpressionMultiply, floor,
                  g.node(unreal.MaterialExpressionConstant2Vector, -1200, r=0.618, g=0.414), x=-1050)
    roll = g.op(unreal.MaterialExpressionMultiply, time, g.scalar('ScrollSpeed', 0.6, -1350), x=-1200)
    roll2 = g.op(unreal.MaterialExpressionMultiply, roll,
                 g.node(unreal.MaterialExpressionConstant2Vector, -1050, r=0.0, g=1.0), x=-1000)
    offset = g.op(unreal.MaterialExpressionAdd, jitter, roll2, x=-900)
    frac = g.node(unreal.MaterialExpressionFrac, -800)
    g.link(offset, '', frac, 'Input')
    tc = g.node(unreal.MaterialExpressionTextureCoordinate, -900)
    uv = g.op(unreal.MaterialExpressionAdd, tc, frac, x=-700)
    tex = g.texture('ScreenMap', 'T_TV_Static', unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -600)
    g.link(uv, '', tex, 'UVs')
    # Scanlines.
    v = g.node(unreal.MaterialExpressionComponentMask, -800, r=False, g=True, b=False, a=False)
    g.link(tc, '', v, 'Input')
    lines = g.op(unreal.MaterialExpressionMultiply, v, None, x=-700)
    lines.set_editor_property('const_b', 700.0)
    sine = g.node(unreal.MaterialExpressionSine, -600)
    g.link(lines, '', sine, 'Input')
    scan = g.op(unreal.MaterialExpressionMultiply, sine, None, x=-500)
    scan.set_editor_property('const_b', 0.12)
    scan = g.op(unreal.MaterialExpressionAdd, scan, None, x=-400)
    scan.set_editor_property('const_b', 0.88)
    em = g.op(unreal.MaterialExpressionMultiply, tex, g.vector('ScreenColor', (0.7, 0.8, 0.95), -600), a_pin='RGB',
              x=-400)
    em = g.op(unreal.MaterialExpressionMultiply, em, g.scalar('EmissiveStrength', 3.0, -500), x=-300)
    em = g.op(unreal.MaterialExpressionMultiply, em, scan, x=-200)
    g.out(em, '', MP.MP_EMISSIVE_COLOR)
    g.out(g.const3((0.02, 0.02, 0.02)), '', MP.MP_BASE_COLOR)
    g.out(g.const(0.15), '', MP.MP_ROUGHNESS)
    return finish(mat)


def master_decal():
    mat, g = new_master('M_HH_Decal', {'material_domain': unreal.MaterialDomain.MD_DEFERRED_DECAL,
                                       'blend_mode': unreal.BlendMode.BLEND_TRANSLUCENT})
    mat.set_editor_property('used_with_skeletal_mesh', False)
    tex = g.texture('DecalMap', 'T_Decal_OilStain', unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    base = g.op(unreal.MaterialExpressionMultiply, tex, g.vector('Tint', (1, 1, 1)), a_pin='RGB')
    g.out(base, '', MP.MP_BASE_COLOR)
    opacity = g.op(unreal.MaterialExpressionMultiply, tex, g.scalar('Opacity', 1.0), a_pin='A')
    g.out(opacity, '', MP.MP_OPACITY)
    g.out(g.scalar('Roughness', 0.6), '', MP.MP_ROUGHNESS)
    return finish(mat)


def master_highlight():
    """Overlay material for interactables in focus: a warm fresnel rim."""
    mat, g = new_master('M_HH_Highlight', {'blend_mode': unreal.BlendMode.BLEND_TRANSLUCENT,
                                           'shading_model': unreal.MaterialShadingModel.MSM_UNLIT})
    fres = g.node(unreal.MaterialExpressionFresnel, -700, exponent=2.5, base_reflect_fraction=0.04)
    em = g.op(unreal.MaterialExpressionMultiply, g.vector('HighlightColor', (1.0, 0.72, 0.38)), fres)
    em = g.op(unreal.MaterialExpressionMultiply, em, g.scalar('Intensity', 2.5))
    g.out(em, '', MP.MP_EMISSIVE_COLOR)
    op = g.op(unreal.MaterialExpressionMultiply, fres, g.scalar('RimOpacity', 0.55))
    op = g.op(unreal.MaterialExpressionAdd, op, g.scalar('BaseOpacity', 0.05))
    g.out(op, '', MP.MP_OPACITY)
    return finish(mat)


MASTERS = {}


def build_masters():
    U.ensure_dir(MASTER_DIR)
    MASTERS.update({
        'Surface': master_surface(), 'Character': master_character(), 'Solid': master_solid(),
        'Emissive': master_emissive(), 'Glass': master_glass(), 'Image': master_image(False),
        'ImageMask': master_image(True), 'Screen': master_screen(), 'Decal': master_decal(),
    })
    master_highlight()


# ---------------------------------------------------------------------------------------------
# Physical materials

SURFACES = {'PM_Concrete': 1, 'PM_Wood': 2, 'PM_Metal': 3, 'PM_Fabric': 4, 'PM_Water': 5, 'PM_Gravel': 6}


def build_physical():
    U.ensure_dir(PHYS_DIR)
    out = {}
    for name, idx in SURFACES.items():
        pm = U.create_or_load(name, PHYS_DIR, unreal.PhysicalMaterial, unreal.PhysicalMaterialFactoryNew())
        try:
            pm.set_editor_property('surface_type', getattr(unreal.PhysicalSurface, f'SURFACE_TYPE{idx}'))
        except Exception as exc:  # noqa: BLE001
            U.warn(f'{name}: {exc}')
        out[name] = pm
    return out


def surface_for(name):
    n = name.lower()
    if any(k in n for k in ('concrete', 'block', 'brick', 'plaster', 'asphalt')):
        return 'PM_Concrete'
    if any(k in n for k in ('wood', 'pegboard', 'cork', 'cardboard')):
        return 'PM_Wood'
    if any(k in n for k in ('metal', 'steel', 'chrome', 'van_paint', 'chainlink', 'aluminium', 'brass', 'copper')):
        return 'PM_Metal'
    if any(k in n for k in ('fabric', 'leather', 'nylon', 'rubber', 'tire', 'rug')):
        return 'PM_Fabric'
    return None


# ---------------------------------------------------------------------------------------------
# Instances

def _tex(name):
    return U.find(name, required=True)


def build_instances():
    phys = build_physical()
    U.ensure_dir(INSTANCE_DIR)
    factory = unreal.MaterialInstanceConstantFactoryNew()
    for name, spec in sorted(catalog().items()):
        master = MASTERS.get(spec['master'])
        if master is None:
            U.warn(f'{name}: unknown master {spec["master"]}')
            continue
        mi = U.create_or_load(name, INSTANCE_DIR, unreal.MaterialInstanceConstant, factory)
        U.MEL.set_material_instance_parent(mi, master)
        try:
            _params(mi, spec)
        except Exception as exc:  # noqa: BLE001
            U.warn(f'{name}: {exc}')
        pm = surface_for(name)
        if pm:
            try:
                mi.set_editor_property('phys_material', phys[pm])
            except Exception as exc:  # noqa: BLE001
                U.warn(f'{name} phys: {exc}')
        U.MEL.update_material_instance(mi)


def _params(mi, spec):
    sc = U.MEL.set_material_instance_scalar_parameter_value
    vc = U.MEL.set_material_instance_vector_parameter_value
    tx = U.MEL.set_material_instance_texture_parameter_value
    m = spec['master']
    if m in ('Surface', 'Character'):
        for param, suffix in (('BaseColorMap', '_BC'), ('NormalMap', '_N'), ('ORMMap', '_ORM')):
            tex = _tex(spec['tex'] + suffix)
            if tex is not None:
                tx(mi, param, tex)
        sc(mi, 'UVScale', float(spec['uv']))
        sc(mi, 'RoughnessScale', float(spec.get('rough', 1.0)))
        sc(mi, 'MetallicScale', float(spec.get('metal', 1.0)))
        if m == 'Surface':
            vc(mi, 'Tint', U.colour(spec['tint']))
            sc(mi, 'RoughnessOffset', float(spec.get('rough_add', 0.0)))
            sc(mi, 'NormalStrength', float(spec.get('normal', 1.0)))
        else:
            if spec.get('skin'):
                sc(mi, 'UseSkinTint', 1.0)
                vc(mi, 'SkinTint', U.colour(spec['tint']))
            else:
                vc(mi, 'Tint', U.colour(spec['tint']))
            sc(mi, 'UseTint2', 1.0 if spec.get('use_tint2') else 0.0)
    elif m == 'Solid':
        vc(mi, 'BaseColor', U.colour(spec['color']))
        sc(mi, 'Roughness', float(spec['rough']))
        sc(mi, 'Metallic', float(spec['metal']))
    elif m == 'Emissive':
        vc(mi, 'BaseColor', U.colour(spec['color']))
        vc(mi, 'EmissiveColor', U.colour(spec['emissive']))
        sc(mi, 'EmissiveStrength', float(spec['strength']))
        sc(mi, 'Roughness', float(spec['rough']))
    elif m == 'Glass':
        vc(mi, 'Tint', U.colour(spec['tint']))
        sc(mi, 'Opacity', float(spec['opacity']))
        sc(mi, 'Roughness', float(spec['rough']))
    elif m in ('Image', 'ImageMask'):
        tex = _tex(spec['image'])
        if tex is not None:
            tx(mi, 'ImageMap', tex)
        sc(mi, 'Roughness', float(spec['rough']))
    elif m == 'Screen':
        tex = _tex(spec['image'])
        if tex is not None:
            tx(mi, 'ScreenMap', tex)
        vc(mi, 'ScreenColor', U.colour(spec['emissive']))
        sc(mi, 'EmissiveStrength', float(spec['strength']))
        sc(mi, 'ScrollSpeed', float(spec['scroll']))
    elif m == 'Decal':
        tex = _tex(spec['image'])
        if tex is not None:
            tx(mi, 'DecalMap', tex)
        vc(mi, 'Tint', U.colour(spec['tint']))
        sc(mi, 'Opacity', float(spec['opacity']))
        sc(mi, 'Roughness', float(spec['rough']))


def build_all():
    build_masters()
    build_instances()
    U.refresh_index()
