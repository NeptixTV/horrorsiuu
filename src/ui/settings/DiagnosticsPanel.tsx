// Settings → Diagnostics: live engine status, counters, event log and the
// repair tools (restart audio, rebuild mixer, panic, report).
import { useEffect, useState, useSyncExternalStore } from 'react';
import { engine } from '../../audio/engine';
import { diagnostics } from '../../audio/diagnostics';
import { APP_VERSION } from '../../admin/feed';
import { toast } from '../../state/ui';
import { Icon } from '../components/Icon';
import { Row, Section, Stat } from './controls';

function useDiag() {
  return useSyncExternalStore((cb) => diagnostics.subscribe(cb), () => diagnostics.version);
}

const COUNTER_LABELS: Record<keyof typeof diagnostics.counters, string> = {
  lateCallbacks: 'Scheduler running late',
  skippedEvents: 'Skipped events',
  scheduleErrors: 'Scheduling errors',
  nanDetections: 'Broken signals blocked',
  runaways: 'Runaway signals stopped',
  limiterHits: 'Peaks caught by the limiter',
  graphResets: 'Mixer rebuilds',
  engineRestarts: 'Audio engine restarts',
  deviceChanges: 'Device changes',
  repairedProjects: 'Projects repaired',
};

export function DiagnosticsSection() {
  useDiag();
  const [status, setStatus] = useState(engine.status());
  const [guard, setGuard] = useState(engine.guardStats);
  useEffect(() => {
    const id = setInterval(() => { setStatus(engine.status()); setGuard({ ...engine.guardStats }); }, 400);
    return () => clearInterval(id);
  }, []);
  const c = diagnostics.counters;
  const healthy = status.state === 'running' && c.scheduleErrors === 0 && c.nanDetections === 0 && c.runaways === 0;

  const copyReport = async () => {
    const text = diagnostics.report({
      version: APP_VERSION,
      platform: navigator.platform || 'unknown',
      state: status.state,
      sampleRate: status.sampleRate,
      baseLatencyMs: status.baseLatencyMs.toFixed(1),
      outputLatencyMs: status.outputLatencyMs.toFixed(1),
      worklets: status.worklets,
      guard: status.guard,
      inputOpen: status.inputOpen,
    });
    try {
      await navigator.clipboard.writeText(text);
      toast('Report copied – no device names or file paths; channel names from your project can appear in the events', 'ok');
    } catch {
      toast('Could not access the clipboard', 'error');
    }
  };

  return (
    <>
      <Section title="Status">
        <div className={`sc-health ${healthy ? 'ok' : 'warn'}`}>
          <Icon name={healthy ? 'shield' : 'info'} size={16} />
          {healthy ? 'Audio is running normally.' : status.state !== 'running' ? `Audio is ${status.state} – click anywhere in the app or press “Restart audio engine”.` : 'Problems were detected and repaired automatically – see the log below.'}
        </div>
        <div className="sc-stats">
          <Stat label="Engine" value={status.state} warn={status.state !== 'running'} />
          <Stat label="Sample rate" value={`${status.sampleRate} Hz`} />
          <Stat label="Base latency" value={`${status.baseLatencyMs.toFixed(1)} ms`} />
          <Stat label="Output latency" value={`${status.outputLatencyMs.toFixed(1)} ms`} />
          <Stat label="Voices" value={status.voices} />
          <Stat label="Mixer strips / instruments" value={`${status.strips} / ${status.channels}`} />
          <Stat label="AudioWorklet" value={status.worklets ? 'yes' : 'no'} warn={!status.worklets} />
          <Stat label="Output guard" value={status.guard} />
          <Stat label="Limiter now" value={guard.reductionDb < -0.1 ? `${guard.reductionDb.toFixed(1)} dB` : 'idle'} warn={guard.reductionDb < -3} />
          <Stat label="Output muted" value={guard.muted ? 'yes (protection)' : 'no'} warn={guard.muted} />
          <Stat label="Microphone" value={status.inputOpen ? 'open' : 'closed'} />
          <Stat label="Worst scheduler delay" value={`${diagnostics.worstLateMs.toFixed(0)} ms`} warn={diagnostics.worstLateMs > 100} />
        </div>
      </Section>
      <Section title="Repair tools" hint="Use these if the sound is stuck, distorted or silent. Your project is not changed.">
        <Row label="Restart audio engine" desc="Closes and re-opens the audio device and rebuilds everything. Fixes most device problems (headphones changed, sound gone).">
          <button className="btn primary" onClick={() => void engine.restart('restart button').then(() => toast('Audio engine restarted', 'ok'))}>Restart</button>
        </Row>
        <Row label="Rebuild mixer" desc="Recreates all mixer and instrument nodes (clears stuck effects and echoes).">
          <button className="btn" onClick={() => engine.resetGraph('rebuild button')}>Rebuild</button>
        </Row>
        <Row label="Panic" desc="Stops all sounding notes, echoes and reverb tails immediately.">
          <button className="btn danger" onClick={() => engine.panic()}>Panic</button>
        </Row>
      </Section>
      <Section title="Counters">
        <div className="sc-stats">
          {(Object.keys(COUNTER_LABELS) as (keyof typeof c)[]).map((k) => <Stat key={k} label={COUNTER_LABELS[k]} value={c[k]} warn={(k === 'scheduleErrors' || k === 'nanDetections' || k === 'runaways') && c[k] > 0} />)}
        </div>
      </Section>
      <Section title="Event log" hint="Stays on this computer. “Copy report” puts a summary on the clipboard – without device names or file names – so you can send it along with a bug report.">
        <div className="sc-inline">
          <button className="btn small" onClick={() => void copyReport()}><Icon name="copy" size={11} /> Copy report</button>
          <button className="btn small ghost" onClick={() => diagnostics.clear()}>Clear</button>
        </div>
        <div className="sc-log">
          {diagnostics.entries.length === 0 && <div className="sc-log-empty">No events yet.</div>}
          {[...diagnostics.entries].reverse().slice(0, 120).map((e, i) => (
            <div key={`${e.time}-${i}`} className={`sc-log-row ${e.level}`}>
              <span className="sc-log-time">{new Date(e.time).toLocaleTimeString()}</span>
              <span className="sc-log-code">{e.code}</span>
              <span className="sc-log-text">{e.text}</span>
            </div>
          ))}
        </div>
      </Section>
    </>
  );
}
