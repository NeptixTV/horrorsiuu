// "What's new" tour: shown once after an update (Settings → General can show
// it again via Help → What's new). Every entry has a button that opens the
// feature right away.
import { useEffect } from 'react';
import { APP_VERSION } from '../../admin/feed';
import { closeWindow, openWindow, setUI, type FloatingWindow } from '../../state/ui';
import { addDreamChord, showDock } from '../commands';
import { openNotes } from '../notes/NotesWindow';
import { openSettingsPage } from './SettingsWindow';
import { Window } from '../components/Window';
import { Icon } from '../components/Icon';
import './dialogs.css';

interface Item {
  icon: string;
  title: string;
  text: string;
  action?: [string, () => void];
  mine?: boolean;
}

const ITEMS: Item[] = [
  { icon: 'shield', title: 'Rock-solid audio', text: 'Fixed the crackling, sudden distortion and dead headphones. A protection stage now catches extreme peaks and broken signals, and the engine repairs itself if a device disappears.', action: ['Diagnostics', () => openSettingsPage('diagnostics')] },
  { icon: 'dream', title: 'DreamChord', text: 'A chord instrument with 7 pads and a joystick: dreamy, warm, nostalgic sounds with arpeggiator, strum and hold. Everything you play can be recorded.', action: ['Try DreamChord', () => addDreamChord('Ocean Dream')] },
  { icon: 'dice', title: 'Idea dice', text: 'Out of ideas? Roll chords, bass, melody and a beat that fit your key – lock what you like, re-roll the rest.', action: ['Roll an idea', () => openWindow({ kind: 'ideas' })], mine: true },
  { icon: 'capture', title: 'Capture', text: 'The studio quietly remembers what you played in the last 3 minutes – even without recording. One click (or Alt+C) turns it into a pattern.', mine: true },
  { icon: 'notes', title: 'Song notes & teleprompter', text: 'Write lyrics inside the project. While recording they scroll in big letters – and jump to [Chorus] when the playhead passes a marker called Chorus.', action: ['Open notes', () => openNotes()], mine: true },
  { icon: 'automation', title: 'Automation', text: 'Volume, pan and every effect knob can change over the song. Mixer → right-click a strip → Automate, or the Automate button in any effect.' },
  { icon: 'wand', title: 'Piano roll tools', text: 'Chord stamps, scale lock, strum, arpeggiate, humanize, legato, reverse, copy & paste, arrow-key editing.', action: ['Open piano roll', () => showDock('piano')] },
  { icon: 'midi', title: 'Export & MIDI', text: 'WAV in 16 / 24 / 32 bit, stems as one ZIP, MIDI export and import.', action: ['Export…', () => openWindow({ kind: 'export' })] },
  { icon: 'sparkle', title: 'New effects', text: 'Tape Machine (wow, flutter, saturation), Stereo Width with goniometer and a Noise Gate – plus your own presets for every effect and synth.' },
  { icon: 'history', title: 'Safer projects', text: 'Automatic backups, a project doctor that repairs damaged files, and an undo history you can jump around in.', action: ['Backups', () => openSettingsPage('projects')] },
  { icon: 'settings', title: 'Settings center', text: 'Audio devices, latency, headphone test, themes, scaling, autosave, notifications and privacy – all in one place.', action: ['Settings', () => openSettingsPage('audio')] },
];

export function WhatsNewWindow({ w }: { w: FloatingWindow }) {
  // seen as soon as it was shown once
  useEffect(() => { setUI({ seenWhatsNew: APP_VERSION }); }, []);
  const run = (fn: () => void) => { closeWindow(w.id); fn(); };
  return (
    <Window w={w} width={520} title={<><Icon name="sparkle" size={12} /> <b>What's new in Goofy Studio {APP_VERSION}</b></>}>
      <div className="dlg whatsnew">
        <div className="wn-list">
          {ITEMS.map((it) => (
            <div key={it.title} className="wn-item">
              <span className="wn-icon"><Icon name={it.icon} size={16} /></span>
              <div className="wn-main">
                <div className="wn-title">{it.title}{it.mine && <span className="wn-tag" title="Invented for this version">new idea</span>}</div>
                <div className="wn-text">{it.text}</div>
              </div>
              {it.action && <button className="btn small" onClick={() => run(it.action![1])}>{it.action[0]}</button>}
            </div>
          ))}
        </div>
        <div className="dlg-row" style={{ justifyContent: 'flex-end' }}>
          <span className="crumb" style={{ marginRight: 'auto', alignSelf: 'center' }}>Help → What's new shows this again.</span>
          <button className="btn primary" onClick={() => closeWindow(w.id)}>Let's go</button>
        </div>
      </div>
    </Window>
  );
}
