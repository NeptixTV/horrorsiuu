"""Placement helpers shared by the world builders. They write into build_hideout's layout list
(BH.ACTORS), which build_world.py swaps per level, and its mesh registry (BH.MESHES)."""
import math

from mathutils import Vector

import build_hideout as BH
import hhblend as H
import houses as HS
import props as P
import worldkit as K

MISSING = set()


def has(name):
    return name in P.PROPS or name in BH.MESHES


def static_m(name, matrix, collection='Town', tags=None, shadow=True):
    """StaticMeshActor with an arbitrary world matrix."""
    if not has(name):
        MISSING.add(name)
        return None
    obj = BH.obj_from_mesh(name, matrix, collection)
    entry = dict(name=obj.name, cls='StaticMeshActor', folder=collection, mesh=name, **H.ue_transform(obj))
    if tags:
        entry['tags'] = list(tags)
    if not shadow:
        entry['cast_shadow'] = False
    BH.ACTORS.append(entry)
    return obj


def place(name, loc=(0, 0, 0), rot=0, scale=1.0, M=None, collection='Town', tags=None, shadow=True):
    m = K.mat(loc, rot, scale)
    if M is not None:
        m = M @ m
    return static_m(name, m, collection, tags, shadow)


def unique(builder, folder='Town', collision='complex', nanite=False):
    return BH.register_unique(builder, folder, collision, nanite)


def finish(bld, collection='Town', tags=None, nanite=True):
    """Register and place a Building's shell and trim meshes plus its lit-window cards."""
    shell = unique(bld.shell, collision='complex', nanite=nanite)
    trim = unique(bld.trim, collision='complex', nanite=False)
    static_m(shell, bld.M, collection, tags)
    static_m(trim, bld.M, collection, tags)
    HS.place_glows(bld, static_m)


def yaw_of(M):
    return math.degrees(M.to_euler('XYZ').z)


def world(M, loc):
    return tuple(M @ Vector(loc))


def practical_m(M, kind, fixture, loc, rot=0.0, **kw):
    """BH.practical in a building's local frame (buildings only rotate about Z)."""
    r = rot if isinstance(rot, (tuple, list)) else (0, 0, rot)
    return BH.practical(kind, fixture, world(M, loc), (r[0], r[1], r[2] + yaw_of(M)), **kw)


def actor_m(cls, M, loc=(0, 0, 0), rot=0.0, **kw):
    return BH.actor(cls, M @ K.mat(loc, rot), **kw)


def door_actor(M, o, mesh, locked=False, house=None, tags=None, name=None, inside_positive_y=False, sounds='wood',
               **lock):
    """HHDoorActor in an opening created by House.door(). Lock options: bCanPick, bCanPry, bCanCutGlass,
    KeyId, PickSeconds, ... (see AHHDoorActor)."""
    props = {}
    if sounds == 'wood':
        props.update(OpenSound=BH.asset('S_Door_Wood_Open'), CloseSound=BH.asset('S_Door_Wood_Close'),
                     CreakSound=BH.asset('S_Door_Creak_Slow'))
    props.update(LockedSound=BH.asset('S_Door_Locked_Rattle_01'), UnlockSound=BH.asset('S_Lockpick_Success_01'),
                 PickLoopSound=BH.asset('S_Lockpick_Loop'), PrySound=BH.asset('S_Crowbar_Pry_01'),
                 GlassCutSound=BH.asset('S_Glass_Cut_01'), OpenAngle=95.0)
    props['bLocked'] = bool(locked)
    props['bInsideIsPositiveY'] = bool(inside_positive_y)
    if house:
        props['HouseTag'] = house
    props.update(lock)
    all_tags = list(tags or [])
    return BH.actor('HHDoorActor', M @ K.mat(o['hinge'], o['rot']), 'Gameplay', mesh=mesh,
                    mesh_component='DoorMesh', properties=props, tags=all_tags or None, name=name)
