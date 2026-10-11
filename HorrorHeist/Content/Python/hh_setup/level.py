"""Builds the maps from SourceArt/Layout/<map>.json (written by Tools/AssetGen/blender/build_world.py).

L_Town is a plain level that the persistent maps (L_Hideout and the job maps) stream in as an
always-loaded sub-level. Re-running is safe: only the actors this script placed (tag HH_Generated)
in the map being rebuilt are destroyed and placed again; anything added by hand is kept.
"""
import unreal

from . import util as U

MAPS_DIR = U.ROOT + '/Maps'
GENERATED = 'HH_Generated'
# Build order: the streamed town first, then the persistent maps that reference it.
ORDER = ('L_Town', 'L_Hideout', 'L_Job_Vance', 'L_Job_Marrow', 'L_Job_Rectory')
GAME_MODES = {'HHHeistGameMode': '/Script/HorrorHeist.HHHeistGameMode'}


def _subsystems():
    return (unreal.get_editor_subsystem(unreal.LevelEditorSubsystem),
            unreal.get_editor_subsystem(unreal.EditorActorSubsystem))


def _world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def open_or_create(map_path, rebuild):
    """Open (or create) a map and remove the actors a previous run generated in THIS map only
    (streamed sub-levels are loaded too and must not be touched)."""
    les, eas = _subsystems()
    U.ensure_dir(MAPS_DIR)
    if U.EAL.does_asset_exist(map_path):
        les.load_level(map_path)
        if rebuild:
            prefix = map_path + '.'
            removed = 0
            for actor in eas.get_all_level_actors():
                if actor.actor_has_tag(GENERATED) and actor.get_path_name().startswith(prefix):
                    eas.destroy_actor(actor)
                    removed += 1
            U.log(f'{map_path}: removed {removed} generated actors')
    else:
        if not les.new_level(map_path):
            raise RuntimeError('Could not create ' + map_path)
    return les, eas


def ensure_streaming(les, map_name, sublevels):
    """Add the sub-levels as always-loaded streaming levels (no-op if already there) and make the
    persistent level current again (adding a level makes the new one current)."""
    world = _world()
    for sub in sublevels:
        package = f'{MAPS_DIR}/{sub}'
        if not U.EAL.does_asset_exist(package):
            U.warn(f'{map_name}: sub-level {sub} does not exist (build order?)')
            continue
        try:
            unreal.EditorLevelUtils.add_level_to_world(world, package, unreal.LevelStreamingAlwaysLoaded)
        except Exception as exc:  # noqa: BLE001
            U.warn(f'{map_name}: could not stream {sub}: {exc}')
    try:
        les.set_current_level_by_name(map_name)
    except Exception as exc:  # noqa: BLE001
        U.warn(f'{map_name}: could not make the persistent level current: {exc}')


def set_game_mode(map_name, mode):
    path = GAME_MODES.get(mode)
    if not path:
        return
    klass = unreal.load_class(None, path)
    if klass is None:
        U.warn(f'{map_name}: game mode {path} not found (is the C++ module built?)')
        return
    try:
        settings = _world().get_world_settings()
        settings.set_editor_property('default_game_mode', klass)
    except Exception as exc:  # noqa: BLE001
        U.warn(f'{map_name}: could not set the game mode override: {exc}')


def _vec(v):
    return unreal.Vector(float(v[0]), float(v[1]), float(v[2]))


def _rot(q):
    return unreal.Quat(float(q[0]), float(q[1]), float(q[2]), float(q[3])).rotator()


def apply_component(comp, values):
    if comp is None:
        return
    try:
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    except Exception:  # noqa: BLE001
        pass
    items = list(values.items())
    items.sort(key=lambda kv: 0 if kv[0] == 'IntensityUnits' else 1)
    for key, value in items:
        try:
            if key == 'world':
                comp.set_world_location_and_rotation(_vec(value['location']), _rot(value['quat']), False, False)
            elif key == 'RelativeLocation':
                comp.set_relative_location(_vec(value), False, False)
            elif key == 'RelativeQuat':
                comp.set_relative_rotation(_rot(value), False, False)
            elif key == 'RelativeScale3D':
                comp.set_relative_scale3d(_vec(value))
            elif key == 'RelativeDirection':
                comp.set_relative_rotation(unreal.MathLibrary.make_rot_from_x(_vec(value)), False, False)
            elif key == 'StaticMesh':
                comp.set_static_mesh(U.convert(value))
            elif key == 'LightColor':
                comp.set_light_color(U.colour(value), True)
            elif key == 'IntensityUnits':
                comp.set_editor_property('intensity_units', U.convert(value))
            else:
                setter = 'set_' + U.snake(key)
                converted = U.convert(value)
                if hasattr(comp, setter) and not isinstance(value, list):
                    getattr(comp, setter)(converted)
                else:
                    U.set_prop(comp, key, value)
        except Exception as exc:  # noqa: BLE001
            U.warn(f'{comp.get_name()}.{key}: {exc}')


def spawn(eas, e):
    cls = e['cls']
    loc = _vec(e['location'])
    rot = _rot(e['quat'])
    if cls == 'StaticMeshActor':
        mesh = U.find(e['mesh'])
        if mesh is None:
            return None
        actor = eas.spawn_actor_from_object(mesh, loc, rot)
        if actor is not None and e.get('cast_shadow') is False:
            actor.get_editor_property('static_mesh_component').set_cast_shadow(False)
    else:
        klass = getattr(unreal, cls, None)
        if klass is None:
            U.warn(f'Unknown actor class {cls} (is the C++ module built?)')
            return None
        actor = eas.spawn_actor_from_class(klass, loc, rot)
    if actor is None:
        U.warn('Could not spawn ' + e['name'])
        return None
    actor.set_actor_scale3d(_vec(e.get('scale', (1, 1, 1))))
    actor.set_actor_label(e['name'])
    actor.set_folder_path(e.get('folder', 'Props'))
    tags = [unreal.Name(GENERATED)] + [unreal.Name(t) for t in e.get('tags', [])]
    actor.set_editor_property('tags', tags)

    props = dict(e.get('properties', {}))
    if cls == 'DecalActor':
        mat = U.convert(props.pop('DecalMaterial', None))
        decal = actor.get_editor_property('decal')
        if mat is not None:
            decal.set_decal_material(mat)
    if cls.startswith('HH'):
        for comp in actor.get_components_by_class(unreal.SceneComponent):
            try:
                comp.set_mobility(unreal.ComponentMobility.MOVABLE)
            except Exception:  # noqa: BLE001
                pass
    for key, value in props.items():
        U.set_prop(actor, key, value)
    for comp_name, values in e.get('components', {}).items():
        try:
            comp = actor.get_editor_property(U.snake(comp_name))
        except Exception as exc:  # noqa: BLE001
            U.warn(f'{e["name"]}: no component {comp_name} ({exc})')
            continue
        apply_component(comp, values)
    return actor


def build_environment(eas, p):
    fog = eas.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, -100), unreal.Rotator())
    fog.set_actor_label('HeightFog')
    fog.set_folder_path('Environment')
    fog.set_editor_property('tags', [unreal.Name(GENERATED)])
    fc = fog.get_editor_property('component')
    for call, args in (('set_fog_density', (p['fog_density'],)), ('set_fog_height_falloff', (p['fog_height_falloff'],)),
                       ('set_volumetric_fog', (True,)),
                       ('set_volumetric_fog_extinction_scale', (p['volumetric_extinction'],)),
                       ('set_volumetric_fog_distance', (4000.0,)),
                       ('set_volumetric_fog_albedo', (unreal.Color(r=216, g=216, b=216, a=255),))):
        try:
            getattr(fc, call)(*args)
        except Exception as exc:  # noqa: BLE001
            U.warn(f'fog {call}: {exc}')
    for name in ('fog_inscattering_luminance', 'fog_inscattering_color'):
        try:
            fc.set_editor_property(name, U.colour(p['fog_color']))
            break
        except Exception:  # noqa: BLE001
            continue

    ppv = eas.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 150), unreal.Rotator())
    ppv.set_actor_label('PostProcess')
    ppv.set_folder_path('Environment')
    ppv.set_editor_property('tags', [unreal.Name(GENERATED)])
    ppv.set_editor_property('unbound', True)
    s = ppv.get_editor_property('settings')

    def ov(name, value):
        try:
            s.set_editor_property('override_' + name, True)
            s.set_editor_property(name, value)
        except Exception as exc:  # noqa: BLE001
            U.warn(f'post process {name}: {exc}')

    ov('auto_exposure_method', unreal.AutoExposureMethod.AEM_HISTOGRAM)
    ov('auto_exposure_min_brightness', float(p['exposure_min']))
    ov('auto_exposure_max_brightness', float(p['exposure_max']))
    ov('auto_exposure_bias', float(p['exposure_bias']))
    ov('auto_exposure_speed_up', 2.0)
    ov('auto_exposure_speed_down', 0.8)
    ov('bloom_intensity', float(p['bloom']))
    ov('vignette_intensity', float(p['vignette']))
    ov('film_grain_intensity', float(p['grain']))
    c, sat = float(p['contrast']), float(p['saturation'])
    ov('color_contrast', unreal.Vector4(c, c, c, 1.0))
    ov('color_saturation', unreal.Vector4(sat, sat, sat, 1.0))
    ov('color_gain_shadows', unreal.Vector4(*[float(x) for x in p['shadows_tint']], 1.0))
    ov('color_gain_highlights', unreal.Vector4(*[float(x) for x in p['highlights_tint']], 1.0))
    dirt = U.find(p.get('lens_dirt'), required=False)
    if dirt is not None:
        ov('bloom_dirt_mask', dirt)
        ov('bloom_dirt_mask_intensity', 2.0)
    ppv.set_editor_property('settings', s)


def build_map(name, rebuild=True):
    try:
        layout = U.load_json('Layout', name + '.json')
    except FileNotFoundError:
        U.warn(f'{name}: no layout (run Tools/AssetGen/blender/build_world.py)')
        return
    map_path = f'{MAPS_DIR}/{name}'
    les, eas = open_or_create(map_path, rebuild)
    ensure_streaming(les, name, layout.get('streaming', []))
    set_game_mode(name, layout.get('game_mode'))
    prefix = map_path + '.'
    spawned = misplaced = 0
    with unreal.ScopedSlowTask(len(layout['actors']), f'Placing {name}') as task:
        task.make_dialog(False)
        for e in layout['actors']:
            task.enter_progress_frame(1)
            try:
                if e['cls'] == 'HHEnvironment':
                    build_environment(eas, e['properties'])
                    continue
                actor = spawn(eas, e)
                if actor is None:
                    continue
                spawned += 1
                if not actor.get_path_name().startswith(prefix):
                    misplaced += 1
            except Exception as exc:  # noqa: BLE001
                U.warn(f'{name}/{e.get("name")}: {exc}')
    if misplaced:
        U.warn(f'{name}: {misplaced} actors were placed into a streamed sub-level instead of the map itself')
    les.save_current_level()
    U.log(f'{name}: {spawned} actors placed')


def build_level(rebuild=True):
    """All maps, in dependency order; ends with the hideout open."""
    for name in ORDER:
        build_map(name, rebuild)
    les, _ = _subsystems()
    les.load_level(f'{MAPS_DIR}/L_Hideout')
