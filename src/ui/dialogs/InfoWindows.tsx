import logo from '../../assets/logo.png';
import type { FloatingWindow } from '../../state/ui';
import { Window } from '../components/Window';
import { exportWav } from '../commands';
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
        <h2>Goofy Studio <span className="crumb">v1.0</span></h2>
        <p>A browser based digital audio workstation built on the Web Audio API and AudioWorklets.</p>
        <p className="crumb">All instruments and samples are synthesised in real time – no third-party sample content.</p>
      </div>
    </Window>
  );
}

export function ExportWindow({ w }: { w: FloatingWindow }) {
  return (
    <Window w={w} title={<b>Export</b>} width={340}>
      <div className="dlg">
        <button className="btn" onClick={() => void exportWav('song')}>Export song as WAV</button>
        <button className="btn" onClick={() => void exportWav('pattern')}>Export current pattern as WAV</button>
      </div>
    </Window>
  );
}
