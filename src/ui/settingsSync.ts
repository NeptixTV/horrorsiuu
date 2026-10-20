// Pushes the persisted settings into the parts of the app that use them
// (audio engine, transport, history, frame rate, UI scale, motion) and keeps
// them in sync when they change.
import { engine } from '../audio/engine';
import { setHistoryLimit } from '../state/store';
import { useUI, type UIState } from '../state/ui';
import { startAutosave } from '../project/manager';
import { setFrameRate } from './components/frameLoop';
import { desktop } from '../admin/desktop';

function applyScale(f: number) {
  const factor = Math.min(1.6, Math.max(0.7, f || 1));
  if (desktop?.setZoom) {
    void desktop.setZoom(factor);
    document.documentElement.style.removeProperty('zoom');
  } else {
    // browsers: CSS zoom (Chromium / Firefox ≥ 126 / Safari)
    document.documentElement.style.setProperty('zoom', String(factor));
  }
}

export function installSettingsSync() {
  const apply = (s: UIState, prev?: UIState) => {
    const changed = (...keys: (keyof UIState)[]) => !prev || keys.some((k) => s[k] !== prev[k]);
    if (changed('limiter', 'ceilingDb', 'protect', 'outputVolume')) {
      engine.applyOutput({ limiter: s.limiter, ceilingDb: s.ceilingDb, protect: s.protect, volume: s.outputVolume });
    }
    if (changed('lookahead', 'metronomeVolume')) engine.setTransportOptions({ lookahead: s.lookahead, metronomeVolume: s.metronomeVolume });
    engine.countInBars = s.countInBars;
    engine.releaseMicAfterRecording = s.releaseMic;
    engine.autoFallbackOutput = s.autoFallbackOutput;
    if (changed('historyLimit')) setHistoryLimit(s.historyLimit);
    if (changed('meterFps')) setFrameRate(s.meterFps);
    if (changed('uiScale')) applyScale(s.uiScale);
    if (changed('reduceMotion')) document.documentElement.classList.toggle('reduce-motion', s.reduceMotion);
    if (prev && changed('autosaveInterval')) startAutosave();
  };
  apply(useUI.getState());
  useUI.subscribe((s, prev) => apply(s, prev));
}
