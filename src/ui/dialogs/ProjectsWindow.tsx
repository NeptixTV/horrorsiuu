import { useEffect, useState } from 'react';
import { closeWindow, toast, type FloatingWindow } from '../../state/ui';
import { storage, type TrashRecord } from '../../project/storage';
import { openProject, newProject, importProjectFile, trashProject, restoreFromTrash, deleteForever, trashHousekeeping } from '../../project/manager';
import { daysLeft, TRASH_DAYS } from '../../project/trash';
import { useProject } from '../../state/store';
import { Window } from '../components/Window';
import { Icon } from '../components/Icon';
import { pickFile } from './filePick';
import './dialogs.css';

type Tab = 'projects' | 'trash';
type TrashRow = Omit<TrashRecord, 'data'>;

export function ProjectsWindow({ w }: { w: FloatingWindow }) {
  const [tab, setTab] = useState<Tab>(w.win.kind === 'projects' && w.win.tab === 'trash' ? 'trash' : 'projects');
  const [list, setList] = useState<{ id: string; name: string; updatedAt: number }[] | null>(null);
  const [trash, setTrash] = useState<TrashRow[] | null>(null);
  const [flash, setFlash] = useState<string | null>(null);
  const currentId = useProject((s) => s.project.id);
  const load = () => storage.listProjects().then(setList).catch(() => setList([]));
  const loadTrash = () => storage.listTrash().then(setTrash).catch(() => setTrash([]));
  useEffect(() => {
    void load();
    // also clears out old entries and finds projects deleted before the trash existed
    void trashHousekeeping().then(loadTrash);
  }, []);
  const confirmDiscard = () => !useProject.getState().dirty || confirm('Discard unsaved changes of the current project?');
  const now = Date.now();

  const remove = async (p: { id: string; name: string }) => {
    if (!(await trashProject(p.id))) return;
    toast(`“${p.name}” moved to the trash – you can restore it there for ${TRASH_DAYS} days`, 'ok', true);
    void load();
    void loadTrash();
  };
  const restore = async (t: TrashRow) => {
    const r = await restoreFromTrash(t.id);
    void loadTrash();
    if (!r) return;
    toast(`“${r.name}” is back in your projects`, 'ok', true);
    await load();
    setFlash(r.id);
    setTab('projects');
  };
  const forget = async (rows: TrashRow[]) => {
    const what = rows.length === 1 ? `“${rows[0].name}”` : `all ${rows.length} projects in the trash`;
    if (!confirm(`Delete ${what} for good? This cannot be undone.`)) return;
    await deleteForever(rows.map((r) => r.id));
    void loadTrash();
  };

  return (
    <Window w={w} title={<b>{tab === 'trash' ? 'Trash' : 'Open project'}</b>} width={480}>
      <div className="dlg">
        <div className="dlg-tabs" role="tablist">
          <button role="tab" aria-selected={tab === 'projects'} className={tab === 'projects' ? 'on' : ''} onClick={() => setTab('projects')}>
            <Icon name="folder" size={12} /> Projects{list ? ` (${list.length})` : ''}
          </button>
          <button role="tab" aria-selected={tab === 'trash'} className={tab === 'trash' ? 'on' : ''} onClick={() => setTab('trash')}>
            <Icon name="trash" size={12} /> Trash{trash?.length ? ` (${trash.length})` : ''}
          </button>
        </div>

        {tab === 'projects' && (
          <>
            <div className="dlg-row">
              <button className="btn" onClick={() => { if (confirmDiscard()) { newProject(); closeWindow(w.id); } }}><Icon name="file" size={12} /> New empty project</button>
              <button className="btn" onClick={() => pickFile('.json,application/json', async (f) => { if (confirmDiscard() && (await importProjectFile(f))) closeWindow(w.id); })}><Icon name="import" size={12} /> Import file…</button>
            </div>
            <div className="dlg-list">
              {list === null && <div className="empty-hint">Loading…</div>}
              {list?.length === 0 && <div className="empty-hint">No saved projects yet. Press Ctrl+S to save the current one.</div>}
              {list?.map((p) => (
                <div key={p.id} className={`dlg-item ${p.id === currentId ? 'active' : ''} ${p.id === flash ? 'flash' : ''}`} onDoubleClick={async () => { if (confirmDiscard() && (await openProject(p.id))) closeWindow(w.id); }}>
                  <Icon name="folder" size={13} />
                  <div className="dlg-item-main">
                    <div className="dlg-item-name">{p.name}{p.id === currentId && <span className="crumb"> · open</span>}</div>
                    <div className="dlg-item-sub">{new Date(p.updatedAt).toLocaleString()}</div>
                  </div>
                  <button className="btn small" onClick={async () => { if (confirmDiscard() && (await openProject(p.id))) closeWindow(w.id); }}>Open</button>
                  <button
                    className="btn small ghost danger"
                    // the open project would come straight back with the next autosave
                    disabled={p.id === currentId}
                    title={p.id === currentId ? 'This project is open – open another one first to delete it' : `Move to the trash (restorable for ${TRASH_DAYS} days)`}
                    onClick={() => void remove(p)}
                  ><Icon name="trash" size={11} /></button>
                </div>
              ))}
            </div>
          </>
        )}

        {tab === 'trash' && (
          <>
            <div className="dlg-trash-head">
              <span>Deleted projects stay here for {TRASH_DAYS} days, then they are removed for good.</span>
              <button className="btn small ghost danger" disabled={!trash?.length} onClick={() => trash && void forget(trash)}>Empty trash</button>
            </div>
            <div className="dlg-list">
              {trash === null && <div className="empty-hint">Loading…</div>}
              {trash?.length === 0 && <div className="empty-hint">The trash is empty.</div>}
              {trash?.map((t) => {
                const left = daysLeft(t.deletedAt, now);
                return (
                  <div key={t.id} className="dlg-item" onDoubleClick={() => void restore(t)}>
                    <Icon name="trash" size={13} />
                    <div className="dlg-item-main">
                      <div className="dlg-item-name">{t.name}</div>
                      <div className="dlg-item-sub">
                        {t.from === 'backup'
                          ? `Deleted earlier – found in its backup from ${new Date(t.updatedAt).toLocaleString()}`
                          : `Deleted ${new Date(t.deletedAt).toLocaleString()}`}
                        {' · '}
                        <span className={left <= 3 ? 'dlg-warn' : ''}>{left <= 1 ? 'last day' : `${left} days left`}</span>
                      </div>
                    </div>
                    <button className="btn small" title="Put it back into your projects" onClick={() => void restore(t)}><Icon name="undo" size={11} /> Restore</button>
                    <button className="btn small ghost danger" title="Delete for good" onClick={() => void forget([t])}><Icon name="close" size={11} /></button>
                  </div>
                );
              })}
            </div>
          </>
        )}
      </div>
    </Window>
  );
}
