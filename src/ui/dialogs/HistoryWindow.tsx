// Undo history: every change as a list – click a step to jump there (later
// steps stay available as redo until you change something).
import { jumpToHistory, useProject } from '../../state/store';
import { useUI, type FloatingWindow } from '../../state/ui';
import { Window } from '../components/Window';
import { Icon } from '../components/Icon';
import './dialogs.css';

export function HistoryWindow({ w }: { w: FloatingWindow }) {
  const past = useProject((s) => s.pastLabels);
  const future = useProject((s) => s.futureLabels);
  const limit = useUI((s) => s.historyLimit);
  const rows = [
    { label: 'Opened project', index: 0, state: 'past' as const },
    ...past.map((l, i) => ({ label: l, index: i + 1, state: 'past' as const })),
    ...future.map((l, i) => ({ label: l, index: past.length + 1 + i, state: 'future' as const })),
  ];
  const current = past.length;
  return (
    <Window w={w} title={<><Icon name="history" size={12} /> <b>Undo history</b></>} width={340}>
      <div className="dlg">
        <div className="dlg-list hist">
          {rows.map((r) => (
            <button
              key={r.index}
              className={`hist-row ${r.index === current ? 'current' : ''} ${r.state === 'future' ? 'future' : ''}`}
              onClick={() => jumpToHistory(r.index)}
              title={r.state === 'future' ? 'Redo up to here' : 'Go back to this point'}
            >
              <span className="hist-n">{r.index}</span>
              <span className="hist-label">{r.label}</span>
              {r.index === current && <span className="hist-now">now</span>}
            </button>
          ))}
        </div>
        <p className="crumb">Ctrl+Z / Ctrl+Shift+Z step one by one. Up to {limit} steps are kept (Settings → Projects).</p>
      </div>
    </Window>
  );
}
