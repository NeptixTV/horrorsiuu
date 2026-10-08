import { useUI } from '../state/ui';
import { EffectWindow } from './effects/EffectWindow';
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
          case 'effect': return <EffectWindow key={w.id} w={w} trackId={k.trackId} effectId={k.effectId} />;
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
