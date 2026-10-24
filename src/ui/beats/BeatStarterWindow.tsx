// "New Beat": pick a genre, get a finished, mixed starting beat.
import { useState } from 'react';
import { closeWindow, setUI, type FloatingWindow } from '../../state/ui';
import { useProject, getProject } from '../../state/store';
import { BEAT_GENRES, createBeatProject } from '../../project/beatStarter';
import { loadNewProject } from '../../project/manager';
import { ensureProjectSounds } from '../../audio/pro/loader';
import { engine } from '../../audio/engine';
import { PRO_KITS } from '../../audio/pro/catalog';
import { Window } from '../components/Window';
import { Icon } from '../components/Icon';
import { loadKit, previewPro } from '../proSounds';
import './beats.css';

const KEYS = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

/** Builds the beat, opens it and starts playing the song. */
export async function startBeat(genre: string, key?: number) {
  const p = createBeatProject({ genre, key });
  loadNewProject(p, `${BEAT_GENRES.find((g) => g.id === genre)?.emoji ?? ''} ${p.name} ready – press Space to play`);
  setUI({ dockOpen: true, dockTab: 'drums' });
  engine.transport?.setMode('song');
  await ensureProjectSounds(getProject(), 8000);
}

export function BeatStarterWindow({ w }: { w: FloatingWindow }) {
  const [genre, setGenre] = useState('trap');
  const [key, setKey] = useState<number | 'random'>('random');
  const [busy, setBusy] = useState(false);
  const dirty = useProject((s) => s.dirty);
  const g = BEAT_GENRES.find((x) => x.id === genre)!;
  const kit = PRO_KITS.find((k) => k.id === g.kit);
  const create = async () => {
    if (dirty && !confirm('Start a new beat? Unsaved changes of the current project are discarded (Ctrl+S saves first).')) return;
    setBusy(true);
    try {
      await startBeat(genre, key === 'random' ? undefined : key);
      closeWindow(w.id);
      void engine.resume().then(() => engine.transport?.play());
    } finally {
      setBusy(false);
    }
  };
  return (
    <Window w={w} title={<b>New Beat</b>} width={620}>
      <div className="dlg beat-start">
        <div className="beat-genres">
          {BEAT_GENRES.map((x) => {
            const k = PRO_KITS.find((kk) => kk.id === x.kit);
            return (
              <button
                key={x.id}
                className={`beat-genre ${genre === x.id ? 'on' : ''}`}
                style={{ ['--gc' as string]: k?.color ?? 'var(--accent)' }}
                onClick={() => { setGenre(x.id); if (k) void previewPro(k.slots[0].sound); }}
                onDoubleClick={() => void create()}
              >
                <span className="beat-emoji">{x.emoji}</span>
                <b>{x.name}</b>
                <span className="beat-bpm">{k?.bpm} BPM</span>
              </button>
            );
          })}
        </div>
        <div className="beat-info">
          <div className="beat-desc">{g.description}</div>
          <div className="beat-parts">
            <span><Icon name="drums" size={11} /> {kit?.name}</span>
            <span><Icon name="wave" size={11} /> {g.bass === 'kick' ? '808 on the kick' : 'Bass line'}</span>
            {g.chords && <span><Icon name="piano" size={11} /> {g.chords.name}</span>}
            {g.melody && <span><Icon name="sparkle" size={11} /> {g.melody.name}</span>}
            <span><Icon name="mixer" size={11} /> mixed &amp; mastered</span>
          </div>
        </div>
        <div className="dlg-row beat-actions">
          <label className="beat-key">
            Key
            <select className="input" value={key} onChange={(e) => setKey(e.target.value === 'random' ? 'random' : Number(e.target.value))}>
              <option value="random">🎲 Surprise me</option>
              {KEYS.map((k, i) => <option key={k} value={i}>{k} {g.minor ? 'minor' : 'major'}</option>)}
            </select>
          </label>
          <span className="spacer" />
          <button className="btn" title="Only add this genre's drum kit, its groove and the 808 to the open project" onClick={() => { loadKit(g.kit); closeWindow(w.id); setUI({ dockOpen: true, dockTab: 'drums' }); }}>
            <Icon name="plus" size={11} /> Add kit to this project
          </button>
          <button className="btn primary" disabled={busy} onClick={() => void create()}>
            <Icon name="play" size={11} /> {busy ? 'Building…' : 'Create beat'}
          </button>
        </div>
        <div className="beat-hint">A new project with intro, drop, break and drop. Everything is editable – change sounds, notes and effects as you like. Create it again for a different one.</div>
      </div>
    </Window>
  );
}
