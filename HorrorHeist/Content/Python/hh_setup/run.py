"""Entry points: run_all() builds everything; run_step('Level') re-runs one step."""
import importlib
import os
import traceback

import unreal

from . import data, importers, level, materials, version
from . import util as U

STEPS = [
    ('Textures', importers.import_textures),
    ('Audio', importers.import_audio),
    ('Materials', materials.build_all),
    ('Meshes', importers.import_meshes),
    # Maps before data: the job definitions point at their maps.
    ('Level', level.build_level),
    ('Data', data.build_all),
]
REPORT = 'HHSetup_Report.txt'


def _reload():
    for mod in (U, version, importers, materials, data, level):
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


def _write_report():
    """Every warning of this run in Saved/HHSetup_Report.txt (overwritten each run)."""
    path = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), REPORT)
    try:
        with open(path, 'w', encoding='utf-8') as fh:
            fh.write('The Quiet Job - content build report\n')
            fh.write(f'{len(U.ERRORS)} warning(s)\n\n')
            for i, line in enumerate(U.ERRORS, 1):
                fh.write(f'{i:3d}. {line}\n')
    except OSError as exc:
        U.log(f'Could not write {path}: {exc}')
    return path


def _report():
    path = _write_report()
    if U.ERRORS:
        U.log(f'Finished with {len(U.ERRORS)} warnings (see {path}).')
        first = '\n'.join('- ' + e.splitlines()[0][:160] for e in U.ERRORS[:8])
        more = '' if len(U.ERRORS) <= 8 else f'\n... and {len(U.ERRORS) - 8} more'
        msg = (f'Content built with {len(U.ERRORS)} warnings:\n\n{first}{more}\n\n'
               f'Full list: {path}\n(also in Window > Output Log, filter "HH Setup").')
    else:
        U.log('Finished without warnings.')
        msg = ('Content built without warnings. Press Play (Net Mode: Listen Server, 2+ players) to try the '
               'hideout; the stair door leads up into town.')
    unreal.EditorDialog.show_message('The Quiet Job', msg, unreal.AppMsgType.OK)
