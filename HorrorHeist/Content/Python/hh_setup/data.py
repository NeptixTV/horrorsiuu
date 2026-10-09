"""Primary data assets: cosmetics, equipment, jobs, the character definition and DA_GameData."""
import unreal

from . import util as U

DATA = U.ROOT + '/Data'


def _factory(cls):
    f = unreal.DataAssetFactory()
    f.set_editor_property('data_asset_class', cls)
    return f


def _asset(name, folder, cls):
    return U.create_or_load(name, folder, cls, _factory(cls))


def _set(obj, **values):
    for key, value in values.items():
        try:
            obj.set_editor_property(key, value)
        except Exception as exc:  # noqa: BLE001
            U.warn(f'{obj.get_name()}.{key}: {exc}')


def _item_common(asset, entry, order):
    _set(asset,
         item_id=unreal.Name(entry['id']),
         display_name=unreal.Text(entry['name']),
         description=unreal.Text(entry.get('desc', '')),
         flavor_text=unreal.Text(entry.get('flavor', '')),
         rarity=U.enum_value('EHHItemRarity', entry.get('rarity', 'Common')),
         price=int(entry.get('price', 0)),
         unlock_level=int(entry.get('level', 1)),
         owned_by_default=bool(entry.get('owned')),
         sort_order=int(entry.get('order') or order))
    icon = U.find('T_Icon_' + entry['id'], required=False)
    if icon is not None:
        _set(asset, icon=icon)
    if entry.get('theme'):
        _set(asset, theme=unreal.Text(entry['theme']))


def build_cosmetics(cat):
    folder = DATA + '/Cosmetics'
    for i, e in enumerate(cat['cosmetics']):
        a = _asset('DA_' + e['id'], folder, unreal.HHCosmeticDefinition)
        _item_common(a, e, i)
        _set(a, slot=U.enum_value('EHHCosmeticSlot', e['slot']), is_skin_tone=bool(e.get('skin')),
             hides_hair=bool(e.get('hides_hair')))
        mesh = e.get('mesh')
        if mesh:
            obj = U.find(mesh)
            if isinstance(obj, unreal.SkeletalMesh):
                _set(a, skeletal_mesh=obj)
            elif isinstance(obj, unreal.StaticMesh):
                _set(a, static_mesh=obj, attach_bone=unreal.Name(e.get('bone', 'head')))
        if e.get('tint'):
            _set(a, primary_tint=U.colour(e['tint'], 1.0))
        if e.get('tint2'):
            _set(a, secondary_tint=U.colour(e['tint2'], 1.0))


def build_equipment(cat):
    folder = DATA + '/Equipment'
    for i, e in enumerate(cat['equipment']):
        a = _asset('DA_' + e['id'], folder, unreal.HHEquipmentDefinition)
        _item_common(a, e, i)
        stats = []
        for s in e.get('stats', []):
            st = unreal.HHItemStat()
            _set(st, stat_id=unreal.Name(s['id']), label=unreal.Text(s['label']), value=float(s['value']),
                 display_max=float(s['max']), lower_is_better=bool(s.get('lower_better')))
            stats.append(st)
        _set(a, slot=U.enum_value('EHHEquipmentSlot', e['slot']), stats=stats)
        mesh = U.find(e.get('mesh'), required=False)
        if mesh is not None:
            _set(a, mesh=mesh)


def build_missions(cat):
    folder = DATA + '/Missions'
    for e in cat['missions']:
        a = _asset('DA_' + e['id'], folder, unreal.HHMissionDefinition)
        _set(a, mission_id=unreal.Name(e['id']), display_name=unreal.Text(e['name']),
             address=unreal.Text(e['address']), briefing=unreal.Text(e['briefing']), rumor=unreal.Text(e['rumor']),
             primary_target=unreal.Text(e['target']),
             difficulty=U.enum_value('EHHMissionDifficulty', e['difficulty']), residents=int(e['residents']),
             recommended_crew=int(e['crew']), payout_min=int(e['pay'][0]), payout_max=int(e['pay'][1]),
             unlock_level=int(e['level']), sort_order=int(e['order']))
        photo = U.find(e.get('photo'), required=False)
        if photo is not None:
            _set(a, photo=photo)


def build_character():
    a = _asset('DA_Character_Default', DATA + '/Characters', unreal.HHCharacterDefinition)
    _set(a, body_mesh=U.find('SK_HH_Body'), mesh_yaw_offset=-90.0, walk_anim_speed=150.0, run_anim_speed=400.0,
         crouch_walk_anim_speed=120.0)
    for key, name in (('idle', 'A_HH_Idle'), ('walk', 'A_HH_Walk'), ('run', 'A_HH_Run'),
                      ('crouch_idle', 'A_HH_CrouchIdle'), ('crouch_walk', 'A_HH_CrouchWalk'), ('fall', 'A_HH_Fall')):
        anim = U.find(name)
        if anim is not None:
            _set(a, **{key: anim})
    return a


def build_game_data(cat, character):
    a = _asset('DA_GameData', DATA, unreal.HHGameData)
    d = cat['defaults']
    _set(a, default_character=character, starting_cash=int(d['starting_cash']),
         default_cosmetics=[unreal.Name(v) for v in d['cosmetics'].values()],
         default_equipment=[unreal.Name(v) for v in d['equipment'].values()])
    sounds = {
        'settings_mix': 'SM_HH_Settings', 'music_class': 'SC_Music', 'sfx_class': 'SC_SFX',
        'ambient_class': 'SC_Ambient', 'dialogue_class': 'SC_Dialogue', 'ui_class': 'SC_UI',
        'voice_chat_class': 'SC_VoiceChat', 'lobby_music': 'M_Hideout_Theme', 'title_sting': 'M_Title_Sting',
        'departure_sting': 'M_Departure_Sting', 'ui_hover': 'UI_Hover', 'ui_click': 'UI_Click', 'ui_back': 'UI_Back',
        'ui_confirm': 'UI_Confirm', 'ui_error': 'UI_Error', 'ui_open_panel': 'UI_OpenPanel', 'ui_equip': 'UI_Equip',
        'ui_purchase': 'UI_Purchase', 'ui_ready': 'UI_Ready', 'ui_unready': 'UI_Unready',
        'ui_countdown_tick': 'UI_CountdownTick', 'ui_notify': 'UI_Notify', 'ui_level_up': 'UI_LevelUp',
        'flashlight_on': 'S_Flashlight_On', 'flashlight_off': 'S_Flashlight_Off',
        'interaction_highlight': 'M_HH_Highlight',
    }
    for key, name in sounds.items():
        obj = U.find(name)
        if obj is not None:
            _set(a, **{key: obj})

    def many(prefix, count):
        return [o for o in (U.find(f'{prefix}{i:02d}', required=False) for i in range(1, count + 1)) if o]

    _set(a, landing_sounds=many('S_Land_', 2), default_footsteps=many('S_Footstep_Concrete_', 6))
    sets = []
    for surface, prefix, n, volume in ((1, 'S_Footstep_Concrete_', 6, 1.0), (2, 'S_Footstep_Wood_', 6, 1.0),
                                       (3, 'S_Footstep_Metal_', 6, 0.9), (4, 'S_Footstep_Fabric_', 4, 0.8),
                                       (5, 'S_Footstep_Concrete_', 6, 1.1), (6, 'S_Footstep_Concrete_', 6, 1.0)):
        fs = unreal.HHFootstepSet()
        _set(fs, surface_type=surface, sounds=many(prefix, n), volume_multiplier=volume)
        sets.append(fs)
    _set(a, footstep_sets=sets)
    return a


def build_all():
    cat = U.load_json('Data', 'catalog.json')
    build_cosmetics(cat)
    build_equipment(cat)
    build_missions(cat)
    character = build_character()
    build_game_data(cat, character)
    U.refresh_index()
