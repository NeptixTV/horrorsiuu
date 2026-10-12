import { useUI } from '../state/ui';
import { EffectWindow } from './effects/EffectWindow';
import { ClipSettingsWindow, TrackSettingsWindow } from './settings/SettingsWindows';
import { SampleEditorWindow } from './library/SampleEditor';
import { StemSplitterWindow } from './stems/StemSplitter';
import { AdminWindow } from './admin/AdminWindow';
import { ChannelWindow } from './instruments/ChannelWindow';
import { SettingsWindow } from './dialogs/SettingsWindow';
import { ProjectsWindow } from './dialogs/ProjectsWindow';
import { AboutWindow, ExportWindow, ShortcutsWindow } from './dialogs/InfoWindows';

export function Windows() {
  const windows = useUI((s) => s.windows);
  return (
    <>
      {windows.map((w) => {
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
          case 'settings': return <SettingsWindow key={w.id} w={w} />;
          case 'projects': return <ProjectsWindow key={w.id} w={w} />;
          case 'shortcuts': return <ShortcutsWindow key={w.id} w={w} />;
          case 'about': return <AboutWindow key={w.id} w={w} />;
          case 'export': return <ExportWindow key={w.id} w={w} />;
          default: return null;
        }
      })}
    </>
  );
}
