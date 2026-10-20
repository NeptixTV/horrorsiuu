import logo from '../../assets/logo.png';
import { useState } from 'react';
import { setUI, ui, useUI, type FloatingWindow } from '../../state/ui';
import { useProject } from '../../state/store';
import { stemSources } from '../../audio/render';
import { exportPatternMidi, exportSongMidi } from '../../midi/midiProject';
import { Icon } from '../components/Icon';
import { Window } from '../components/Window';
import { exportStems, exportWav } from '../commands';
import { APP_VERSION } from '../../admin/feed';
import './dialogs.css';

const SHORTCUTS: [string, string][] = [
  ['Space', 'Play / Pause'],
  ['R', 'Record'],
  ['L', 'Toggle pattern / song mode'],
  ['Ctrl/Cmd + S', 'Save'],
  ['Ctrl/Cmd + Shift + S', 'Save as'],
  ['Ctrl/Cmd + O', 'Open project'],
  ['Ctrl/Cmd + N', 'New project'],
  ['Ctrl/Cmd + Z', 'Undo'],
  ['Ctrl/Cmd + Shift + Z / Ctrl + Y', 'Redo'],
  ['Ctrl/Cmd + A', 'Select all (playlist or piano roll)'],
  ['Ctrl/Cmd + D', 'Duplicate selected clips'],
  ['Delete / Backspace', 'Delete selection'],
  ['M', 'Mute selected channel (or mixer insert)'],
  ['S', 'Solo selected channel (or mixer insert)'],
  ['F5', 'Focus playlist'],
  ['F6 / F7 / F9', 'Channel rack / piano roll / mixer'],
  ['F10 / F11 / F12', 'Drum machine / vocal tuner / analyzer'],
  ['Alt + F8', 'Toggle browser'],
  ['Typing keyboard', 'Z–M and Q–U rows play notes (toggle in toolbar)'],
  ['Ctrl + wheel', 'Zoom (playlist / piano roll)'],
  ['Alt + C', 'Capture: last played phrase → new pattern'],
  ['Alt + I', 'Idea dice'],
  ['Alt + N', 'Song notes & teleprompter'],
  ['Alt + drag', 'Bypass snap'],
  ['Shift + drag', 'Copy clips / notes'],
  ['Right click', 'Delete note (piano roll) / context menus'],
];

export function ShortcutsWindow({ w }: { w: FloatingWindow }) {
  return (
    <Window w={w} title={<b>Keyboard shortcuts</b>} width={440}>
      <div className="dlg">
        <div className="dlg-shortcuts">
          {SHORTCUTS.map(([k, v]) => (
            <div key={k} className="dlg-sc"><span className="kbd">{k}</span><span>{v}</span></div>
          ))}
        </div>
      </div>
    </Window>
  );
}

export function AboutWindow({ w }: { w: FloatingWindow }) {
  return (
    <Window w={w} title={<b>About</b>} width={380}>
      <div className="dlg about">
        <img src={logo} alt="" />
        <h2>Goofy Studio <span className="crumb">v{APP_VERSION}</span></h2>
        <p>A browser based digital audio workstation built on the Web Audio API and AudioWorklets.</p>
        <p className="crumb">All instruments and samples are synthesised in real time – no third-party sample content.</p>
      </div>
    </Window>
  );
}

export function ExportWindow({ w }: { w: FloatingWindow }) {
  const s = useUI();
  const loop = useProject((x) => x.project.loop);
  const project = useProject((x) => x.project);
  const [range, setRange] = useState<'auto' | 'song' | 'loop'>('song');
  const [busy, setBusy] = useState<string | null>(null);
  const [progress, setProgress] = useState(0);
  const stemCount = stemSources(project).length + 1;
  const run = async (label: string, fn: (onProgress: (p: number) => void) => Promise<void>) => {
    setBusy(label);
    setProgress(0);
    try {
      await fn(setProgress);
    } finally {
      setBusy(null);
    }
  };
  const common = { range, onProgress: setProgress } as const;
  return (
    <Window w={w} title={<><Icon name="export" size={12} /> <b>Export</b></>} width={460}>
      <div className="dlg export">
        <section>
          <h3>Audio format</h3>
          <div className="exp-grid">
            <label>Quality
              <select className="input" value={s.exportBits} onChange={(e) => setUI({ exportBits: Number(e.target.value) as 16 | 24 | 32 })}>
                <option value={16}>WAV 16 bit (CD, dithered)</option>
                <option value={24}>WAV 24 bit (studio)</option>
                <option value={32}>WAV 32 bit float (no clipping)</option>
              </select>
            </label>
            <label>Sample rate
              <select className="input" value={s.exportRate} onChange={(e) => setUI({ exportRate: Number(e.target.value) })}>
                {[44100, 48000, 96000].map((r) => <option key={r} value={r}>{r / 1000} kHz</option>)}
              </select>
            </label>
            <label>Range
              <select className="input" value={range} onChange={(e) => setRange(e.target.value as typeof range)}>
                <option value="song">Whole song</option>
                <option value="loop" disabled={!(loop.end > loop.start)}>Loop region only</option>
              </select>
            </label>
            <label>Tail (reverb / echo)
              <select className="input" value={s.exportTail} onChange={(e) => setUI({ exportTail: Number(e.target.value) })}>
                {[0.5, 1, 2.5, 5, 10].map((t) => <option key={t} value={t}>{t} s</option>)}
              </select>
            </label>
          </div>
          <label className="dlg-check"><input type="checkbox" checked={s.exportNormalize} onChange={(e) => setUI({ exportNormalize: e.target.checked })} /> Normalise (loudest peak at −1 dBFS)</label>
        </section>
        <section>
          <h3>Export</h3>
          <div className="exp-actions">
            <button className="btn primary" disabled={!!busy} onClick={() => void run('Song', (p) => exportWav('song', { ...common, onProgress: p }))}><Icon name="wave" size={12} /> Song as WAV</button>
            <button className="btn" disabled={!!busy} onClick={() => void run('Pattern', (p) => exportWav('pattern', { onProgress: p }))}><Icon name="rack" size={12} /> Current pattern as WAV</button>
            <button className="btn" disabled={!!busy} title="Every mixer insert as its own WAV file (as heard in the mix) + the full mix, in one ZIP" onClick={() => void run('Stems', (p) => exportStems({ ...common, onProgress: p }))}><Icon name="mixer" size={12} /> Stems (ZIP, {stemCount} files)</button>
          </div>
          <div className="exp-actions">
            <button className="btn" disabled={!!busy} onClick={exportSongMidi}><Icon name="midi" size={12} /> Song as MIDI</button>
            <button className="btn" disabled={!!busy} onClick={() => { const id = ui().selectedPatternId; if (id) exportPatternMidi(id); }}><Icon name="midi" size={12} /> Pattern as MIDI</button>
          </div>
        </section>
        {busy && (
          <div className="exp-progress">
            <div className="exp-bar"><div style={{ width: `${Math.round(progress * 100)}%` }} /></div>
            <span>Rendering {busy.toLowerCase()}… {Math.round(progress * 100)} %</span>
          </div>
        )}
        <p className="crumb">Rendering happens on your computer, faster than real time. Automation, effects and all instruments are included.</p>
      </div>
    </Window>
  );
}
