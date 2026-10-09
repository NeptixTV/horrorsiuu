"""Imports SourceArt (textures, audio, meshes, animations) into /Game/HorrorHeist."""
import os

import unreal

from . import util as U

TEX_ROOT = U.ROOT + '/Textures'
AUDIO_ROOT = U.ROOT + '/Audio'
MESH_ROOT = U.ROOT + '/Meshes'


def _run_tasks(tasks):
    if tasks:
        U.AT.import_asset_tasks(tasks)
    out = []
    for task in tasks:
        paths = list(task.get_editor_property('imported_object_paths') or [])
        if not paths:
            U.warn('Import produced nothing: ' + str(task.filename))
        out.append(paths)
    return out


def _task(filename, dest, name=None, options=None):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', filename)
    task.set_editor_property('destination_path', dest)
    if name:
        task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', False)
    if options is not None:
        task.set_editor_property('options', options)
    return task


# =============================================================================================
# Textures

def import_textures():
    folders = {'Materials': 'Materials', 'Unique': 'Unique', 'Decals': 'Decals', 'Icons': 'Icons'}
    tasks = []
    for sub, dest in folders.items():
        src = os.path.join(U.SOURCE, 'Textures', sub)
        if not os.path.isdir(src):
            continue
        U.ensure_dir(f'{TEX_ROOT}/{dest}')
        for fn in sorted(os.listdir(src)):
            if fn.lower().endswith(('.png', '.jpg', '.jpeg', '.tga')):
                tasks.append(_task(os.path.join(src, fn), f'{TEX_ROOT}/{dest}', os.path.splitext(fn)[0]))
    U.log(f'Importing {len(tasks)} textures')
    _run_tasks(tasks)
    U.refresh_index()
    for task in tasks:
        name = os.path.splitext(os.path.basename(task.filename))[0]
        tex = U.find(name, required=False)
        if tex is None:
            continue
        try:
            configure_texture(tex, name, task.destination_path)
        except Exception as exc:  # noqa: BLE001
            U.warn(f'{name}: {exc}')


def configure_texture(tex, name, folder):
    if name.endswith('_N'):
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property('srgb', False)
    elif name.endswith('_ORM'):
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property('srgb', False)
    elif folder.endswith('/Icons') or name.startswith('T_Mission_'):
        tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property('never_stream', True)
    elif name == 'T_TV_Static':
        tex.set_editor_property('filter', unreal.TextureFilter.TF_NEAREST)


# =============================================================================================
# Audio

CLASS_FOR_CATEGORY = {'Ambient': 'SC_Ambient', 'SFX': 'SC_SFX', 'UI': 'SC_UI', 'Music': 'SC_Music',
                      'Dialogue': 'SC_Dialogue'}


def create_sound_classes():
    folder = AUDIO_ROOT + '/Classes'
    classes = {}
    for name in ('SC_Master', 'SC_Music', 'SC_SFX', 'SC_Ambient', 'SC_Dialogue', 'SC_UI', 'SC_VoiceChat'):
        classes[name] = U.create_or_load(name, folder, unreal.SoundClass, unreal.SoundClassFactory())
    try:
        classes['SC_Master'].set_editor_property('child_classes', [c for n, c in classes.items() if n != 'SC_Master'])
    except Exception as exc:  # noqa: BLE001
        U.log(f'Sound class hierarchy left flat ({exc})')
    U.create_or_load('SM_HH_Settings', folder, unreal.SoundMix, unreal.SoundMixFactory())
    return classes


def attenuation(radius):
    radius = int(round(radius / 100.0) * 100)
    name = f'ATT_R{radius}'
    folder = AUDIO_ROOT + '/Attenuation'
    existing = U.find(name, required=False)
    if existing is not None:
        return existing
    att = U.create_or_load(name, folder, unreal.SoundAttenuation, unreal.SoundAttenuationFactory())
    settings = att.get_editor_property('attenuation')
    for key, value in (('attenuate', True), ('spatialize', True),
                       ('attenuation_shape_extents', unreal.Vector(radius * 0.08, 0, 0)),
                       ('falloff_distance', float(radius)),
                       ('distance_algorithm', unreal.AttenuationDistanceModel.NATURAL_SOUND),
                       ('attenuate_with_lpf', True), ('lpf_radius_min', radius * 0.25),
                       ('lpf_radius_max', float(radius)), ('enable_occlusion', True),
                       ('occlusion_low_pass_filter_frequency', 1800.0), ('occlusion_volume_attenuation', 0.6),
                       ('occlusion_interpolation_time', 0.25), ('enable_reverb_send', True)):
        try:
            settings.set_editor_property(key, value)
        except Exception as exc:  # noqa: BLE001
            U.log(f'attenuation {key}: {exc}')
    att.set_editor_property('attenuation', settings)
    return att


def import_audio():
    classes = create_sound_classes()
    manifest = U.load_json('Audio', 'manifest.json')
    entries = manifest if isinstance(manifest, list) else manifest.get('sounds', manifest)
    if isinstance(entries, dict):
        entries = [dict(file=k, **v) if 'file' not in v else v for k, v in entries.items()]
    tasks = []
    for e in entries:
        rel = e['file']
        path = rel if os.path.isabs(rel) else os.path.join(U.PROJECT, rel)
        sub = os.path.basename(os.path.dirname(path))
        tasks.append((_task(path, f'{AUDIO_ROOT}/{sub}', os.path.splitext(os.path.basename(path))[0]), e))
    for _, e in tasks:
        U.ensure_dir(_.destination_path)
    U.log(f'Importing {len(tasks)} sounds')
    _run_tasks([t for t, _ in tasks])
    U.refresh_index()
    for task, e in tasks:
        name = os.path.splitext(os.path.basename(task.filename))[0]
        wave = U.find(name, required=False)
        if wave is None:
            continue
        try:
            wave.set_editor_property('looping', bool(e.get('loop')))
            sc = classes.get(CLASS_FOR_CATEGORY.get(e.get('category'), 'SC_SFX'))
            if sc is not None:
                wave.set_editor_property('sound_class_object', sc)
            if e.get('volume'):
                wave.set_editor_property('volume', float(e['volume']))
            if e.get('spatial'):
                wave.set_editor_property('attenuation_settings', attenuation(float(e.get('radius', 1500))))
            if e.get('loop'):
                wave.set_editor_property('virtualization_mode', unreal.VirtualizationMode.PLAY_WHEN_SILENT)
        except Exception as exc:  # noqa: BLE001
            U.warn(f'{name}: {exc}')


# =============================================================================================
# Meshes

def _disable_interchange_fbx():
    # The scripts below drive the classic FBX importer (FbxImportUI options).
    for cmd in ('Interchange.FeatureFlags.Import.FBX 0', 'Interchange.FeatureFlags.Import.FBX.ToLevel 0'):
        try:
            unreal.SystemLibrary.execute_console_command(None, cmd)
        except Exception:  # noqa: BLE001
            pass


def _static_options(nanite):
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', True)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_as_skeletal', False)
    ui.set_editor_property('import_animations', False)
    ui.set_editor_property('automated_import_should_detect_type', False)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    data = ui.get_editor_property('static_mesh_import_data')
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('generate_lightmap_u_vs', False)
    data.set_editor_property('auto_generate_collision', False)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    try:
        data.set_editor_property('build_nanite', bool(nanite))
    except Exception:  # noqa: BLE001
        pass
    return ui


def _skeletal_options(skeleton=None, physics=False):
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', True)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_as_skeletal', True)
    ui.set_editor_property('import_animations', False)
    ui.set_editor_property('automated_import_should_detect_type', False)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property('create_physics_asset', physics)
    if skeleton is not None:
        ui.set_editor_property('skeleton', skeleton)
    data = ui.get_editor_property('skeletal_mesh_import_data')
    data.set_editor_property('import_morph_targets', False)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return ui


def _anim_options(skeleton):
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_as_skeletal', True)
    ui.set_editor_property('import_animations', True)
    ui.set_editor_property('automated_import_should_detect_type', False)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_ANIMATION)
    ui.set_editor_property('skeleton', skeleton)
    data = ui.get_editor_property('anim_sequence_import_data')
    data.set_editor_property('animation_length', unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    data.set_editor_property('import_meshes_in_bone_hierarchy', False)
    data.set_editor_property('remove_redundant_keys', False)
    return ui


def _assign_static_materials(mesh):
    for index, slot in enumerate(mesh.get_editor_property('static_materials')):
        name = str(slot.get_editor_property('material_slot_name'))
        mi = U.find(name, required=False)
        if mi is None:
            U.warn(f'{mesh.get_name()}: no material for slot {name}')
            continue
        mesh.set_material(index, mi)


def _assign_skeletal_materials(mesh):
    mats = mesh.get_editor_property('materials')
    for m in mats:
        name = str(m.get_editor_property('material_slot_name'))
        mi = U.find(name, required=False)
        if mi is not None:
            m.set_editor_property('material_interface', mi)
        else:
            U.warn(f'{mesh.get_name()}: no material for slot {name}')
    mesh.set_editor_property('materials', mats)


def _collision(mesh, kind):
    body = mesh.get_editor_property('body_setup')
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if kind == 'complex' and body is not None:
        body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    elif kind in ('box', 'convex'):
        shape = unreal.ScriptingCollisionShapeType.BOX if kind == 'box' else unreal.ScriptingCollisionShapeType.NDOP26
        try:
            sub.remove_collisions(mesh)
        except Exception:  # noqa: BLE001
            pass
        sub.add_simple_collisions(mesh, shape)


def import_meshes():
    _disable_interchange_fbx()
    meshes = U.load_json('Meshes', 'meshes.json')
    statics = {n: e for n, e in meshes.items() if e.get('kind', 'static') == 'static'}
    tasks = []
    for name, e in sorted(statics.items()):
        dest = f"{MESH_ROOT}/{e['folder']}"
        U.ensure_dir(dest)
        tasks.append((_task(os.path.join(U.PROJECT, e['file']), dest, name, _static_options(e.get('nanite'))), e))
    U.log(f'Importing {len(tasks)} static meshes')
    _run_tasks([t for t, _ in tasks])
    U.refresh_index()
    for task, e in tasks:
        mesh = U.find(task.destination_name, required=False)
        if mesh is None:
            continue
        try:
            _assign_static_materials(mesh)
            _collision(mesh, e.get('collision', 'box'))
        except Exception as exc:  # noqa: BLE001
            U.warn(f'{task.destination_name}: {exc}')

    # Body first: it owns the skeleton everything else binds to.
    body_entry = meshes.get('SK_HH_Body')
    if body_entry is None:
        U.warn('SK_HH_Body missing from meshes.json')
        return
    dest = f'{MESH_ROOT}/Character'
    U.ensure_dir(dest)
    _run_tasks([_task(os.path.join(U.PROJECT, body_entry['file']), dest, 'SK_HH_Body', _skeletal_options(None, True))])
    U.refresh_index()
    body = U.find('SK_HH_Body')
    if body is None:
        return
    skeleton = body.get_editor_property('skeleton')
    _assign_skeletal_materials(body)

    garments = {n: e for n, e in meshes.items() if e.get('kind') == 'skeletal' and n != 'SK_HH_Body'}
    tasks = []
    for name, e in sorted(garments.items()):
        gdest = f"{MESH_ROOT}/{e['folder']}"
        U.ensure_dir(gdest)
        tasks.append(_task(os.path.join(U.PROJECT, e['file']), gdest, name, _skeletal_options(skeleton, False)))
    U.log(f'Importing {len(tasks)} skinned cosmetics')
    _run_tasks(tasks)
    U.refresh_index()
    for name in garments:
        mesh = U.find(name, required=False)
        if mesh is not None:
            _assign_skeletal_materials(mesh)

    anims = {n: e for n, e in meshes.items() if e.get('kind') == 'anim'}
    adest = f'{MESH_ROOT}/Character/Anims'
    U.ensure_dir(adest)
    tasks = [_task(os.path.join(U.PROJECT, e['file']), adest, name, _anim_options(skeleton))
             for name, e in sorted(anims.items())]
    U.log(f'Importing {len(tasks)} animations')
    results = _run_tasks(tasks)
    # The FBX importer may append the take name; rename to the plain file name.
    for task, paths in zip(tasks, results):
        want = f'{adest}/{task.destination_name}'
        for path in paths:
            obj = U.EAL.load_asset(path)
            if isinstance(obj, unreal.AnimSequence) and obj.get_path_name().split('.')[0] != want:
                if U.EAL.does_asset_exist(want):
                    U.EAL.delete_asset(want)
                U.EAL.rename_asset(obj.get_path_name().split('.')[0], want)
    U.refresh_index()
    for name in anims:
        anim = U.find(name, required=False)
        if anim is None:
            continue
        try:
            anim.set_editor_property('enable_root_motion', False)
        except Exception:  # noqa: BLE001
            pass
