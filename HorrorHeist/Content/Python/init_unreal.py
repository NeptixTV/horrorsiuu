"""
Runs when the editor starts. Adds a "The Quiet Job" menu and, on the first launch (no hideout
map yet), offers to build all content from SourceArt.
"""
import unreal

MAP = '/Game/HorrorHeist/Maps/L_Hideout'
GAME_DATA = '/Game/HorrorHeist/Data/DA_GameData'


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
            ('HH_Level', 'Rebuild Hideout Level Only', 'Re-place the hideout from SourceArt/Layout',
             'import hh_setup.run as r; r.run_step("Level")'),
            ('HH_Data', 'Rebuild Data Assets Only', 'Cosmetics, equipment, jobs and DA_GameData',
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


def _first_run_check(_delta):
    global _handle
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    if registry.is_loading_assets():
        return
    unreal.unregister_slate_post_tick_callback(_handle)
    _handle = None
    lib = unreal.EditorAssetLibrary
    if lib.does_asset_exist(MAP) and lib.does_asset_exist(GAME_DATA):
        return
    answer = unreal.EditorDialog.show_message(
        'The Quiet Job',
        'The game content has not been built yet.\n\nImport the generated art, sounds and music, create the '
        'materials and data assets and build the hideout level now? This takes a few minutes.\n\n'
        '(You can do this later via the "The Quiet Job" menu.)',
        unreal.AppMsgType.YES_NO)
    if answer == unreal.AppReturnType.YES:
        import hh_setup.run as run
        run.run_all(reload_modules=False)
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
