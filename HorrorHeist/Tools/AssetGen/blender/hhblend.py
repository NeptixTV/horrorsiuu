"""
Shared helpers for the Blender asset generators (run with the 'bpy' Python module or inside
Blender 4.2+). Geometry is built procedurally with bmesh; every mesh is exported to FBX for
Unreal and its placement is written to a layout JSON in Unreal coordinates.

Conventions
  * Props are authored in metres, characters in centimetres (unit scale 0.01).
  * Blender (x, y, z) maps to Unreal (x, -y, z) with the default FBX axes (-Z forward, Y up).
  * UVs are box-projected in metres; materials scale them (see hhmaterials.py).
"""
import json
import math
import os

import bmesh
import bpy
from mathutils import Matrix, Quaternion, Vector

from hhmaterials import CATALOG

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
TEX_DIR = os.path.join(PROJECT, 'SourceArt', 'Textures')
MESH_DIR = os.path.join(PROJECT, 'SourceArt', 'Meshes')

MIRROR = Matrix.Diagonal((1.0, -1.0, 1.0))
# Unreal cameras/spot/rect lights look down +X with +Z up; Blender's look down -Z with +Y up.
CAMERA_BASIS = Matrix(((0, 1, 0), (0, 0, -1), (-1, 0, 0)))


# =======================================================================================
# Scene

def reset(unit_scale=1.0):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = unit_scale
    if unit_scale != 1.0:
        scene.unit_settings.length_unit = 'CENTIMETERS'
    return scene


def link(obj, collection=None):
    (collection or bpy.context.scene.collection).objects.link(obj)
    return obj


def collection(name):
    if name in bpy.data.collections:
        return bpy.data.collections[name]
    col = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(col)
    return col


def deselect_all():
    for obj in bpy.context.view_layer.objects:
        obj.select_set(False)


# =======================================================================================
# Preview materials (Cycles); Unreal builds its own from the same catalogue

def _image(path, non_color=False):
    img = bpy.data.images.load(path, check_existing=True)
    if non_color:
        img.colorspace_settings.name = 'Non-Color'
    return img


def _tex_path(name, suffix=''):
    for folder in ('Materials', 'Unique', 'Decals', 'Generated'):
        for ext in ('.jpg', '.png'):
            path = os.path.join(TEX_DIR, folder, name + suffix + ext)
            if os.path.exists(path):
                return path
    return None


def material(name):
    if name in bpy.data.materials:
        return bpy.data.materials[name]
    spec = CATALOG[name]
    mat = bpy.data.materials.new(name)
    if mat.node_tree is None:
        mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new('ShaderNodeOutputMaterial')
    bsdf = nt.nodes.new('ShaderNodeBsdfPrincipled')
    nt.links.new(bsdf.outputs['BSDF'], out.inputs['Surface'])
    master = spec['master']

    def uv_node(scale):
        coord = nt.nodes.new('ShaderNodeTexCoord')
        mapping = nt.nodes.new('ShaderNodeMapping')
        mapping.inputs['Scale'].default_value = (scale, scale, 1.0)
        nt.links.new(coord.outputs['UV'], mapping.inputs['Vector'])
        return mapping.outputs['Vector']

    def tex(path, vec, non_color=False):
        node = nt.nodes.new('ShaderNodeTexImage')
        node.image = _image(path, non_color)
        nt.links.new(vec, node.inputs['Vector'])
        return node

    if master in ('Surface', 'Character'):
        vec = uv_node(spec['uv'])
        base = _tex_path(spec['tex'], '_BC')
        if base:
            bc = tex(base, vec)
            tint = spec['tint']
            mul = nt.nodes.new('ShaderNodeVectorMath')
            mul.operation = 'MULTIPLY'
            nt.links.new(bc.outputs['Color'], mul.inputs[0])
            mul.inputs[1].default_value = tint
            nt.links.new(mul.outputs[0], bsdf.inputs['Base Color'])
            orm = tex(_tex_path(spec['tex'], '_ORM'), vec, True)
            sep = nt.nodes.new('ShaderNodeSeparateColor')
            nt.links.new(orm.outputs['Color'], sep.inputs['Color'])
            rough = nt.nodes.new('ShaderNodeMath')
            rough.operation = 'MULTIPLY_ADD'
            rough.use_clamp = True
            nt.links.new(sep.outputs['Green'], rough.inputs[0])
            rough.inputs[1].default_value = spec.get('rough', 1.0)
            rough.inputs[2].default_value = spec.get('rough_add', 0.0)
            nt.links.new(rough.outputs['Value'], bsdf.inputs['Roughness'])
            metal = nt.nodes.new('ShaderNodeMath')
            metal.operation = 'MULTIPLY'
            nt.links.new(sep.outputs['Blue'], metal.inputs[0])
            metal.inputs[1].default_value = spec.get('metal', 1.0)
            nt.links.new(metal.outputs['Value'], bsdf.inputs['Metallic'])
            # DirectX normal -> OpenGL for Blender: flip green.
            nrm = tex(_tex_path(spec['tex'], '_N'), vec, True)
            sep_n = nt.nodes.new('ShaderNodeSeparateColor')
            nt.links.new(nrm.outputs['Color'], sep_n.inputs['Color'])
            flip = nt.nodes.new('ShaderNodeMath')
            flip.operation = 'SUBTRACT'
            flip.inputs[0].default_value = 1.0
            nt.links.new(sep_n.outputs['Green'], flip.inputs[1])
            comb = nt.nodes.new('ShaderNodeCombineColor')
            nt.links.new(sep_n.outputs['Red'], comb.inputs['Red'])
            nt.links.new(flip.outputs['Value'], comb.inputs['Green'])
            nt.links.new(sep_n.outputs['Blue'], comb.inputs['Blue'])
            nmap = nt.nodes.new('ShaderNodeNormalMap')
            nmap.inputs['Strength'].default_value = spec.get('normal', 1.0)
            nt.links.new(comb.outputs['Color'], nmap.inputs['Color'])
            nt.links.new(nmap.outputs['Normal'], bsdf.inputs['Normal'])
        else:
            bsdf.inputs['Base Color'].default_value = (*spec['tint'], 1.0)
    elif master == 'Solid':
        bsdf.inputs['Base Color'].default_value = (*spec['color'], 1.0)
        bsdf.inputs['Roughness'].default_value = spec['rough']
        bsdf.inputs['Metallic'].default_value = spec['metal']
    elif master == 'Emissive':
        bsdf.inputs['Base Color'].default_value = (*spec['color'], 1.0)
        bsdf.inputs['Emission Color'].default_value = (*spec['emissive'], 1.0)
        bsdf.inputs['Emission Strength'].default_value = spec['strength'] * 0.6
        bsdf.inputs['Roughness'].default_value = spec['rough']
    elif master == 'Glass':
        bsdf.inputs['Base Color'].default_value = (*spec['tint'], 1.0)
        bsdf.inputs['Roughness'].default_value = spec['rough']
        bsdf.inputs['Alpha'].default_value = max(0.05, spec['opacity'])
    elif master == 'Screen':
        path = _tex_path(spec['image'])
        node = tex(path, uv_node(1.0))
        mul = nt.nodes.new('ShaderNodeVectorMath')
        mul.operation = 'MULTIPLY'
        nt.links.new(node.outputs['Color'], mul.inputs[0])
        mul.inputs[1].default_value = spec['emissive']
        nt.links.new(mul.outputs[0], bsdf.inputs['Emission Color'])
        bsdf.inputs['Emission Strength'].default_value = spec['strength']
        bsdf.inputs['Base Color'].default_value = (0.02, 0.02, 0.02, 1.0)
        bsdf.inputs['Roughness'].default_value = 0.15
    elif master in ('Image', 'ImageMask', 'Decal'):
        path = _tex_path(spec['image'])
        node = tex(path, uv_node(1.0))
        nt.links.new(node.outputs['Color'], bsdf.inputs['Base Color'])
        bsdf.inputs['Roughness'].default_value = spec.get('rough', 0.8)
        if master != 'Image':
            nt.links.new(node.outputs['Alpha'], bsdf.inputs['Alpha'])
    return mat


# =======================================================================================
# Geometry builder

def _mat4(loc=(0, 0, 0), rot=(0, 0, 0), scale=(1, 1, 1)):
    """rot in degrees (XYZ euler)."""
    from mathutils import Euler
    r = Euler([math.radians(a) for a in rot], 'XYZ').to_matrix().to_4x4()
    s = Matrix.Diagonal((*scale, 1.0))
    return Matrix.Translation(loc) @ r @ s


def _box_uv(faces, uv_layer, rotate=False, offset=(0.0, 0.0)):
    for f in faces:
        n = f.normal
        ax = max(range(3), key=lambda i: abs(n[i]))
        for loop in f.loops:
            co = loop.vert.co
            if ax == 2:
                u, v = co.x * (1 if n.z >= 0 else -1), co.y
            elif ax == 0:
                u, v = co.y * (1 if n.x >= 0 else -1), co.z
            else:
                u, v = -co.x * (1 if n.y >= 0 else -1), co.z
            if rotate:
                u, v = v, u
            loop[uv_layer].uv = (u + offset[0], v + offset[1])


def _cyl_uv(faces, uv_layer, axis_vec, center, radius):
    """Cylindrical mapping for side faces, planar for caps."""
    axis = Vector(axis_vec).normalized()
    ref = Vector((1, 0, 0)) if abs(axis.x) < 0.9 else Vector((0, 1, 0))
    t1 = (ref - axis * ref.dot(axis)).normalized()
    t2 = axis.cross(t1)
    c = Vector(center)
    for f in faces:
        if abs(f.normal.dot(axis)) > 0.7:
            for loop in f.loops:
                d = loop.vert.co - c
                loop[uv_layer].uv = (d.dot(t1), d.dot(t2))
            continue
        fc = f.calc_center_median() - c
        a0 = math.atan2(fc.dot(t2), fc.dot(t1))
        for loop in f.loops:
            d = loop.vert.co - c
            a = math.atan2(d.dot(t2), d.dot(t1))
            while a - a0 > math.pi:
                a -= 2 * math.pi
            while a - a0 < -math.pi:
                a += 2 * math.pi
            loop[uv_layer].uv = (a * radius, d.dot(axis))


class Builder:
    """Accumulates primitives (each with its own material slot) into one mesh."""

    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new('UVMap')
        self.mats = []

    # -- internals ---------------------------------------------------------------------
    def _slot(self, mat):
        if mat not in self.mats:
            self.mats.append(mat)
        return self.mats.index(mat)

    def _begin(self):
        pb = bmesh.new()
        uv = pb.loops.layers.uv.new('UVMap')
        return pb, uv

    def _commit(self, pb, mat, smooth=True):
        idx = self._slot(mat)
        for f in pb.faces:
            f.material_index = idx
            f.smooth = smooth
        tmp = bpy.data.meshes.new('_tmp')
        pb.to_mesh(tmp)
        pb.free()
        self.bm.from_mesh(tmp)
        bpy.data.meshes.remove(tmp)

    @staticmethod
    def _bevel(pb, width, segments=2):
        if width <= 0:
            return
        bmesh.ops.bevel(pb, geom=list(pb.edges) + list(pb.verts), offset=width, offset_type='OFFSET',
                        segments=segments, profile=0.5, affect='EDGES', clamp_overlap=True)

    # -- primitives --------------------------------------------------------------------
    def box(self, size, loc=(0, 0, 0), mat='MI_Plastic_Grey', rot=(0, 0, 0), bevel=0.0, segs=2,
            uv_rotate=False, uv_offset=(0, 0)):
        pb, uv = self._begin()
        bmesh.ops.create_cube(pb, size=1.0, matrix=_mat4(loc, rot, size))
        self._bevel(pb, bevel, segs)
        pb.normal_update()
        _box_uv(pb.faces, uv, uv_rotate, uv_offset)
        self._commit(pb, mat)
        return self

    def box_minmax(self, lo, hi, mat='MI_Plastic_Grey', bevel=0.0, **kw):
        size = [hi[i] - lo[i] for i in range(3)]
        loc = [(hi[i] + lo[i]) * 0.5 for i in range(3)]
        return self.box(size, loc, mat, bevel=bevel, **kw)

    def cyl(self, radius, depth, loc=(0, 0, 0), mat='MI_Plastic_Grey', rot=(0, 0, 0), segs=16,
            radius2=None, caps=True):
        pb, uv = self._begin()
        m = _mat4(loc, rot)
        bmesh.ops.create_cone(pb, cap_ends=caps, cap_tris=False, segments=segs, radius1=radius,
                              radius2=radius if radius2 is None else radius2, depth=depth, matrix=m)
        pb.normal_update()
        axis = m.to_3x3() @ Vector((0, 0, 1))
        _cyl_uv(pb.faces, uv, axis, m.to_translation(), max(radius, radius2 or 0))
        self._commit(pb, mat)
        return self

    def sphere(self, radius, loc=(0, 0, 0), mat='MI_Plastic_Grey', scale=(1, 1, 1), rot=(0, 0, 0),
               segs=16, rings=10):
        pb, uv = self._begin()
        bmesh.ops.create_uvsphere(pb, u_segments=segs, v_segments=rings, radius=radius,
                                  matrix=_mat4(loc, rot, scale))
        pb.normal_update()
        _box_uv(pb.faces, uv)
        self._commit(pb, mat)
        return self

    def torus(self, major, minor, loc=(0, 0, 0), mat='MI_Plastic_Grey', rot=(0, 0, 0), segs=24, rsegs=10,
              arc=360.0, scale=(1, 1, 1)):
        """Torus in the local XY plane; 'arc' < 360 makes an open handle (capped)."""
        pb, uv = self._begin()
        m = _mat4(loc, rot, scale)
        closed = arc >= 359.9
        n = segs if closed else segs + 1
        rings = []
        for i in range(n):
            a = math.radians(arc) * i / segs
            centre = Vector((major * math.cos(a), major * math.sin(a), 0))
            out = Vector((math.cos(a), math.sin(a), 0))
            rings.append([pb.verts.new(m @ (centre + out * minor * math.cos(b) + Vector((0, 0, minor * math.sin(b)))))
                          for b in (2 * math.pi * k / rsegs for k in range(rsegs))])
        for i in range(n if closed else n - 1):
            a, b = rings[i], rings[(i + 1) % n]
            for k in range(rsegs):
                k2 = (k + 1) % rsegs
                f = pb.faces.new((a[k], b[k], b[k2], a[k2]))
                for loop, (uu, vv) in zip(f.loops, ((i, k), (i + 1, k), (i + 1, k + 1), (i, k + 1))):
                    loop[uv].uv = (uu / segs * math.radians(arc) * major, vv / rsegs * 2 * math.pi * minor)
        if not closed:
            pb.faces.new(list(reversed(rings[0])))
            pb.faces.new(rings[-1])
        bmesh.ops.recalc_face_normals(pb, faces=pb.faces)
        self._commit(pb, mat)
        return self

    def lathe(self, profile, loc=(0, 0, 0), mat='MI_Plastic_Grey', rot=(0, 0, 0), segs=24, scale=(1, 1, 1),
              closed=False):
        """profile: [(radius, z), ...]. Radius 0 closes an end; 'closed' joins the last point back to the
        first (a shell such as a lamp shade) instead of capping the ends."""
        pb, uv = self._begin()
        m = _mat4(loc, rot, scale)
        rings = []
        for r, z in profile:
            if r < 1e-6:
                rings.append([pb.verts.new(m @ Vector((0, 0, z)))])
            else:
                rings.append([pb.verts.new(m @ Vector((r * math.cos(a), r * math.sin(a), z)))
                              for a in (2 * math.pi * i / segs for i in range(segs))])
        pairs = list(zip(rings, rings[1:]))
        if closed:
            pairs.append((rings[-1], rings[0]))
        for a, b in pairs:
            if len(a) == 1 and len(b) == 1:
                continue
            for i in range(segs):
                j = (i + 1) % segs
                if len(a) == 1:
                    pb.faces.new((a[0], b[i], b[j]))
                elif len(b) == 1:
                    pb.faces.new((a[i], b[0], a[j]))
                else:
                    pb.faces.new((a[i], a[j], b[j], b[i]))
        if len(rings[0]) > 1 and not closed:
            pb.faces.new(list(reversed(rings[0])))
        if len(rings[-1]) > 1 and not closed:
            pb.faces.new(rings[-1])
        bmesh.ops.recalc_face_normals(pb, faces=pb.faces)
        pb.normal_update()
        radius = max(p[0] for p in profile) * max(scale)
        _cyl_uv(pb.faces, uv, m.to_3x3() @ Vector((0, 0, 1)), m.to_translation(), radius)
        self._commit(pb, mat)
        return self

    def tube(self, points, radius, mat='MI_Steel', segs=12, bend=0.0, caps=True):
        """Sweep a circle along a polyline. 'bend' rounds the corners with that radius."""
        pts = [Vector(p) for p in points]
        if bend > 0 and len(pts) > 2:
            rounded = [pts[0]]
            for a, b, c in zip(pts, pts[1:], pts[2:]):
                d1 = (b - a).normalized()
                d2 = (c - b).normalized()
                cut = min(bend, (b - a).length * 0.45, (c - b).length * 0.45)
                p0 = b - d1 * cut
                p1 = b + d2 * cut
                for k in range(7):
                    t = k / 6
                    q = (1 - t) ** 2 * p0 + 2 * (1 - t) * t * b + t * t * p1
                    rounded.append(q)
            rounded.append(pts[-1])
            pts = rounded
        pb, uv = self._begin()
        rings = []
        prev_n = None
        for i, p in enumerate(pts):
            if i == 0:
                t = (pts[1] - p).normalized()
            elif i == len(pts) - 1:
                t = (p - pts[i - 1]).normalized()
            else:
                t = ((p - pts[i - 1]).normalized() + (pts[i + 1] - p).normalized()).normalized()
            if prev_n is None:
                ref = Vector((0, 0, 1)) if abs(t.z) < 0.9 else Vector((1, 0, 0))
                n = (ref - t * ref.dot(t)).normalized()
            else:
                n = (prev_n - t * prev_n.dot(t)).normalized()
            prev_n = n
            bn = t.cross(n)
            rings.append([pb.verts.new(p + (n * math.cos(a) + bn * math.sin(a)) * radius)
                          for a in (2 * math.pi * k / segs for k in range(segs))])
        dist = [0.0]
        for a, b in zip(pts, pts[1:]):
            dist.append(dist[-1] + (b - a).length)
        for ri in range(len(rings) - 1):
            for k in range(segs):
                k2 = (k + 1) % segs
                f = pb.faces.new((rings[ri][k], rings[ri][k2], rings[ri + 1][k2], rings[ri + 1][k]))
                for loop, (kk, rr) in zip(f.loops, ((k, ri), (k + 1, ri), (k + 1, ri + 1), (k, ri + 1))):
                    loop[uv].uv = (kk / segs * 2 * math.pi * radius, dist[rr])
        if caps:
            pb.faces.new(list(reversed(rings[0])))
            pb.faces.new(rings[-1])
        bmesh.ops.recalc_face_normals(pb, faces=pb.faces)
        self._commit(pb, mat)
        return self

    def prism(self, poly, depth, loc=(0, 0, 0), mat='MI_Plastic_Grey', rot=(0, 0, 0), bevel=0.0, axis='Y'):
        """Extrude a 2D polygon (in the plane perpendicular to 'axis') by depth, centred."""
        pb, uv = self._begin()
        m = _mat4(loc, rot)
        if axis == 'Y':
            to3 = lambda a, b, d: Vector((a, d, b))      # noqa: E731
        elif axis == 'X':
            to3 = lambda a, b, d: Vector((d, a, b))      # noqa: E731
        else:
            to3 = lambda a, b, d: Vector((a, b, d))      # noqa: E731
        front = [pb.verts.new(m @ to3(a, b, -depth / 2)) for a, b in poly]
        back = [pb.verts.new(m @ to3(a, b, depth / 2)) for a, b in poly]
        caps = [pb.faces.new(front), pb.faces.new(list(reversed(back)))]
        n = len(poly)
        for i in range(n):
            j = (i + 1) % n
            pb.faces.new((front[i], front[j], back[j], back[i]))
        bmesh.ops.recalc_face_normals(pb, faces=pb.faces)
        self._bevel(pb, bevel)
        ngons = [f for f in pb.faces if len(f.verts) > 4]
        if ngons:
            bmesh.ops.triangulate(pb, faces=ngons, quad_method='BEAUTY', ngon_method='EAR_CLIP')
        pb.normal_update()
        _box_uv(pb.faces, uv)
        self._commit(pb, mat)
        return self

    def plane(self, w, h, loc=(0, 0, 0), mat='MI_Paper', rot=(0, 0, 0), uv_rect=(0, 0, 1, 1), two_sided=False):
        """A w x h quad in the local XZ plane facing -Y (towards a viewer in front of a wall
        that runs along X). uv_rect maps (u0, v0, u1, v1) across it."""
        pb, uv = self._begin()
        m = _mat4(loc, rot)
        corners = [(-w / 2, -h / 2), (w / 2, -h / 2), (w / 2, h / 2), (-w / 2, h / 2)]
        verts = [pb.verts.new(m @ Vector((x, 0, z))) for x, z in corners]
        f = pb.faces.new(verts)
        u0, v0, u1, v1 = uv_rect
        for loop, (uu, vv) in zip(f.loops, ((u0, v0), (u1, v0), (u1, v1), (u0, v1))):
            loop[uv].uv = (uu, vv)
        if two_sided:
            back = pb.faces.new(list(reversed(verts)))
            for loop in back.loops:
                idx = verts.index(loop.vert)
                loop[uv].uv = ((u0, v0), (u1, v0), (u1, v1), (u0, v1))[idx]
        self._commit(pb, mat, smooth=False)
        return self

    def grid_plane(self, w, h, loc, mat, rot=(0, 0, 0), sag=0.0, nx=8, ny=8, uv_scale=1.0):
        """Subdivided horizontal (XY) plane with optional centre sag (cushions, cloth)."""
        pb, uv = self._begin()
        m = _mat4(loc, rot)
        verts = []
        for j in range(ny + 1):
            row = []
            for i in range(nx + 1):
                x = (i / nx - 0.5) * w
                y = (j / ny - 0.5) * h
                z = sag * math.sin(math.pi * i / nx) * math.sin(math.pi * j / ny)
                row.append(pb.verts.new(m @ Vector((x, y, z))))
            verts.append(row)
        for j in range(ny):
            for i in range(nx):
                f = pb.faces.new((verts[j][i], verts[j][i + 1], verts[j + 1][i + 1], verts[j + 1][i]))
                for loop, (a, b) in zip(f.loops, ((i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1))):
                    loop[uv].uv = (a / nx * w * uv_scale, b / ny * h * uv_scale)
        self._commit(pb, mat)
        return self

    def merge(self, other, matrix=None):
        """Append another builder's geometry (keeps its materials)."""
        tmp = bpy.data.meshes.new('_tmp')
        other.bm.to_mesh(tmp)
        remap = [self._slot(m) for m in other.mats]
        pb = bmesh.new()
        pb.from_mesh(tmp)
        if matrix is not None:
            bmesh.ops.transform(pb, matrix=matrix, verts=pb.verts)
        for f in pb.faces:
            f.material_index = remap[f.material_index] if f.material_index < len(remap) else 0
        pb.to_mesh(tmp)
        pb.free()
        self.bm.from_mesh(tmp)
        bpy.data.meshes.remove(tmp)
        return self

    # -- output ------------------------------------------------------------------------
    def mesh(self, sharp_angle=40.0):
        me = bpy.data.meshes.new(self.name)
        self.bm.normal_update()
        self.bm.to_mesh(me)
        self.bm.free()
        for name in self.mats:
            me.materials.append(material(name))
        me.set_sharp_from_angle(angle=math.radians(sharp_angle))
        return me

    def object(self, sharp_angle=40.0, weighted=True):
        me = self.mesh(sharp_angle)
        return mesh_object(me, weighted)


def mesh_object(me, weighted=True, name=None):
    obj = bpy.data.objects.new(name or me.name, me)
    if weighted:
        mod = obj.modifiers.new('WeightedNormal', 'WEIGHTED_NORMAL')
        mod.keep_sharp = True
        mod.weight = 50
    return obj


# =======================================================================================
# Export

FBX_COMMON = dict(
    axis_forward='-Z', axis_up='Y', apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE',
    use_mesh_modifiers=True, mesh_smooth_type='FACE', add_leaf_bones=False, use_custom_props=False,
    embed_textures=False, path_mode='AUTO', use_tspace=False, use_triangles=False,
)


def export_mesh_object(obj, path):
    """Export one mesh object at the origin (its transform is ignored)."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    deselect_all()
    saved = obj.matrix_world.copy()
    parent = obj.parent
    obj.parent = None
    obj.matrix_world = Matrix.Identity(4)
    was_hidden = obj.hide_get()
    obj.hide_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={'MESH'}, bake_anim=False,
                             **FBX_COMMON)
    obj.parent = parent
    obj.matrix_world = saved
    obj.hide_set(was_hidden)
    obj.select_set(False)


class MeshManifest:
    """Records every exported mesh for the Unreal importer."""

    def __init__(self, path):
        self.path = path
        self.data = {}
        if os.path.exists(path):
            with open(path, 'r', encoding='utf-8') as fh:
                self.data = json.load(fh)

    def add(self, name, rel_file, materials, folder, collision='box', nanite=False, kind='static', extra=None):
        entry = dict(file=rel_file, materials=list(materials), folder=folder, collision=collision,
                     nanite=nanite, kind=kind)
        if extra:
            entry.update(extra)
        self.data[name] = entry

    def save(self):
        os.makedirs(os.path.dirname(self.path), exist_ok=True)
        with open(self.path, 'w', encoding='utf-8') as fh:
            json.dump(self.data, fh, indent=1, sort_keys=True)


# =======================================================================================
# Unreal transforms

def to_ue_location(v, unit=100.0):
    return [round(v.x * unit, 3), round(-v.y * unit, 3), round(v.z * unit, 3)]


def to_ue_quat(rot3, camera_like=False):
    """Mirror a Blender rotation into Unreal space. Returns [x, y, z, w]."""
    m = MIRROR @ rot3 @ MIRROR
    if camera_like:
        m = m @ CAMERA_BASIS
    q = m.to_quaternion().normalized()
    return [round(q.x, 6), round(q.y, 6), round(q.z, 6), round(q.w, 6)]


def ue_transform(obj, camera_like=False):
    loc, rot, scale = obj.matrix_world.decompose()
    return dict(location=to_ue_location(loc), quat=to_ue_quat(rot.to_matrix(), camera_like),
                scale=[round(scale.x, 4), round(scale.y, 4), round(scale.z, 4)])


def ue_vector(v, unit=100.0):
    """Blender local offset (metres) -> Unreal asset-space offset (cm)."""
    return to_ue_location(Vector(v), unit)
