import { setUI, useUI, type DockTab } from '../state/ui';
import { Icon } from './components/Icon';
import { ChannelRack } from './channelrack/ChannelRack';
import { PianoRoll } from './pianoroll/PianoRoll';
import { Mixer } from './mixer/Mixer';
import { DrumMachine } from './drums/DrumMachine';
import { VocalPanel } from './vocal/VocalPanel';
import { AnalyzerPanel } from './visualizers/AnalyzerPanel';

const TABS: { id: DockTab; label: string; icon: string; key: string }[] = [
  { id: 'rack', label: 'Channel Rack', icon: 'rack', key: 'F6' },
  { id: 'piano', label: 'Piano Roll', icon: 'piano', key: 'F7' },
  { id: 'mixer', label: 'Mixer', icon: 'mixer', key: 'F9' },
  { id: 'drums', label: 'Drum Machine', icon: 'drums', key: 'F10' },
  { id: 'vocal', label: 'Record & Tune', icon: 'mic', key: 'F11' },
  { id: 'analyzer', label: 'Analyzer', icon: 'spectrum', key: 'F12' },
];

export function Dock() {
  const tab = useUI((s) => s.dockTab);
  return (
    <div className="dock">
      <div className="dock-tabs">
        {TABS.map((t) => (
          <button key={t.id} className={`dock-tab ${tab === t.id ? 'active' : ''}`} onClick={() => setUI({ dockTab: t.id })} title={`${t.label} (${t.key})`}>
            <Icon name={t.icon} size={12} /> {t.label}
          </button>
        ))}
        <span className="spacer" />
        <button className="btn ghost icon small" title="Hide panel" onClick={() => setUI({ dockOpen: false })}><Icon name="chevronDown" size={12} /></button>
      </div>
      <div className="dock-body" key={tab}>
        {tab === 'rack' && <ChannelRack />}
        {tab === 'piano' && <PianoRoll />}
        {tab === 'mixer' && <Mixer />}
        {tab === 'drums' && <DrumMachine />}
        {tab === 'vocal' && <VocalPanel />}
        {tab === 'analyzer' && <AnalyzerPanel />}
      </div>
    </div>
  );
}
