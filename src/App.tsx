import { useEffect, useState } from 'react';
import { engine } from './audio/engine';
import { getProject, useProject } from './state/store';
import { setUI, ui, useUI } from './state/ui';
import { applyTheme, resolveTheme } from './styles/themes';
import { installRecordingHandler, restoreLastProject, startAutosave } from './project/manager';
import { Splash } from './ui/splash/Splash';
import { SetupWizard } from './ui/setup/SetupWizard';
import { Toolbar, useAudioUnlockHint } from './ui/toolbar/Toolbar';
import { Browser } from './ui/browser/Browser';
import { Playlist } from './ui/playlist/Playlist';
import { Dock } from './ui/Dock';
import { Windows } from './ui/Windows';
import { Splitter } from './ui/components/Splitter';
import { ContextMenuHost } from './ui/components/ContextMenu';
import { Icon } from './ui/components/Icon';
import { installShortcuts } from './ui/shortcuts';
import { syncTransportUI, showDock } from './ui/commands';
import { clamp } from './state/utils';
import './styles/base.css';
import './styles/app.css';

function useTheme() {
  const theme = useUI((s) => s.theme);
  const accent = useUI((s) => s.accent);
  const custom = useUI((s) => s.custom);
  useEffect(() => {
    applyTheme(resolveTheme(theme, custom, accent));
  }, [theme, accent, custom]);
}

function Toast() {
  const t = useUI((s) => s.toast);
  if (!t) return null;
  return <div key={t.id} className={`toast ${t.kind}`}>{t.text}</div>;
}

function StatusBar() {
  const lastAction = useProject((s) => s.lastAction);
  const name = useProject((s) => s.project.name);
  const dirty = useProject((s) => s.dirty);
  const [running, setRunning] = useState(engine.running);
  useEffect(() => {
    const id = setInterval(() => setRunning(engine.running), 800);
    return () => clearInterval(id);
  }, []);
  return (
    <div className="statusbar">
      <span className="sb-project"><b>{name}</b>{dirty ? ' •' : ''}</span>
      <span>{lastAction ? `Last action: ${lastAction}` : 'Ready'}</span>
      <span className="spacer" />
      <span className="sb-hint">Space play · R record · Ctrl+Z undo · Ctrl+S save · double-click a channel to edit</span>
      <span className="spacer" />
      <button className={`sb-audio ${running ? 'on' : ''}`} onClick={() => void engine.resume()} title={running ? 'Audio engine running' : 'Click to start audio'}>
        <span className="sb-dot" />
        {running ? `Audio ${(engine.ctx.sampleRate / 1000).toFixed(1)} kHz` : 'Audio suspended – click to enable'}
      </button>
    </div>
  );
}

function Main() {
  const browserOpen = useUI((s) => s.browserOpen);
  const browserWidth = useUI((s) => s.browserWidth);
  const dockOpen = useUI((s) => s.dockOpen);
  const dockHeight = useUI((s) => s.dockHeight);
  useAudioUnlockHint();
  return (
    <div className="app">
      <Toolbar />
      <div className="app-body">
        {browserOpen && (
          <>
            <div className="app-browser" style={{ width: browserWidth }}><Browser /></div>
            <Splitter dir="x" onDrag={(d) => setUI({ browserWidth: clamp(ui().browserWidth + d, 160, 480) })} />
          </>
        )}
        <div className="app-center">
          <div className="app-playlist"><Playlist /></div>
          {dockOpen ? (
            <>
              <Splitter dir="y" onDrag={(d) => setUI({ dockHeight: clamp(ui().dockHeight - d, 140, window.innerHeight - 200) })} />
              <div className="app-dock" style={{ height: dockHeight }}><Dock /></div>
            </>
          ) : (
            <div className="dock-collapsed">
              {(['rack', 'piano', 'mixer', 'drums', 'vocal', 'analyzer'] as const).map((t) => (
                <button key={t} className="dock-tab" onClick={() => showDock(t)}>
                  <Icon name={{ rack: 'rack', piano: 'piano', mixer: 'mixer', drums: 'drums', vocal: 'mic', analyzer: 'spectrum' }[t]} size={12} />
                  {{ rack: 'Channel Rack', piano: 'Piano Roll', mixer: 'Mixer', drums: 'Drum Machine', vocal: 'Record & Tune', analyzer: 'Analyzer' }[t]}
                </button>
              ))}
            </div>
          )}
        </div>
      </div>
      <StatusBar />
      <Windows />
    </div>
  );
}

type Report = (label: string, p: number) => void;
let reporter: Report = () => undefined;
let bootPromise: Promise<void> | null = null;

/** Boots the engine and wires the stores exactly once (safe under StrictMode). */
function boot(): Promise<void> {
  if (!bootPromise) {
    bootPromise = (async () => {
      const report: Report = (l, p) => reporter(l, p);
      const t0 = performance.now();
      await engine.init(ui().latencyHint, report);
      report('Restoring last project…', 0.75);
      await restoreLastProject();
      report('Connecting mixer & instruments…', 0.85);
      engine.attach(getProject);
      engine.transport.patternId = ui().selectedPatternId;
      engine.transport.metronome = ui().metronome;
      useProject.subscribe((s, prev) => { if (s.project !== prev.project) engine.sync(s.project); });
      useUI.subscribe((s, prev) => { if (s.selectedPatternId !== prev.selectedPatternId && engine.transport) engine.transport.patternId = s.selectedPatternId; });
      engine.onChange(syncTransportUI);
      engine.sync(getProject());
      installRecordingHandler();
      startAutosave();
      installShortcuts();
      if (ui().outputDeviceId) void engine.setOutputDevice(ui().outputDeviceId);
      report('Building interface…', 0.95);
      // give the splash animation time to breathe
      const elapsed = performance.now() - t0;
      await new Promise((r) => setTimeout(r, Math.max(0, 1900 - elapsed)));
      report('Ready', 1);
    })();
  }
  return bootPromise;
}

export function App() {
  useTheme();
  const [progress, setProgress] = useState(0);
  const [label, setLabel] = useState('Starting…');
  const [ready, setReady] = useState(false);
  const [splashGone, setSplashGone] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const setupDone = useUI((s) => s.setupDone);

  useEffect(() => {
    let cancelled = false;
    reporter = (l, p) => { if (!cancelled) { setLabel(l); setProgress(p); } };
    boot().then(
      () => { if (!cancelled) setReady(true); },
      (err) => {
        console.error(err);
        if (!cancelled) setError(err instanceof Error ? err.message : String(err));
      },
    );
    const unlock = () => void engine.resume();
    window.addEventListener('pointerdown', unlock, true);
    window.addEventListener('keydown', unlock, true);
    return () => {
      cancelled = true;
      window.removeEventListener('pointerdown', unlock, true);
      window.removeEventListener('keydown', unlock, true);
    };
  }, []);

  if (error) {
    return (
      <div className="fatal">
        <h2>Goofy Studio could not start</h2>
        <p>{error}</p>
        <p className="crumb">Please use a current version of Chrome, Edge, Firefox or Safari.</p>
      </div>
    );
  }

  return (
    <>
      {ready && <div className={`app-shell ${splashGone ? 'entered' : ''}`}><Main /></div>}
      {!splashGone && <Splash progress={progress} label={label} done={ready} onGone={() => setSplashGone(true)} />}
      {ready && splashGone && !setupDone && <SetupWizard onDone={() => undefined} />}
      <ContextMenuHost />
      <Toast />
    </>
  );
}
