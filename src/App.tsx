import { useEffect, useState } from 'react';
import { engine } from './audio/engine';
import { getProject, installGestureSafety, useProject } from './state/store';
import { openWindow, setUI, toast, ui, useUI } from './state/ui';
import { APP_VERSION, compareVersions } from './admin/feed';
import { installSettingsSync } from './ui/settingsSync';
import { installAutomationPlayback } from './ui/automationPlayback';
import { installAudioStability } from './ui/audioStability';
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
import { AskTextHost } from './ui/dialogs/askText';
import { StartupNotices } from './ui/admin/StartupNotices';
import { loadLibrary } from './ui/library/libraryStore';
import { Icon } from './ui/components/Icon';
import { ErrorBoundary } from './ui/components/ErrorBoundary';
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
  const showHints = useUI((s) => s.showHints);
  const [running, setRunning] = useState(engine.running);
  const [sr, setSr] = useState(engine.initialized ? engine.ctx.sampleRate : 0);
  useEffect(() => {
    const tick = () => { setRunning(engine.running); setSr(engine.initialized ? engine.ctx.sampleRate : 0); };
    const id = setInterval(tick, 800);
    const off = engine.onChange(tick);
    return () => { clearInterval(id); off(); };
  }, []);
  return (
    <div className="statusbar">
      <span className="sb-project"><b>{name}</b>{dirty ? ' •' : ''}</span>
      <span>{lastAction ? `Last action: ${lastAction}` : 'Ready'}</span>
      <span className="spacer" />
      {showHints && <span className="sb-hint">Space play · R record · Ctrl+Z undo · Ctrl+S save · double-click a channel to edit</span>}
      <span className="spacer" />
      <button className={`sb-audio ${running ? 'on' : ''}`} onClick={() => void engine.resume()} title={running ? 'Audio engine running' : 'Click to start audio'}>
        <span className="sb-dot" />
        {running ? `Audio ${(sr / 1000).toFixed(1)} kHz` : 'Audio suspended – click to enable'}
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
      <ErrorBoundary name="Toolbar" inline><Toolbar /></ErrorBoundary>
      <div className="app-body">
        {browserOpen && (
          <>
            <div className="app-browser" style={{ width: browserWidth }}><ErrorBoundary name="Browser"><Browser /></ErrorBoundary></div>
            <Splitter dir="x" onDrag={(d) => setUI({ browserWidth: clamp(ui().browserWidth + d, 160, 480) })} />
          </>
        )}
        <div className="app-center">
          <div className="app-playlist"><ErrorBoundary name="Playlist"><Playlist /></ErrorBoundary></div>
          {dockOpen ? (
            <>
              <Splitter dir="y" onDrag={(d) => setUI({ dockHeight: clamp(ui().dockHeight - d, 140, window.innerHeight - 200) })} />
              <div className="app-dock" style={{ height: dockHeight }}><ErrorBoundary name="Panel"><Dock /></ErrorBoundary></div>
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
      const s0 = ui();
      await engine.init({ latencyHint: s0.latencyHint, latencyMs: s0.latencyMs, sampleRate: s0.sampleRate }, report);
      report('Restoring last project…', 0.75);
      await restoreLastProject();
      report('Connecting mixer & instruments…', 0.85);
      engine.attach(getProject);
      engine.transport.patternId = ui().selectedPatternId;
      engine.transport.metronome = ui().metronome;
      useProject.subscribe((s, prev) => { if (s.project !== prev.project) engine.sync(s.project); });
      useUI.subscribe((s, prev) => { if (s.selectedPatternId !== prev.selectedPatternId && engine.transport) engine.transport.patternId = s.selectedPatternId; });
      engine.onChange(syncTransportUI);
      engine.onEvent((e) => {
        if (e.type === 'protect') {
          toast(e.reason === 'runaway'
            ? 'Extremely loud signal stopped to protect your ears – effects were reset. Check feedback / delay settings.'
            : 'A broken audio signal was blocked and the mixer was rebuilt.', 'error');
        } else if (e.type === 'device') {
          toast(e.text, e.kind, true);
        } else if (e.type === 'reset' && e.hard) {
          toast('Audio engine restarted', 'info', true);
        }
      });
      engine.sync(getProject());
      installRecordingHandler();
      installSettingsSync();
      installGestureSafety();
      installAutomationPlayback();
      installAudioStability();
      startAutosave();
      installShortcuts();
      void loadLibrary();
      if (ui().outputDeviceId) void engine.setOutputDevice(ui().outputDeviceId);
      report('Building interface…', 0.95);
      // give the splash animation time to breathe (Settings → Design → Start-up animation)
      const elapsed = performance.now() - t0;
      if (ui().showSplash && !ui().reduceMotion) await new Promise((r) => setTimeout(r, Math.max(0, 1900 - elapsed)));
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
      {!splashGone && <Splash progress={progress} label={label} done={ready} quick={!ui().showSplash || ui().reduceMotion} onGone={() => setSplashGone(true)} />}
      {ready && splashGone && !setupDone && <SetupWizard onDone={() => undefined} />}
      {ready && splashGone && setupDone && <StartupNotices />}
      {ready && splashGone && setupDone && <WhatsNewOnce />}
      <ContextMenuHost />
      <AskTextHost />
      <Toast />
    </>
  );
}

/** Opens the "What's new" tour once after an update (not on top of the setup wizard). */
function WhatsNewOnce() {
  useEffect(() => {
    const seen = ui().seenWhatsNew;
    // only after an update (not after going back to an older version)
    if (seen && compareVersions(seen, APP_VERSION) >= 0) return;
    const t = setTimeout(() => openWindow({ kind: 'whatsnew' }), 700);
    return () => clearTimeout(t);
  }, []);
  return null;
}
