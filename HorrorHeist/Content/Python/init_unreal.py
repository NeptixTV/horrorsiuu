"""
Runs when the editor starts. Adds a "The Quiet Job" menu and offers to build all content from
SourceArt when it is missing or was built by an older version of the project (the C++ then
expects maps, jobs and data the old content does not have - e.g. jobs without a map can't be
pinned on the job board).
"""
import unreal

MAP = '/Game/HorrorHeist/Maps/L_Hideout'
GAME_DATA = '/Game/HorrorHeist/Data/DA_GameData'
# Everything the current code needs to start a job from the hideout.
REQUIRED = (MAP, '/Game/HorrorHeist/Maps/L_Town', '/Game/HorrorHeist/Maps/L_Job_Vance', GAME_DATA,
            '/Game/HorrorHeist/Data/Missions/DA_JOB_Vance', '/Game/HorrorHeist/Data/Loot/DA_LT_CoinCollection')


def _add_menu():
    menus = unreal.ToolMenus.get()
    main = menus.find_menu('LevelEditor.MainMenu')
    if main is None:
        return
    sub = main.add_sub_menu('HorrorHeist', 'HorrorHeist', 'HorrorHeistMenu', 'The Quiet Job',
                            'Build content for The Quiet Job')
    for name, label, tip, cmd in (
            ('HH_BuildAll', 'Build / Rebuild All Content', 'Import SourceArt, create materials, data and the hideout',
             'import hh_setup.run as r; r.run_all()'),
            ('HH_Level', 'Rebuild Maps Only', 'Re-place hideout, town and job maps from SourceArt/Layout',
             'import hh_setup.run as r; r.run_step("Level")'),
            ('HH_Data', 'Rebuild Data Assets Only', 'Cosmetics, equipment, loot, jobs and DA_GameData',
             'import hh_setup.run as r; r.run_step("Data")'),
            ('HH_Open', 'Open Hideout', 'Open L_Hideout',
             'import unreal; unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level("%s")' % MAP)):
        entry = unreal.ToolMenuEntry(name=name, type=unreal.MultiBlockType.MENU_ENTRY)
        entry.set_label(label)
        entry.set_tool_tip(tip)
        entry.set_string_command(unreal.ToolMenuStringCommandType.PYTHON, '', cmd)
        sub.add_menu_entry('Content', entry)
    menus.refresh_all_widgets()


_handle = None


def _content_problems():
    """Why the built content does not match this version of the project (empty list = up to date)."""
    from hh_setup.version import CONTENT_VERSION
    lib = unreal.EditorAssetLibrary
    missing = [p.rsplit('/', 1)[1] for p in REQUIRED if not lib.does_asset_exist(p)]
    if missing:
        return ['missing: ' + ', '.join(missing)]
    problems = []
    data = unreal.load_asset(GAME_DATA)
    try:
        built = int(data.get_editor_property('content_version')) if data else 0
    except Exception:  # noqa: BLE001 - the property is newer than the compiled editor
        built = 0
    if built < CONTENT_VERSION:
        problems.append(f'content version {built}, this project needs {CONTENT_VERSION}')
    return problems


def _first_run_check(_delta):
    global _handle
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    if registry.is_loading_assets():
        return
    unreal.unregister_slate_post_tick_callback(_handle)
    _handle = None
    lib = unreal.EditorAssetLibrary
    first_build = not (lib.does_asset_exist(MAP) and lib.does_asset_exist(GAME_DATA))
    problems = [] if first_build else _content_problems()
    if not first_build and not problems:
        return
    if first_build:
        text = ('The game content has not been built yet.\n\nImport the generated art, sounds and music, create the '
                'materials and data assets and build the hideout, town and job maps now? This takes a few minutes.')
    else:
        text = ('The game content was built by an older version of the project and does not match the code '
                '(' + '; '.join(problems) + ').\n\nWithout a rebuild, jobs can\'t be pinned or started.\n\n'
                'Rebuild all content now? This takes a few minutes and replaces the generated maps and data '
                '(assets under /Game/HorrorHeist).')
    unreal.log_warning('[HH Setup] content check: ' + ('first build' if first_build else '; '.join(problems)))
    answer = unreal.EditorDialog.show_message(
        'The Quiet Job', text + '\n\n(You can do this later via the "The Quiet Job" menu.)', unreal.AppMsgType.YES_NO)
    if answer == unreal.AppReturnType.YES:
        import hh_setup.run as run
        run.run_all(reload_modules=not first_build)
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(MAP)


try:
    _add_menu()
except Exception as exc:  # noqa: BLE001
    unreal.log_warning('[HH Setup] menu: %s' % exc)

try:
    # Not in commandlets (cooking, automation): there is no Slate tick there.
    if unreal.is_editor() and not unreal.SystemLibrary.is_unattended():
        _handle = unreal.register_slate_post_tick_callback(_first_run_check)
except Exception as exc:  # noqa: BLE001
    unreal.log_warning('[HH Setup] first-run check disabled: %s' % exc)
