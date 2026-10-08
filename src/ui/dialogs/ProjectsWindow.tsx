import { useEffect, useState } from 'react';
import { closeWindow, type FloatingWindow } from '../../state/ui';
import { storage } from '../../project/storage';
import { openProject, newProject, importProjectFile } from '../../project/manager';
import { useProject } from '../../state/store';
import { Window } from '../components/Window';
import { Icon } from '../components/Icon';
import { pickFile } from './filePick';
import './dialogs.css';

export function ProjectsWindow({ w }: { w: FloatingWindow }) {
  const [list, setList] = useState<{ id: string; name: string; updatedAt: number }[] | null>(null);
  const currentId = useProject((s) => s.project.id);
  const load = () => storage.listProjects().then(setList).catch(() => setList([]));
  useEffect(() => { void load(); }, []);
  const confirmDiscard = () => !useProject.getState().dirty || confirm('Discard unsaved changes of the current project?');
  return (
    <Window w={w} title={<b>Open project</b>} width={460}>
      <div className="dlg">
        <div className="dlg-row">
          <button className="btn" onClick={() => { if (confirmDiscard()) { newProject(); closeWindow(w.id); } }}><Icon name="file" size={12} /> New empty project</button>
          <button className="btn" onClick={() => pickFile('.json,application/json', async (f) => { if (confirmDiscard() && (await importProjectFile(f))) closeWindow(w.id); })}><Icon name="import" size={12} /> Import file…</button>
        </div>
        <div className="dlg-list">
          {list === null && <div className="empty-hint">Loading…</div>}
          {list?.length === 0 && <div className="empty-hint">No saved projects yet. Press Ctrl+S to save the current one.</div>}
          {list?.map((p) => (
            <div key={p.id} className={`dlg-item ${p.id === currentId ? 'active' : ''}`} onDoubleClick={async () => { if (confirmDiscard() && (await openProject(p.id))) closeWindow(w.id); }}>
              <Icon name="folder" size={13} />
              <div className="dlg-item-main">
                <div className="dlg-item-name">{p.name}{p.id === currentId && <span className="crumb"> · open</span>}</div>
                <div className="dlg-item-sub">{new Date(p.updatedAt).toLocaleString()}</div>
              </div>
              <button className="btn small" onClick={async () => { if (confirmDiscard() && (await openProject(p.id))) closeWindow(w.id); }}>Open</button>
              <button className="btn small ghost danger" title="Delete" onClick={async () => { if (confirm(`Delete “${p.name}” permanently?`)) { await storage.deleteProject(p.id); void load(); } }}><Icon name="trash" size={11} /></button>
            </div>
          ))}
        </div>
      </div>
    </Window>
  );
}
