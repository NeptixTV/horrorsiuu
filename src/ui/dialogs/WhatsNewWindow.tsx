// "What's new" tour: shown once after an update (Settings → General can show
// it again via Help → What's new). Every entry has a button that opens the
// feature right away.
import { useEffect } from 'react';
import { APP_VERSION } from '../../admin/feed';
import { closeWindow, openWindow, setUI, type FloatingWindow } from '../../state/ui';
import { openProjectsWindow, showDock } from '../commands';
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
  { icon: 'trash', title: 'Trash for projects', text: 'Deleted a song by mistake? Deleted projects now go to the trash and can be restored for 30 days. Projects you deleted before this update are back in there too, as long as their automatic backup still exists.', action: ['Open the trash', () => openProjectsWindow('trash')], mine: true },
  { icon: 'bolt', title: 'New Beat in one click', text: 'Pick a genre – Trap, Drill, Boom Bap, Lo-Fi, House, Phonk or Afrobeats – and get a finished, mixed beat: drums, 808, chords, melody, intro, drop and break. Ctrl+B.', action: ['Make a beat', () => openWindow({ kind: 'beatStarter' })], mine: true },
  { icon: 'crown', title: 'Pro Sounds', text: '73 new sounds: a real upright piano with three velocity layers, strings, flute, horns, organ, bells and percussion recorded from real instruments – plus hard 808s, kicks, snares and hats made for beats. The old sounds are still there.', action: ['Open the browser', () => setUI({ browserOpen: true })] },
  { icon: 'drums', title: 'Drum kits & hi-hat rolls', text: '7 ready kits with a starter groove. Hi-hat rolls (×2 … ×8 per step) with the roll brush, Alt+click on a step or the 🎲 on a hat row. Swap a pad\'s sound with ◀ ▶.', action: ['Drum Machine', () => showDock('drums')] },
  { icon: 'upload', title: 'Your own sounds', text: 'Drag WAV / MP3 files or a whole sample-pack folder anywhere into the studio. They are trimmed and sorted into kicks, snares, 808s, loops … in My Sounds – for every project.', action: ['My Sounds', () => setUI({ browserOpen: true })], mine: true },
  { icon: 'wave', title: '808 slides', text: 'Pro 808s play mono with glide: let two notes overlap in the piano roll and the 808 slides into the next note.' },
  { icon: 'sparkle', title: 'Beat effects', text: 'Clipper, Sidechain Pump (ducks under the kick), Multiband "OTT", Punch (transient shaper) and Lo-Fi Crusher – plus one-click chains like Hard Trap Master, Drum Bus Punch and 808 Grit.' },
  { icon: 'maximize', title: 'Feels like a real program', text: 'Dark title bar with a menu bar in the desktop app, drop-down lists and tooltips in the studio\'s own style, no browser menus or page zoom, and the window remembers its size.' },
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
