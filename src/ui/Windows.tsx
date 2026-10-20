import { closeWindow, useUI, type FloatingWindow } from '../state/ui';
import { ErrorBoundary } from './components/ErrorBoundary';
import { Window as FloatWindow } from './components/Window';
import { EffectWindow } from './effects/EffectWindow';
import { ClipSettingsWindow, TrackSettingsWindow } from './settings/SettingsWindows';
import { SampleEditorWindow } from './library/SampleEditor';
import { StemSplitterWindow } from './stems/StemSplitter';
import { AdminWindow } from './admin/AdminWindow';
import { ChannelWindow } from './instruments/ChannelWindow';
import { DreamChordWindow } from './dreamchord/DreamChordWindow';
import { AutomationWindow } from './automation/AutomationWindow';
import { HistoryWindow } from './dialogs/HistoryWindow';
import { SettingsWindow } from './dialogs/SettingsWindow';
import { ProjectsWindow } from './dialogs/ProjectsWindow';
import { AboutWindow, ExportWindow, ShortcutsWindow } from './dialogs/InfoWindows';

export function Windows() {
  const windows = useUI((s) => s.windows);
  return (
    <>
      {windows.map((w) => (
        <ErrorBoundary key={w.id} name={windowName(w)} onClose={() => closeWindow(w.id)} frame={(c) => <FloatWindow w={w} title={<b>{windowName(w)}</b>} width={420}>{c}</FloatWindow>}>
          {renderWindow(w)}
        </ErrorBoundary>
      ))}
    </>
  );
}

function windowName(w: FloatingWindow) {
  const n: Record<string, string> = { dreamchord: 'DreamChord', channel: 'Instrument', effect: 'Effect', clipEffect: 'Effect', settings: 'Settings' };
  return n[w.win.kind] ?? 'Window';
}

function renderWindow(w: FloatingWindow) {
        const k = w.win;
        switch (k.kind) {
          case 'effect': return <EffectWindow key={w.id} w={w} target={{ kind: 'mixer', id: k.trackId }} effectId={k.effectId} />;
          case 'clipEffect': return <EffectWindow key={w.id} w={w} target={{ kind: 'clip', id: k.clipId }} effectId={k.effectId} />;
          case 'clip': return <ClipSettingsWindow key={w.id} w={w} clipId={k.clipId} />;
          case 'track': return <TrackSettingsWindow key={w.id} w={w} trackId={k.trackId} />;
          case 'sampleEditor': return <SampleEditorWindow key={w.id} w={w} assetId={k.assetId} />;
          case 'stems': return <StemSplitterWindow key={w.id} w={w} />;
          case 'admin': return <AdminWindow key={w.id} w={w} />;
          case 'channel': return <ChannelWindow key={w.id} w={w} channelId={k.channelId} />;
          case 'dreamchord': return <DreamChordWindow key={w.id} w={w} channelId={k.channelId} />;
          case 'automation': return <AutomationWindow key={w.id} w={w} clipId={k.clipId} />;
          case 'history': return <HistoryWindow key={w.id} w={w} />;
          case 'settings': return <SettingsWindow key={w.id} w={w} />;
          case 'projects': return <ProjectsWindow key={w.id} w={w} />;
          case 'shortcuts': return <ShortcutsWindow key={w.id} w={w} />;
          case 'about': return <AboutWindow key={w.id} w={w} />;
          case 'export': return <ExportWindow key={w.id} w={w} />;
          default: return null;
        }
}
