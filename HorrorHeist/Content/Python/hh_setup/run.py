"""Entry points: run_all() builds everything; run_step('Level') re-runs one step."""
import importlib
import traceback

import unreal

from . import data, importers, level, materials
from . import util as U

STEPS = [
    ('Textures', importers.import_textures),
    ('Audio', importers.import_audio),
    ('Materials', materials.build_all),
    ('Meshes', importers.import_meshes),
    ('Data', data.build_all),
    ('Level', level.build_level),
]


def _reload():
    for mod in (U, importers, materials, data, level):
        importlib.reload(mod)


def run_all(reload_modules=True):
    if reload_modules:
        _reload()
    U.ERRORS.clear()
    U.refresh_index()
    with unreal.ScopedSlowTask(len(STEPS), 'The Quiet Job: building content from SourceArt') as task:
        task.make_dialog(True)
        for label, fn in STEPS:
            if task.should_cancel():
                U.warn('Cancelled before ' + label)
                break
            task.enter_progress_frame(1, label + '...')
            _run(label, fn)
    _report()


def run_step(label):
    _reload()
    U.ERRORS.clear()
    U.refresh_index()
    for name, fn in STEPS:
        if name.lower() == label.lower():
            _run(name, fn)
    _report()


def _run(label, fn):
    U.log(f'--- {label} ---')
    try:
        fn()
    except Exception:  # noqa: BLE001
        U.warn(f'{label} failed:\n{traceback.format_exc()}')
    try:
        U.save_all()
    except Exception:  # noqa: BLE001
        U.warn('Saving failed:\n' + traceback.format_exc())


def _report():
    if U.ERRORS:
        U.log(f'Finished with {len(U.ERRORS)} warnings (see Output Log, filter "HH Setup").')
        msg = 'Content built with %d warnings.\nOpen Window > Output Log and filter for "HH Setup".' % len(U.ERRORS)
    else:
        U.log('Finished without warnings.')
        msg = 'Content built. Press Play (Net Mode: Listen Server, 2+ players) to try the hideout.'
    unreal.EditorDialog.show_message('The Quiet Job', msg, unreal.AppMsgType.OK)
